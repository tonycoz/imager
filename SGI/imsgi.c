#include "imsgi.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* values for the storage field */
#define SGI_STORAGE_VERBATIM 0
#define SGI_STORAGE_RLE 1

/* values for the colormap field */
#define SGI_COLORMAP_NORMAL 0
#define SGI_COLORMAP_DITHERED 1
#define SGI_COLORMAP_SCREEN 2
#define SGI_COLORMAP_COLORMAP 3

typedef struct {
  unsigned short imagic;
  unsigned char storagetype;
  unsigned char BPC;
  unsigned short dimensions;
  unsigned short xsize, ysize, zsize;
  unsigned int pixmin, pixmax;
  char name[80];
  unsigned int colormap;
} rgb_header;

static i_img *
read_rgb_8_verbatim(i_img *im, io_glue *ig, rgb_header const *hdr);
static i_img *
read_rgb_8_rle(i_img *im, io_glue *ig, rgb_header const *hdr);
static i_img *
read_rgb_16_verbatim(i_img *im, io_glue *ig, rgb_header const *hdr);
static i_img *
read_rgb_16_rle(i_img *im, io_glue *ig, rgb_header const *hdr);

#define Sample16ToF(num) ((num) / 65535.0)

#define _STRING(x) #x
#define STRING(x) _STRING(x)

/*
=head1 NAME

rgb.c - implements reading and writing sgi image files, uses io layer.

=head1 SYNOPSIS

   io_glue *ig = io_new_fd( fd );
   i_img *im   = i_readrgb_wiol(ig, 0); // disallow partial reads
   // or 
   io_glue *ig = io_new_fd( fd );
   return_code = i_writergb_wiol(im, ig); 

=head1 DESCRIPTION

imsgi.c implements the basic functions to read and write portable SGI
files.  It uses the iolayer and needs either a seekable source or an
entire memory mapped buffer.

=head1 FUNCTION REFERENCE

Some of these functions are internal.

=over

=cut
*/

typedef struct {
  int start, length;
} stlen_pair;

typedef enum { NoInit, Raw, Rle } rle_state;



typedef struct {
  int compressed;
  int bytepp;
  io_glue *ig;
} rgb_dest;






/*
=item rgb_header_unpack(header, headbuf)

Unpacks the header structure into from buffer and stores
in the header structure.

    header - header structure
    headbuf - buffer to unpack from

=cut
*/


static
void
rgb_header_unpack(rgb_header *header, unsigned char *headbuf) {
  header->imagic      = (headbuf[0]<<8) + headbuf[1];
  header->storagetype = headbuf[2];
  header->BPC         = headbuf[3];
  header->dimensions  = (headbuf[4]<<8) + headbuf[5];
  header->xsize       = (headbuf[6]<<8) + headbuf[7];
  header->ysize       = (headbuf[8]<<8) + headbuf[9];
  header->zsize       = (headbuf[10]<<8) + headbuf[11];
  header->pixmin      = (headbuf[12]<<24) + (headbuf[13]<<16)+(headbuf[14]<<8)+headbuf[15];
  header->pixmax      = (headbuf[16]<<24) + (headbuf[17]<<16)+(headbuf[18]<<8)+headbuf[19];
  memcpy(header->name,headbuf+24,80);
  header->name[79] = '\0';
  header->colormap    = (headbuf[104]<<24) + (headbuf[101]<<16)+(headbuf[102]<<8)+headbuf[103];
}

#if 0 /* this is currently unused */

/*
=item rgb_header_pack(header, headbuf)

Packs header structure into buffer for writing.

    header - header structure
    headbuf - buffer to pack into

=cut
*/

static
void
rgb_header_pack(rgb_header *header, unsigned char headbuf[512]) {
  
  header->imagic      = (headbuf[0]<<8) + headbuf[1];
  header->storagetype = headbuf[2];
  header->BPC         = headbuf[3];
  header->dimensions  = (headbuf[4]<<8) + headbuf[5];
  header->xsize       = (headbuf[6]<<8) + headbuf[7];
  header->ysize       = (headbuf[8]<<8) + headbuf[9];
  header->zsize       = (headbuf[10]<<8) + headbuf[11];
  header->pixmin      = (headbuf[12]<<24) + (headbuf[13]<<16)+(headbuf[14]<<8)+headbuf[15];
  header->pixmax      = (headbuf[16]<<24) + (headbuf[17]<<16)+(headbuf[18]<<8)+headbuf[19];
  memcpy(header->name,headbuf+24,80);
  header->colormap    = (headbuf[104]<<24) + (headbuf[101]<<16)+(headbuf[102]<<8)+headbuf[103];

}





