#include "image.h"
#include <stdarg.h>

/*
=head1 NAME

bmp.c - read and write windows BMP files

=head1 SYNOPSIS

  i_img *im;
  io_glue *ig;

  if (!i_writebmp_wiol(im, ig)) {
    ... error ...
  }
  im = i_readbmp(ig);

=head1 DESCRIPTION

Reads and writes Windows BMP files.

=over

=cut
*/

#define FILEHEAD_SIZE 14
#define INFOHEAD_SIZE 40
#define BI_RGB		0
#define BI_RLE8		1
#define BI_RLE4		2
#define BI_BITFIELDS	3
#define BMPRLE_ENDOFLINE 0
#define BMPRLE_ENDOFBMP 1
#define BMPRLE_DELTA 2

static int read_packed(io_glue *ig, char *format, ...);
static int write_packed(io_glue *ig, char *format, ...);
static int write_bmphead(io_glue *ig, i_img *im, int bit_count, 
                         int data_size);
static int write_1bit_data(io_glue *ig, i_img *im);
static int write_4bit_data(io_glue *ig, i_img *im);
static int write_8bit_data(io_glue *ig, i_img *im);
static int write_24bit_data(io_glue *ig, i_img *im);
static int read_bmp_pal(io_glue *ig, i_img *im, int count);
static i_img *read_1bit_bmp(io_glue *ig, int xsize, int ysize, 
                            int clr_used);
static i_img *read_4bit_bmp(io_glue *ig, int xsize, int ysize, int clr_used, 
                            int compression);
static i_img *read_8bit_bmp(io_glue *ig, int xsize, int ysize, int clr_used, 
                            int compression);
static i_img *read_direct_bmp(io_glue *ig, int xsize, int ysize, 
                              int bit_count, int clr_used, int compression);

/* 
=item i_writebmp_wiol(im, io_glue)

Writes the image as a BMP file.  Uses 1-bit, 4-bit, 8-bit or 24-bit
formats depending on the image.

Never compresses the image.

=cut
*/
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

/*
=item i_readbmp_wiol(ig)

Reads a Windows format bitmap from the given file.

Handles BI_RLE4 and BI_RLE8 compressed images.  Attempts to handle
BI_BITFIELDS images too, but I need a test image.

=cut
*/

i_img *
i_readbmp_wiol(io_glue *ig) {
  int b_magic, m_magic, filesize, dummy, infohead_size;
  int xsize, ysize, planes, bit_count, compression, size_image, xres, yres;
  int clr_used, clr_important, offbits;
  i_img *im;
  
  io_glue_commit_types(ig);
  i_clear_error();

  if (!read_packed(ig, "CCVvvVVVVvvVVVVVV", &b_magic, &m_magic, &filesize, 
		   &dummy, &dummy, &offbits, &infohead_size, 
                   &xsize, &ysize, &planes,
		   &bit_count, &compression, &size_image, &xres, &yres, 
		   &clr_used, &clr_important)) {
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
    im = read_4bit_bmp(ig, xsize, ysize, clr_used, compression);
    break;

  case 8:
    im = read_8bit_bmp(ig, xsize, ysize, clr_used, compression);
    break;

  case 32:
  case 24:
  case 16:
    im = read_direct_bmp(ig, xsize, ysize, bit_count, clr_used, compression);
    break;

  default:
    i_push_errorf(0, "unknown bit count for BMP file (%d)", bit_count);
    return NULL;
  }

  /* store the resolution */
  if (xres && !yres)
    yres = xres;
  else if (yres && !xres)
    xres = yres;
  if (xres) {
    i_tags_set_float(&im->tags, "i_xres", 0, xres * 0.0254);
    i_tags_set_float(&im->tags, "i_yres", 0, yres * 0.0254);
  }
  i_tags_addn(&im->tags, "bmp_compression", 0, compression);
  i_tags_addn(&im->tags, "bmp_important_colors", 0, clr_important);

  return im;
}

