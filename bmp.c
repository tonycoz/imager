#include "image.h"
#include <stdarg.h>

/* possibly this belongs in a global utilities library 
   Reads from the specified "file" the specified sizes.
   The format codes match those used by perl's pack() function,
   though only a few are implemented.
   In all cases the vararg arguement is an int *.

   Returns non-zero if all of the arguments were read.
*/
static
int read_packed(io_glue *ig, char *format, ...) {
  unsigned char buf[4];
  va_list ap;
  int *p;

  va_start(ap, format);

  while (*format) {
    p = va_arg(ap, int *);

    switch (*format) {
    case 'v':
      if (ig->readcb(ig, buf, 2) == -1)
	return 0;
      *p = buf[0] + (buf[1] << 8);
      break;

    case 'V':
      if (ig->readcb(ig, buf, 4) == -1)
	return 0;
      *p = buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
      break;

    case 'C':
      if (ig->readcb(ig, buf, 1) == -1)
	return 0;
      *p = buf[0];
      break;

    case 'c':
      if (ig->readcb(ig, buf, 1) == -1)
	return 0;
      *p = (char)buf[0];
      break;
      
    default:
      m_fatal(1, "Unknown read_packed format code 0x%02x", *format);
    }
    ++format;
  }
  return 1;
}

static int
write_packed(io_glue *ig, char *format, ...) {
  unsigned char buf[4];
  va_list ap;
  int i;

  va_start(ap, format);

  while (*format) {
    i = va_arg(ap, unsigned int);

    switch (*format) {
    case 'v':
      buf[0] = i & 255;
      buf[1] = i / 256;
      if (ig->writecb(ig, buf, 2) == -1)
	return 0;
      break;

    case 'V':
      buf[0] = i & 0xFF;
      buf[1] = (i >> 8) & 0xFF;
      buf[2] = (i >> 16) & 0xFF;
      buf[3] = (i >> 24) & 0xFF;
      if (ig->writecb(ig, buf, 4) == -1)
	return 0;
      break;

    case 'C':
    case 'c':
      buf[0] = i & 0xFF;
      if (ig->writecb(ig, buf, 1) == -1)
	return 0;
      break;

    default:
      m_fatal(1, "Unknown read_packed format code 0x%02x", *format);
    }
    ++format;
  }
  va_end(ap);

  return 1;
}

#define FILEHEAD_SIZE 14
#define INFOHEAD_SIZE 40
#define BI_RGB		0
#define BI_RLE8		1
#define BI_RLE4		2
#define BI_BITFIELDS	3

static
int write_bmphead(io_glue *ig, i_img *im, int bit_count, int data_size) {
  double xres, yres;
  int got_xres, got_yres, aspect_only;
  int colors_used = 0;
  int offset = FILEHEAD_SIZE + INFOHEAD_SIZE;

  got_xres = i_tags_get_float(&im->tags, "i_xres", 0, &xres);
  got_yres = i_tags_get_float(&im->tags, "i_yres", 0, &yres);
  if (!i_tags_get_int(&im->tags, "i_aspect_only", 0,&aspect_only))
    aspect_only = 0;
  if (!got_xres) {
    if (!got_yres)
      xres = yres = 72;
    else
      xres = yres;
  }
  else {
    if (!got_yres)
      yres = xres;
  }
  if (xres <= 0 || yres <= 0)
    xres = yres = 72;
  if (aspect_only) {
    /* scale so the smaller value is 72 */
    double ratio;
    if (xres < yres) {
      ratio = 72.0 / xres;
    }
    else {
      ratio = 72.0 / yres;
    }
    xres *= ratio;
    yres *= ratio;
  }
  /* now to pels/meter */
  xres *= 100.0/2.54;
  yres *= 100.0/2.54;

  if (im->type == i_palette_type) {
    colors_used = i_colorcount(im);
    offset += 4 * colors_used;
  }

  if (!write_packed(ig, "CCVvvVVVVvvVVVVVV", 'B', 'M', data_size+offset, 
		    0, 0, offset, INFOHEAD_SIZE, im->xsize, im->ysize, 1, 
		    bit_count, BI_RGB, 0, (int)xres, (int)yres, 
		    colors_used, 0)){
    i_push_error(0, "cannot write bmp header");
    return 0;
  }
  if (im->type == i_palette_type) {
    int i;
    i_color c;

    for (i = 0; i < colors_used; ++i) {
      i_getcolors(im, i, &c, 1);
      if (im->channels >= 3) {
	if (!write_packed(ig, "CCCC", c.channel[2], c.channel[1], 
			  c.channel[0], 0)) {
	  i_push_error(0, "cannot write palette entry");
	  return 0;
	}
      }
      else {
	if (!write_packed(ig, "CCCC", c.channel[0], c.channel[0], 
			  c.channel[0], 0)) {
	  i_push_error(0, "cannot write palette entry");
	  return 0;
	}
      }
    }
  }

  return 1;
}