/*
=item rgb_dest_write(s, buf, pixels)

Writes pixels from buf to destination s.  Takes care of compressing if the
destination is compressed.

    s - data destination
    buf - source buffer
    pixels - number of pixels to put write to destination

=cut
*/

static
int
rgb_dest_write(rgb_dest *s, unsigned char *buf, size_t pixels) {
  return -1;
}

#endif






/*
=item i_readsgi_wiol(ig, partial)

Read in an image from the iolayer data source and return the image structure to it.
Returns NULL on error.

   ig     - io_glue object
   length - maximum length to read from data source, before closing it -1 
            signifies no limit.

=cut
*/

i_img *
i_readsgi_wiol(io_glue *ig, int partial) {
  i_img *img = NULL;
  int width, height, channels;
  rgb_header header;
  unsigned char headbuf[512];

  mm_log((1,"i_readsgi(ig %p, partial %d)\n", ig, partial));
  i_clear_error();

  if (ig->readcb(ig, headbuf, 512) != 512) {
    i_push_error(errno, "SGI image: could not read header");
    return NULL;
  }

  rgb_header_unpack(&header, headbuf);

  if (header.imagic != 474) {
    i_push_error(0, "SGI image: invalid magic number");
    return NULL;
  }

  mm_log((1,"imagic:         %d\n", header.imagic));
  mm_log((1,"storagetype:    %d\n", header.storagetype));
  mm_log((1,"BPC:            %d\n", header.BPC));
  mm_log((1,"dimensions:     %d\n", header.dimensions));
  mm_log((1,"xsize:          %d\n", header.xsize));
  mm_log((1,"ysize:          %d\n", header.ysize));
  mm_log((1,"zsize:          %d\n", header.zsize));
  mm_log((1,"min:            %d\n", header.pixmin));
  mm_log((1,"max:            %d\n", header.pixmax));
  mm_log((1,"name [skipped]\n"));
  mm_log((1,"colormap:       %d\n", header.colormap));

  if (header.colormap != SGI_COLORMAP_NORMAL) {
    i_push_errorf(0, "SGI image: invalid value for colormap (%d)", header.colormap);
    return NULL;
  }

  if (header.BPC != 1 && header.BPC != 2) {
    i_push_errorf(0, "SGI image: invalid value for BPC (%d)", header.BPC);
    return NULL;
  }

  if (header.storagetype != SGI_STORAGE_VERBATIM 
      && header.storagetype != SGI_STORAGE_RLE) {
    i_push_error(0, "SGI image: invalid storage type field");
    return NULL;
  }

  if (header.pixmin >= header.pixmax) {
    i_push_error(0, "SGI image: invalid pixmin >= pixmax");
    return NULL;
  }

  width    = header.xsize;
  height   = header.ysize;
  channels = header.zsize;

  switch (header.dimensions) {
  case 1:
    channels = 1;
    height = 1;
    break;

  case 2:
    channels = 1;
    break;

  case 3:
    /* fall through and use all of the dimensions */
    break;

  default:
    i_push_error(0, "SGI image: invalid dimension field");
    return NULL;
  }

  if (!i_int_check_image_file_limits(width, height, channels, header.BPC)) {
    mm_log((1, "i_readsgi_wiol: image size exceeds limits\n"));
    return NULL;
  }

  if (header.BPC == 1) {
    img = i_img_8_new(width, height, channels);
    if (!img)
      goto ErrorReturn;

    switch (header.storagetype) {
    case SGI_STORAGE_VERBATIM:
      img = read_rgb_8_verbatim(img, ig, &header);
      break;

    case SGI_STORAGE_RLE:
      img = read_rgb_8_rle(img, ig, &header);
      break;

    default:
      goto ErrorReturn;
    }
  }
  else {
    img = i_img_16_new(width, height, channels);
    if (!img)
      goto ErrorReturn;

    switch (header.storagetype) {
    case SGI_STORAGE_VERBATIM:
      img = read_rgb_16_verbatim(img, ig, &header);
      break;

    case SGI_STORAGE_RLE:
      img = read_rgb_16_rle(img, ig, &header);
      break;

    default:
      goto ErrorReturn;
    }
  }

  if (!img)
    goto ErrorReturn;

  i_tags_set(&img->tags, "sgi_namestr", header.name, -1);
  i_tags_setn(&img->tags, "sgi_pixmin", header.pixmin);
  i_tags_setn(&img->tags, "sgi_pixmax", header.pixmax);
  i_tags_setn(&img->tags, "sgi_bpc", header.BPC);
  i_tags_setn(&img->tags, "sgi_rle", header.storagetype == SGI_STORAGE_RLE);
  i_tags_set(&img->tags, "i_format", "sgi", -1);

  return img;

 ErrorReturn:
  if (img) i_img_destroy(img);
  return NULL;
}