/*
=back

=head1 IMPLEMENTATION FUNCTIONS

Internal functions used in the implementation.

=over

=item read_packed(ig, format, ...)

Reads from the specified "file" the specified sizes.  The format codes
match those used by perl's pack() function, though only a few are
implemented.  In all cases the vararg arguement is an int *.

Returns non-zero if all of the arguments were read.

=cut
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
      
    case '3': /* extension - 24-bit number */
      if (ig->readcb(ig, buf, 3) == -1)
        return 0;
      *p = buf[0] + (buf[1] << 8) + (buf[2] << 16);
      break;
      
    default:
      m_fatal(1, "Unknown read_packed format code 0x%02x", *format);
    }
    ++format;
  }
  return 1;
}

/*
=item write_packed(ig, format, ...)

Writes packed data to the specified io_glue.

Returns non-zero on success.

=cut
*/

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
      m_fatal(1, "Unknown write_packed format code 0x%02x", *format);
    }
    ++format;
  }
  va_end(ap);

  return 1;
}

/*
=item write_bmphead(ig, im, bit_count, data_size)

Writes a Windows BMP header to the file.

Returns non-zero on success.

=cut
*/

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
		    bit_count, BI_RGB, 0, (int)(xres+0.5), (int)(yres+0.5), 
		    colors_used, colors_used)){
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

/*
=item write_1bit_data(ig, im)

Writes the image data as a 1-bit/pixel image.

Returns non-zero on success.

=cut
*/
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

  ig->closecb(ig);

  return 1;
}

/*
=item write_4bit_data(ig, im)

Writes the image data as a 4-bit/pixel image.

Returns non-zero on success.

=cut
*/
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

  ig->closecb(ig);

  return 1;
}

/*
=item write_8bit_data(ig, im)

Writes the image data as a 8-bit/pixel image.

Returns non-zero on success.

=cut
*/
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

  ig->closecb(ig);

  return 1;
}

static int bgr_chans[] = { 2, 1, 0, };
static int grey_chans[] = { 0, 0, 0, };

/*
=item write_24bit_data(ig, im)

Writes the image data as a 24-bit/pixel image.

Returns non-zero on success.

=cut
*/
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
  memset(samples, 0, line_size);
  for (y = im->ysize-1; y >= 0; --y) {
    i_gsamp(im, 0, im->xsize, y, samples, chans, 3);
    if (ig->writecb(ig, samples, line_size) < 0) {
      i_push_error(0, "writing image data");
      myfree(samples);
      return 0;
    }
  }
  myfree(samples);

  ig->closecb(ig);

  return 1;
}

/*
=item read_bmp_pal(ig, im, count)

Reads count palette entries from the file and add them to the image.

Returns non-zero on success.

=cut
*/
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

/*
=item read_1bit_bmp(ig, xsize, ysize, clr_used)

Reads in the palette and image data for a 1-bit/pixel image.

Returns the image or NULL.

=cut
*/
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
  if (!clr_used)
    clr_used = 2;
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

  myfree(packed);
  myfree(line);
  return im;
}