static int
write_1bit_data(io_glue *ig, i_img *im) {
  i_palidx *line;
  unsigned char *packed;
  int byte;
  int mask;
  unsigned char *out;
  int line_size = (im->xsize+7) / 8;
  int x, y;

  /* round up to nearest multiple of four */
  line_size = (line_size + 3) / 4 * 4;

  if (!write_bmphead(ig, im, 1, line_size * im->ysize))
    return 0;

  line = mymalloc(im->xsize + 8);
  memset(line + im->xsize, 0, 8);
  
  packed = mymalloc(line_size);
  memset(packed, 0, line_size);
  
  for (y = im->ysize-1; y >= 0; --y) {
    i_gpal(im, 0, im->xsize, y, line);
    mask = 0x80;
    byte = 0;
    out = packed;
    for (x = 0; x < im->xsize; ++x) {
      if (line[x])
	byte |= mask;
      if ((mask >>= 1) == 0) {
	*out++ = byte;
	byte = 0;
	mask = 0x80;
      }
    }
    if (mask != 0x80) {
      *out++ = byte;
    }
    if (ig->writecb(ig, packed, line_size) < 0) {
      myfree(packed);
      myfree(line);
      i_push_error(0, "writing 1 bit/pixel packed data");
      return 0;
    }
  }
  myfree(packed);
  myfree(line);

  return 1;
}

static int
write_4bit_data(io_glue *ig, i_img *im) {
  i_palidx *line;
  unsigned char *packed;
  unsigned char *out;
  int line_size = (im->xsize+1) / 2;
  int x, y;

  /* round up to nearest multiple of four */
  line_size = (line_size + 3) / 4 * 4;

  if (!write_bmphead(ig, im, 4, line_size * im->ysize))
    return 0;

  line = mymalloc(im->xsize + 2);
  memset(line + im->xsize, 0, 2);
  
  packed = mymalloc(line_size);
  memset(packed, 0, line_size);
  
  for (y = im->ysize-1; y >= 0; --y) {
    i_gpal(im, 0, im->xsize, y, line);
    out = packed;
    for (x = 0; x < im->xsize; x += 2) {
      *out++ = (line[x] << 4) + line[x+1];
    }
    if (ig->writecb(ig, packed, line_size) < 0) {
      myfree(packed);
      myfree(line);
      i_push_error(0, "writing 4 bit/pixel packed data");
      return 0;
    }
  }
  myfree(packed);
  myfree(line);

  return 1;
}

static int
write_8bit_data(io_glue *ig, i_img *im) {
  i_palidx *line;
  int line_size = im->xsize;
  int x, y;

  /* round up to nearest multiple of four */
  line_size = (line_size + 3) / 4 * 4;

  if (!write_bmphead(ig, im, 8, line_size * im->ysize))
    return 0;

  line = mymalloc(im->xsize + 4);
  memset(line + im->xsize, 0, 4);
  
  for (y = im->ysize-1; y >= 0; --y) {
    i_gpal(im, 0, im->xsize, y, line);
    if (ig->writecb(ig, line, line_size) < 0) {
      myfree(line);
      i_push_error(0, "writing 8 bit/pixel packed data");
      return 0;
    }
  }
  myfree(line);

  return 1;
}

static int bgr_chans[] = { 2, 1, 0, };
static int grey_chans[] = { 0, 0, 0, };

static int
write_24bit_data(io_glue *ig, i_img *im) {
  int *chans;
  unsigned char *samples;
  int x, y;
  int line_size = 3 * im->xsize;
  
  line_size = (line_size + 3) / 4 * 4;
  
  if (!write_bmphead(ig, im, 24, line_size * im->ysize))
    return 0;
  chans = im->channels >= 3 ? bgr_chans : grey_chans;
  samples = mymalloc(line_size);
  for (y = im->ysize-1; y >= 0; --y) {
    i_gsamp(im, 0, im->xsize, y, samples, chans, 3);
    if (ig->writecb(ig, samples, line_size) < 0) {
      i_push_error(0, "writing image data");
      myfree(samples);
      return 0;
    }
  }
  myfree(samples);

  return 1;
}

/* no support for writing compressed (RLE8 or RLE4) BMP files */
int
i_writebmp_wiol(i_img *im, io_glue *ig) {
  io_glue_commit_types(ig);
  i_clear_error();

  /* pick a format */
  if (im->type == i_direct_type) {
    return write_24bit_data(ig, im);
  }
  else {
    int pal_size;

    /* must be paletted */
    pal_size = i_colorcount(im);
    if (pal_size <= 2) {
      return write_1bit_data(ig, im);
    }
    else if (pal_size <= 16) {
      return write_4bit_data(ig, im);
    }
    else {
      return write_8bit_data(ig, im);
    }
  }
}