/*
=item i_writergb_wiol(img, ig)

Writes an image in targa format.  Returns 0 on error.

   img    - image to store
   ig     - io_glue object

=cut
*/

undef_int
i_writergb_wiol(i_img *img, io_glue *ig, int wierdpack, int compress, char *idstring, size_t idlen) {
  i_clear_error();
  i_push_error(0, "writing SGI RGB files is not implemented");

  return 0;
}

static i_img *
read_rgb_8_verbatim(i_img *img, io_glue *ig, rgb_header const *header) {
  i_color *linebuf;
  unsigned char *databuf;
  int c, y;
  int savemask;
  i_img_dim width = i_img_get_width(img);
  i_img_dim height = i_img_get_height(img);
  int channels = i_img_getchannels(img);
  int pixmin = header->pixmin;
  int pixmax = header->pixmax;
  int outmax = pixmax - pixmin;
  
  linebuf   = mymalloc(width * sizeof(i_color));
  databuf   = mymalloc(width);

  savemask = i_img_getmask(img);

  for(c = 0; c < channels; c++) {
    i_img_setmask(img, 1<<c);
    for(y = 0; y < height; y++) {
      int x;
      
      if (ig->readcb(ig, databuf, width) != width) {
	i_push_error(0, "SGI image: cannot read image data");
	i_img_destroy(img);
	myfree(linebuf);
	myfree(databuf);
	return NULL;
      }

      if (pixmin == 0 && pixmax == 255) {
	for(x = 0; x < img->xsize; x++)
	  linebuf[x].channel[c] = databuf[x];
      }
      else {
	for(x = 0; x < img->xsize; x++) {
	  int sample = databuf[x];
	  if (sample < pixmin)
	    sample = 0;
	  else if (sample > pixmax)
	    sample = outmax;
	  else
	    sample -= pixmin;
	    
	  linebuf[x].channel[c] = sample * 255 / outmax;
	}
      }
      
      i_plin(img, 0, width, height-1-y, linebuf);
    }
  }
  i_img_setmask(img, savemask);

  myfree(linebuf);
  myfree(databuf);
  
  return img;
}