/*
=item read_4bit_bmp(ig, xsize, ysize, clr_used, compression)

Reads in the palette and image data for a 4-bit/pixel image.

Returns the image or NULL.

Hopefully this will be combined with the following function at some
point.

=cut
*/
static i_img *
read_4bit_bmp(io_glue *ig, int xsize, int ysize, int clr_used, 
              int compression) {
  i_img *im;
  int x, y, lasty, yinc;
  i_palidx *line, *p;
  unsigned char *packed;
  int line_size = (xsize + 1)/2;
  unsigned char *in;
  int size, i;

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
  if (!clr_used)
    clr_used = 16;
  if (!read_bmp_pal(ig, im, clr_used)) {
    i_img_destroy(im);
    return NULL;
  }

  if (line_size < 260)
    packed = mymalloc(260);
  else
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
    myfree(packed);
    myfree(line);
  }
  else if (compression == BI_RLE4) {
    int read_size;
    int want_high;
    int count;

    x = 0;
    while (1) {
      /* there's always at least 2 bytes in a sequence */
      if (ig->readcb(ig, packed, 2) != 2) {
        myfree(packed);
        myfree(line);
        i_push_error(0, "missing data during decompression");
        i_img_destroy(im);
        return NULL;
      }
      else if (packed[0]) {
        line[0] = packed[1] >> 4;
        line[1] = packed[1] & 0x0F;
        for (i = 0; i < packed[0]; i += 2) {
          if (i < packed[0]-1) 
            i_ppal(im, x, x+2, y, line);
          else
            i_ppal(im, x, x+(packed[0]-i), y, line);
          x += 2;
        }
      } else {
        switch (packed[1]) {
        case BMPRLE_ENDOFLINE:
          x = 0;
          y += yinc;
          break;

        case BMPRLE_ENDOFBMP:
          myfree(packed);
          myfree(line);
          return im;

        case BMPRLE_DELTA:
          if (ig->readcb(ig, packed, 2) != 2) {
            myfree(packed);
            myfree(line);
            i_push_error(0, "missing data during decompression");
            i_img_destroy(im);
            return NULL;
          }
          x += packed[0];
          y += yinc * packed[1];
          break;

        default:
          count = packed[1];
          size = (count + 1) / 2;
          read_size = (size+1) / 2 * 2;
          if (ig->readcb(ig, packed, read_size) != read_size) {
            myfree(packed);
            myfree(line);
            i_push_error(0, "missing data during decompression");
            /*i_img_destroy(im);*/
            return im;
          }
          for (i = 0; i < size; ++i) {
            line[0] = packed[i] >> 4;
            line[1] = packed[i] & 0xF;
            i_ppal(im, x, x+2, y, line);
            x += 2;
          }
          break;
        }
      }
    }
  }
  else { /*if (compression == BI_RLE4) {*/
    myfree(packed);
    myfree(line);
    i_push_error(0, "bad compression for 4-bit image");
    i_img_destroy(im);
    return NULL;
  }

  return im;
}

/*
=item read_8bit_bmp(ig, xsize, ysize, clr_used, compression)

Reads in the palette and image data for a 8-bit/pixel image.

Returns the image or NULL.

=cut
*/
static i_img *
read_8bit_bmp(io_glue *ig, int xsize, int ysize, int clr_used, 
              int compression) {
  i_img *im;
  int x, y, lasty, yinc;
  i_palidx *line, *p;
  int line_size = xsize;
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
  if (!clr_used)
    clr_used = 256;
  if (!read_bmp_pal(ig, im, clr_used)) {
    i_img_destroy(im);
    return NULL;
  }

  line = mymalloc(line_size);
  if (compression == BI_RGB) {
    while (y != lasty) {
      if (ig->readcb(ig, line, line_size) != line_size) {
	myfree(line);
	i_push_error(0, "reading 8-bit bmp data");
	i_img_destroy(im);
	return NULL;
      }
      i_ppal(im, 0, xsize, y, line);
      y += yinc;
    }
    myfree(line);
  }
  else if (compression == BI_RLE8) {
    int read_size;
    int want_high;
    int count;
    unsigned char packed[2];

    x = 0;
    while (1) {
      /* there's always at least 2 bytes in a sequence */
      if (ig->readcb(ig, packed, 2) != 2) {
        myfree(line);
        i_push_error(0, "missing data during decompression");
        i_img_destroy(im);
        return NULL;
      }
      if (packed[0]) {
        memset(line, packed[1], packed[0]);
        i_ppal(im, x, x+packed[0], y, line);
        x += packed[0];
      } else {
        switch (packed[1]) {
        case BMPRLE_ENDOFLINE:
          x = 0;
          y += yinc;
          break;

        case BMPRLE_ENDOFBMP:
          myfree(line);
          return im;

        case BMPRLE_DELTA:
          if (ig->readcb(ig, packed, 2) != 2) {
            myfree(line);
            i_push_error(0, "missing data during decompression");
            i_img_destroy(im);
            return NULL;
          }
          x += packed[0];
          y += yinc * packed[1];
          break;

        default:
          count = packed[1];
          read_size = (count+1) / 2 * 2;
          if (ig->readcb(ig, line, read_size) != read_size) {
            myfree(line);
            i_push_error(0, "missing data during decompression");
            i_img_destroy(im);
            return NULL;
          }
          i_ppal(im, x, x+count, y, line);
          x += count;
          break;
        }
      }
    }
  }
  else { 
    myfree(line);
    i_push_errorf(0, "unknown 8-bit BMP compression %d", compression);
    i_img_destroy(im);
    return NULL;
  }

  return im;
}