static int
read_bmp_pal(io_glue *ig, i_img *im, int count) {
  int i;
  int r, g, b, x;
  i_color c;
  
  for (i = 0; i < count; ++i) {
    if (!read_packed(ig, "CCCC", &b, &g, &r, &x)) {
      i_push_error(0, "reading BMP palette");
      return 0;
    }
    c.channel[0] = r;
    c.channel[1] = g;
    c.channel[2] = b;
    if (i_addcolors(im, &c, 1) < 0)
      return 0;
  }
  
  return 1;
}

static i_img *
read_1bit_bmp(io_glue *ig, int xsize, int ysize, int clr_used) {
  i_img *im;
  int x, y, lasty, yinc;
  i_palidx *line, *p;
  unsigned char *packed;
  int line_size = (xsize + 7)/8;
  int byte, bit;
  unsigned char *in;

  line_size = (line_size+3) / 4 * 4;

  if (ysize > 0) {
    y = ysize-1;
    lasty = -1;
    yinc = -1;
  }
  else {
    /* when ysize is -ve it's a top-down image */
    ysize = -ysize;
    y = 0;
    lasty = ysize;
    yinc = 1;
  }
  im = i_img_pal_new(xsize, ysize, 3, 256);
  if (!read_bmp_pal(ig, im, clr_used)) {
    i_img_destroy(im);
    return NULL;
  }

  packed = mymalloc(line_size);
  line = mymalloc(xsize+8);
  while (y != lasty) {
    if (ig->readcb(ig, packed, line_size) != line_size) {
      myfree(packed);
      myfree(line);
      i_push_error(0, "reading 1-bit bmp data");
      i_img_destroy(im);
      return NULL;
    }
    in = packed;
    bit = 0x80;
    p = line;
    for (x = 0; x < xsize; ++x) {
      *p++ = (*in & bit) ? 1 : 0;
      bit >>= 1;
      if (!bit) {
	++in;
	bit = 0x80;
      }
    }
    i_ppal(im, 0, xsize, y, line);
    y += yinc;
  }

  return im;
}
#if 0

static i_img *
read_4bit_bmp(io_glue *ig, int xsize, int ysize, int clr_used) {
  i_img *im;
  int x, y, lasty, yinc;
  i_palidx *line, *p;
  unsigned char *packed;
  int line_size = (xsize + 1)/2;
  unsigned char *in;

  line_size = (line_size+3) / 4 * 4;

  if (ysize > 0) {
    y = ysize-1;
    lasty = -1;
    yinc = -1;
  }
  else {
    /* when ysize is -ve it's a top-down image */
    ysize = -ysize;
    y = 0;
    lasty = ysize;
    yinc = 1;
  }
  im = i_img_pal_new(xsize, ysize, 3, 256);
  if (!read_bmp_pal(ig, im, clr_used)) {
    i_img_destroy(im);
    return NULL;
  }

  packed = mymalloc(line_size);
  line = mymalloc(xsize+1);
  if (compression == BI_RGB) {
    while (y != lasty) {
      if (ig->readcb(ig, packed, line_size) != line_size) {
	myfree(packed);
	myfree(line);
	i_push_error(0, "reading 4-bit bmp data");
	i_img_destroy(im);
	return NULL;
      }
      in = packed;
      p = line;
      for (x = 0; x < xsize; x+=2) {
	*p++ = *in >> 4;
	*p++ = *in & 0x0F;
	++in;
      }
      i_ppal(im, 0, xsize, y, line);
      y += yinc;
    }
  }
  else if (compression == BI_RLE4) {
    return 0;
  }

  return im;
}

#endif

i_img *
i_readbmp_wiol(io_glue *ig) {
#if 0
  int b_magic, m_magic, filesize, dummy, infohead_size;
  int xsize, ysize, planes, bit_count, compression, size_image, xres, yres;
  int clr_used;
  i_img *im;
  
  io_glue_commit_types(ig);
  i_clear_error();

  if (!read_packed(ig, "CCVvvVVVVvvVVVVV", &b_magic, &m_magic, &filesize, 
		   &dummy, &dummy, &infohead_size, &xsize, &ysize, &planes,
		   &bit_count, &compression, &size_image, &xres, &yres, 
		   &clr_used, &dummy)) {
    i_push_error(0, "file too short");
    return 0;
  }
  if (b_magic != 'B' || m_magic != 'M' || infohead_size != INFOHEAD_SIZE
      || planes != 1) {
    i_push_error(0, "not a BMP file");
    return 0;
  }
  
  switch (bit_count) {
  case 1:
    im = read_1bit_bmp(ig, xsize, ysize, clr_used);
    break;

  case 4:
    im = read_4bit_bmp(ig, clr_used, compression);
    break;

  case 8:
    im = read_8bit_bmp(ig, clr_used, compression);
    break;

  case 32:
  case 24:
  case 16:
    im = read_direct_bmp(ig, clr_used, compression);
    break;
  }
#endif
  return 0;
}