static int
read_rle_tables(io_glue *ig, i_img *img,
		unsigned long **pstart_tab, unsigned long **plength_tab, 
		unsigned long *pmax_length) {
  i_img_dim height = i_img_get_height(img);
  int channels = i_img_getchannels(img);
  unsigned char *databuf;
  unsigned long *start_tab, *length_tab;
  unsigned long max_length = 0;
  int i;

  /* assumption: that the lengths are in bytes rather than in pixels */

  databuf    = mymalloc(height * channels * 4);
  start_tab  = mymalloc(height*channels*sizeof(unsigned long));
  length_tab = mymalloc(height*channels*sizeof(unsigned long));
    
    /* Read offset table */
  if (ig->readcb(ig, databuf, height * channels * 4) != height * channels * 4) {
    i_push_error(0, "SGI image: short read reading RLE start table");
    goto ErrorReturn;
  }

  for(i = 0; i < height * channels; i++) 
    start_tab[i] = (databuf[i*4] << 24) | (databuf[i*4+1] << 16) | 
      (databuf[i*4+2] << 8) | (databuf[i*4+3]);


  /* Read length table */
  if (ig->readcb(ig, databuf, height*channels*4) != height*channels*4) {
    i_push_error(0, "SGI image: short read reading RLE length table");
    goto ErrorReturn;
  }

  for(i=0; i < height * channels; i++) {
    length_tab[i] = (databuf[i*4] << 24) + (databuf[i*4+1] << 16)+
      (databuf[i*4+2] << 8) + (databuf[i*4+3]);
    if (length_tab[i] > max_length)
      max_length = length_tab[i];
  }

  mm_log((3, "Offset/length table:\n"));
  for(i=0; i < height * channels; i++)
    mm_log((3, "%d: %d/%d\n", i, start_tab[i], length_tab[i]));

  *pstart_tab = start_tab;
  *plength_tab = length_tab;
  *pmax_length = max_length;

  myfree(databuf);

  return 1;

 ErrorReturn:
  myfree(databuf);
  myfree(start_tab);
  myfree(length_tab);

  return 0;
}

static i_img *
read_rgb_8_rle(i_img *img, io_glue *ig, rgb_header const *header) {
  i_color *linebuf = NULL;
  unsigned char *databuf = NULL;
  unsigned long *start_tab, *length_tab;
  unsigned long max_length;
  i_img_dim width = i_img_get_width(img);
  i_img_dim height = i_img_get_height(img);
  int channels = i_img_getchannels(img);;
  i_img_dim y;
  int c;
  int pixmin = header->pixmin;
  int pixmax = header->pixmax;
  int outmax = pixmax - pixmin;

  if (!read_rle_tables(ig, img,  
		       &start_tab, &length_tab, &max_length)) {
    i_img_destroy(img);
    return NULL;
  }

  mm_log((1, "maxlen for an rle buffer: %d\n", max_length));

  if (max_length > (img->xsize + 1) * 2) {
    i_push_errorf(0, "SGI image: ridiculous RLE line length %lu", max_length);
    goto ErrorReturn;
  }

  linebuf = mymalloc(width*sizeof(i_color));
  databuf = mymalloc(max_length);

  for(y = 0; y < img->ysize; y++) {
    for(c = 0; c < channels; c++) {
      int ci = height * c + y;
      int datalen = length_tab[ci];
      unsigned char *inp;
      i_color *outp;
      int data_left = datalen;
      int pixels_left = width;
      i_sample_t sample;
      
      if (ig->seekcb(ig, start_tab[ci], SEEK_SET) != start_tab[ci]) {
	i_push_error(0, "SGI image: cannot seek to RLE data");
	goto ErrorReturn;
      }
      if (ig->readcb(ig, databuf, datalen) != datalen) {
	i_push_error(0, "SGI image: cannot read RLE data");
	goto ErrorReturn;
      }
      
      inp = databuf;
      outp = linebuf;
      while (data_left) {
	int code = *inp++;
	int count = code & 0x7f;
	--data_left;

	if (count == 0)
	  break;
	if (code & 0x80) {
	  /* literal run */
	  /* sanity checks */
	  if (count > pixels_left) {
	    i_push_error(0, "SGI image: literal run overflows scanline");
	    goto ErrorReturn;
	  }
	  if (count > data_left) {
	    i_push_error(0, "SGI image: literal run consumes more data than available");
	    goto ErrorReturn;
	  }
	  /* copy the run */
	  pixels_left -= count;
	  data_left -= count;
	  if (pixmin == 0 && pixmax == 255) {
	    while (count-- > 0) {
	      outp->channel[c] = *inp++;
	      ++outp;
	    }
	  }
	  else {
	    while (count-- > 0) {
	      int sample = *inp++;
	      if (sample < pixmin)
		sample = 0;
	      else if (sample > pixmax)
		sample = outmax;
	      else
		sample -= pixmin;
	      outp->channel[c] = sample * 255 / outmax;
	      ++outp;
	    }
	  }
	}
	else {
	  /* RLE run */
	  if (count > pixels_left) {
	    i_push_error(0, "SGI image: RLE run overflows scanline");
	    goto ErrorReturn;
	  }
	  if (data_left < 1) {
	    i_push_error(0, "SGI image: RLE run has no data for pixel");
	    goto ErrorReturn;
	  }
	  sample = *inp++;
	  if (pixmin != 0 || pixmax != 255) {
	    if (sample < pixmin)
	      sample = 0;
	    else if (sample > pixmax)
	      sample = outmax;
	    else
	      sample -= pixmin;
	    sample = sample * 255 / outmax;
	  }
	  --data_left;
	  pixels_left -= count;
	  while (count-- > 0) {
	    outp->channel[c] = sample;
	    ++outp;
	  }
	}
      }
      /* must have a full scanline */
      if (pixels_left) {
	i_push_error(0, "SGI image: incomplete RLE scanline");
	goto ErrorReturn;
      }
      /* must have used all of the data */
      if (data_left) {
	i_push_errorf(0, "SGI image: unused RLE data");
	goto ErrorReturn;
      }
    }
    i_plin(img, 0, width, height-1-y, linebuf);
  }

  myfree(linebuf);
  myfree(databuf);
  myfree(start_tab);
  myfree(length_tab);

  return img;

 ErrorReturn:
  if (linebuf)
    myfree(linebuf);
  if (databuf)
    myfree(databuf);
  myfree(start_tab);
  myfree(length_tab);
  i_img_destroy(img);
  return NULL;
}