struct bm_masks {
  unsigned masks[3];
  int shifts[3];
};
static struct bm_masks std_masks[] =
{
  { /* 16-bit */
    { 0770000, 00007700, 00000077, },
    { 10, 4, -2, },
  },
  { /* 24-bit */
    { 0xFF0000, 0x00FF00, 0x0000FF, },
    {       16,        8,        0, },
  },
  { /* 32-bit */
    { 0xFF0000, 0x00FF00, 0x0000FF, },
    {       16,        8,        0, },
  },
};

/*
=item read_direct_bmp(ig, xsize, ysize, bit_count, clr_used, compression)

Skips the palette and reads in the image data for a direct colour image.

Returns the image or NULL.

=cut
*/
static i_img *
read_direct_bmp(io_glue *ig, int xsize, int ysize, int bit_count, 
                int clr_used, int compression) {
  i_img *im;
  int x, y, lasty, yinc;
  i_color *line, *p;
  unsigned char *in;
  int pix_size = bit_count / 8;
  int line_size = xsize * pix_size;
  struct bm_masks masks;
  char unpack_code[2] = "";
  int i;
  int extras;
  char junk[4];
  
  unpack_code[0] = *("v3V"+pix_size-2);
  unpack_code[1] = '\0';

  line_size = (line_size+3) / 4 * 4;
  extras = line_size - xsize * pix_size;

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
  if (compression == BI_RGB) {
    masks = std_masks[pix_size-2];
    
    /* there's a potential "palette" after the header */
    for (i = 0; i < clr_used; ++clr_used) {
      char buf[4];
      if (ig->readcb(ig, buf, 4) != 4) {
        i_push_error(0, "skipping colors");
        return 0;
      }
    }
  }
  else if (compression == BI_BITFIELDS) {
    int pos, bit;
    for (i = 0; i < 3; ++i) {
      if (!read_packed(ig, "V", masks.masks+i)) {
        i_push_error(0, "reading pixel masks");
        return 0;
      }
      /* work out a shift for the mask */
      pos = 0;
      bit = masks.masks[i] & -masks.masks[i];
      while (bit) {
        ++pos;
        bit >>= 1;
      }
      masks.shifts[i] = pos - 8;
    }
  }

  im = i_img_empty(NULL, xsize, ysize);

  line = mymalloc(sizeof(i_color) * xsize);
  while (y != lasty) {
    p = line;
    for (x = 0; x < xsize; ++x) {
      unsigned pixel;
      if (!read_packed(ig, unpack_code, &pixel)) {
        i_push_error(0, "reading image data");
        myfree(line);
        i_img_destroy(im);
        return NULL;
      }
      for (i = 0; i < 3; ++i) {
        if (masks.shifts[i] > 0)
          p->channel[i] = (pixel & masks.masks[i]) >> masks.shifts[i];
        else 
          p->channel[i] = (pixel & masks.masks[i]) << -masks.shifts[i];
      }
      ++p;
    }
    i_plin(im, 0, xsize, y, line);
    if (extras)
      ig->readcb(ig, junk, extras);
    y += yinc;
  }
  myfree(line);

  return im;
}

/*
=head1 SEE ALSO

Imager(3)

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 RESTRICTIONS

Cannot save as compressed BMP.

=head1 BUGS

Doesn't handle OS/2 bitmaps.

16-bit/pixel images haven't been tested.  (I need an image).

BI_BITFIELDS compression hasn't been tested (I need an image).

=cut
*/