static i_img *
read_rgb_16_verbatim(i_img *img, io_glue *ig, rgb_header const *header) {
  i_fcolor *linebuf;
  unsigned char *databuf;
  int c, y;
  int savemask;
  i_img_dim width = i_img_get_width(img);
  i_img_dim height = i_img_get_height(img);
  int channels = i_img_getchannels(img);
  int pixmin = header->pixmin;
  int pixmax = header->pixmax;
  int outmax = pixmax - pixmin;
  
  linebuf   = mymalloc(width * sizeof(i_fcolor));
  databuf   = mymalloc(width * 2);

  savemask = i_img_getmask(img);

  for(c = 0; c < channels; c++) {
    i_img_setmask(img, 1<<c);
    for(y = 0; y < height; y++) {
      int x;
      
      if (ig->readcb(ig, databuf, width*2) != width*2) {
	i_push_error(0, "SGI image: cannot read image data");
	i_img_destroy(img);
	myfree(linebuf);
	myfree(databuf);
	return NULL;
      }

      if (pixmin == 0 && pixmax == 65535) {
	for(x = 0; x < img->xsize; x++)
	  linebuf[x].channel[c] = (databuf[x*2] * 256 + databuf[x*2+1]) / 65535.0;
      }
      else {
	for(x = 0; x < img->xsize; x++) {
	  int sample = databuf[x*2] * 256 + databuf[x*2+1];
	  if (sample < pixmin)
	    sample = 0;
	  else if (sample > pixmax)
	    sample = outmax;
	  else
	    sample -= pixmin;
	    
	  linebuf[x].channel[c] = (double)sample / outmax;
	}
      }
      
      i_plinf(img, 0, width, height-1-y, linebuf);
    }
  }
  i_img_setmask(img, savemask);

  myfree(linebuf);
  myfree(databuf);
  
  return img;
}

static i_img *
read_rgb_16_rle(i_img *img, io_glue *ig, rgb_header const *header) {
  i_fcolor *linebuf = NULL;
  unsigned char *databuf = NULL;
  unsigned long *start_tab, *length_tab;
  unsigned long max_length;
  i_img_dim width = i_img_get_width(img);
  i_img_dim height = i_img_get_height(img);
  int channels = i_img_getchannels(img);;
  i_img_dim y;
  int c;
  int pixmin = header->pixmin;
  int pixmax = header->pixmax;
  int outmax = pixmax - pixmin;

  if (!read_rle_tables(ig, img,  
		       &start_tab, &length_tab, &max_length)) {
    i_img_destroy(img);
    return NULL;
  }

  mm_log((1, "maxlen for an rle buffer: %lu\n", max_length));

  if (max_length > (img->xsize * 2 + 1) * 2) {
    i_push_errorf(0, "SGI image: ridiculous RLE line length %lu", max_length);
    goto ErrorReturn;
  }

  linebuf = mymalloc(width*sizeof(i_fcolor));
  databuf = mymalloc(max_length);

  for(y = 0; y < img->ysize; y++) {
    for(c = 0; c < channels; c++) {
      int ci = height * c + y;
      int datalen = length_tab[ci];
      unsigned char *inp;
      i_fcolor *outp;
      int data_left = datalen;
      int pixels_left = width;
      int sample;
      
      if (datalen & 1) {
	i_push_error(0, "SGI image: invalid RLE length value for BPC=2");
	goto ErrorReturn;
      }
      if (ig->seekcb(ig, start_tab[ci], SEEK_SET) != start_tab[ci]) {
	i_push_error(0, "SGI image: cannot seek to RLE data");
	goto ErrorReturn;
      }
      if (ig->readcb(ig, databuf, datalen) != datalen) {
	i_push_error(0, "SGI image: cannot read RLE data");
	goto ErrorReturn;
      }
      
      inp = databuf;
      outp = linebuf;
      while (data_left > 0) {
	int code = inp[0] * 256 + inp[1];
	int count = code & 0x7f;
	inp += 2;
	data_left -= 2;

	if (count == 0)
	  break;
	if (code & 0x80) {
	  /* literal run */
	  /* sanity checks */
	  if (count > pixels_left) {
	    i_push_error(0, "SGI image: literal run overflows scanline");
	    goto ErrorReturn;
	  }
	  if (count > data_left) {
	    i_push_error(0, "SGI image: literal run consumes more data than available");
	    goto ErrorReturn;
	  }
	  /* copy the run */
	  pixels_left -= count;
	  data_left -= count * 2;
	  if (pixmin == 0 && pixmax == 65535) {
	    while (count-- > 0) {
	      outp->channel[c] = (inp[0] * 256 + inp[1]) / 65535.0;
	      inp += 2;
	      ++outp;
	    }
	  }
	  else {
	    while (count-- > 0) {
	      int sample = inp[0] * 256 + inp[1];
	      if (sample < pixmin)
		sample = 0;
	      else if (sample > pixmax)
		sample = outmax;
	      else
		sample -= pixmin;
	      outp->channel[c] = (double)sample / outmax;
	      ++outp;
	      inp += 2;
	    }
	  }
	}
	else {
	  double fsample;
	  /* RLE run */
	  if (count > pixels_left) {
	    i_push_error(0, "SGI image: RLE run overflows scanline");
	    goto ErrorReturn;
	  }
	  if (data_left < 2) {
	    i_push_error(0, "SGI image: RLE run has no data for pixel");
	    goto ErrorReturn;
	  }
	  sample = inp[0] * 256 + inp[1];
	  inp += 2;
	  data_left -= 2;
	  if (pixmin != 0 || pixmax != 65535) {
	    if (sample < pixmin)
	      sample = 0;
	    else if (sample > pixmax)
	      sample = outmax;
	    else
	      sample -= pixmin;
	    fsample = (double)sample / outmax;
	  }
	  else {
	    fsample = (double)sample / 65535.0;
	  }
	  pixels_left -= count;
	  while (count-- > 0) {
	    outp->channel[c] = fsample;
	    ++outp;
	  }
	}
      }
      /* must have a full scanline */
      if (pixels_left) {
	i_push_error(0, "SGI image: incomplete RLE scanline");
	goto ErrorReturn;
      }
      /* must have used all of the data */
      if (data_left) {
	i_push_errorf(0, "SGI image: unused RLE data");
	goto ErrorReturn;
      }
    }
    i_plinf(img, 0, width, height-1-y, linebuf);
  }

  myfree(linebuf);
  myfree(databuf);
  myfree(start_tab);
  myfree(length_tab);

  return img;

 ErrorReturn:
  if (linebuf)
    myfree(linebuf);
  if (databuf)
    myfree(databuf);
  myfree(start_tab);
  myfree(length_tab);
  i_img_destroy(img);
  return NULL;
}
