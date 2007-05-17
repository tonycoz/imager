#include "imager.h"
#include "log.h"
#include "iolayer.h"

#include <stdlib.h>
#include <errno.h>

/* values for the storage field */
#define SGI_STORAGE_VERBATIM 0
#define SGI_STORAGE_RLE 1

/* values for the colormap field */
#define SGI_COLORMAP_NORMAL 0
#define SGI_COLORMAP_DITHERED 1
#define SGI_COLORMAP_SCREEN 2
#define SGI_COLORMAP_COLORMAP 3

static i_img *
read_rgb_8_verbatim(i_img *im, i_mempool *mp, io_glue *ig);
static i_img *
read_rgb_8_rle(i_img *im, i_mempool *mp, io_glue *ig);
static i_img *
read_rgb_16_verbatim(i_img *im, i_mempool *mp, io_glue *ig);
static i_img *
read_rgb_16_rle(i_img *im, i_mempool *mp, io_glue *ig);

/*
=head1 NAME

rgb.c - implements reading and writing sgi image files, uses io layer.

=head1 SYNOPSIS

   io_glue *ig = io_new_fd( fd );
   i_img *im   = i_readrgb_wiol(ig, -1); // no limit on how much is read
   // or 
   io_glue *ig = io_new_fd( fd );
   return_code = i_writergb_wiol(im, ig); 

=head1 DESCRIPTION

rgb.c implements the basic functions to read and write portable targa
files.  It uses the iolayer and needs either a seekable source or an
entire memory mapped buffer.

=head1 FUNCTION REFERENCE

Some of these functions are internal.

=over

=cut
*/

typedef struct {
  unsigned short imagic;
  unsigned char storagetype;
  unsigned char BPC;
  unsigned short dimensions;
  unsigned short xsize, ysize, zsize;
  unsigned int min, max;
  char name[80];
  unsigned int colormap;
} rgb_header;

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
  header->min         = (headbuf[12]<<24) + (headbuf[13]<<16)+(headbuf[14]<<8)+headbuf[15];
  header->max         = (headbuf[16]<<24) + (headbuf[17]<<16)+(headbuf[18]<<8)+headbuf[19];
  memcpy(header->name,headbuf+20,80);
  header->colormap    = (headbuf[100]<<24) + (headbuf[101]<<16)+(headbuf[102]<<8)+headbuf[103];
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
  header->min         = (headbuf[12]<<24) + (headbuf[13]<<16)+(headbuf[14]<<8)+headbuf[15];
  header->max         = (headbuf[16]<<24) + (headbuf[17]<<16)+(headbuf[18]<<8)+headbuf[19];
  memcpy(header->name,headbuf+20,80);
  header->colormap    = (headbuf[100]<<24) + (headbuf[101]<<16)+(headbuf[102]<<8)+headbuf[103];

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
=item i_readrgb_wiol(ig, partial)

Read in an image from the iolayer data source and return the image structure to it.
Returns NULL on error.

   ig     - io_glue object
   length - maximum length to read from data source, before closing it -1 
            signifies no limit.

=cut
*/

i_img *
i_readrgb_wiol(io_glue *ig, int partial) {
  i_img *img = NULL;
  int width, height, channels;
  rgb_header header;
  unsigned char headbuf[512];
  i_mempool mp;

  mm_log((1,"i_readrgb(ig %p, partial %d)\n", ig, partial));
  i_clear_error();
  io_glue_commit_types(ig);

  if (ig->readcb(ig, headbuf, 512) != 512) {
    i_push_error(errno, "could not read SGI rgb header");
    return NULL;
  }

  rgb_header_unpack(&header, headbuf);
  
  
  mm_log((1,"imagic:         %d\n", header.imagic));
  mm_log((1,"storagetype:    %d\n", header.storagetype));
  mm_log((1,"BPC:            %d\n", header.BPC));
  mm_log((1,"dimensions:     %d\n", header.dimensions));
  mm_log((1,"xsize:          %d\n", header.xsize));
  mm_log((1,"ysize:          %d\n", header.ysize));
  mm_log((1,"zsize:          %d\n", header.zsize));
  mm_log((1,"min:            %d\n", header.min));
  mm_log((1,"max:            %d\n", header.max));
  mm_log((1,"name [skipped]\n"));
  mm_log((1,"colormap:       %d\n", header.colormap));

  if (header.colormap != 0) {
    i_push_error(0, "SGI rgb image has a non zero colormap entry - obsolete format");
    return NULL;
  }

  if (header.storagetype != 0 && header.storagetype != 1) {
    i_push_error(0, "SGI rgb image has has invalid storage field");
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
    i_push_error(0, "Invalid value for dimension in header");
    return NULL;
  }

  if (header.colormap != SGI_COLORMAP_NORMAL) {
    i_push_errorf(0, "invalid value for colormap (%d)", header.colormap);
    return NULL;
  }

  if (header.BPC != 1 && header.BPC != 2) {
    i_push_errorf(0, "invalid value for BPC (%d)", header.BPC);
    return NULL;
  }

  if (!i_int_check_image_file_limits(width, height, channels, header.BPC)) {
    mm_log((1, "i_readbmp_wiol: image size exceeds limits\n"));
    return NULL;
  }

  i_mempool_init(&mp);
  
  if (header.BPC == 1) {
    img = i_img_8_new(width, height, channels);
    if (!img)
      goto ErrorReturn;

    switch (header.storagetype) {
    case SGI_STORAGE_VERBATIM:
      img = read_rgb_8_verbatim(img, &mp, ig);
      break;

    case SGI_STORAGE_RLE:
      img = read_rgb_8_rle(img, &mp, ig);
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
      img = read_rgb_16_verbatim(img, &mp, ig);
      break;

    case SGI_STORAGE_RLE:
      img = read_rgb_16_rle(img, &mp, ig);
      break;

    default:
      goto ErrorReturn;
    }
  }

  if (!img)
    goto ErrorReturn;

  switch (header.storagetype) {
  case 0: /* uncompressed */
    
    break;
  case 1: /* RLE compressed */
    
    
    break;
  }

  i_tags_add(&img->tags, "rgb_namestr", 0, header.name, 80, 0);
  i_tags_add(&img->tags, "i_format", 0, "rgb", -1, 0);

  i_mempool_destroy(&mp);
  return img;

 ErrorReturn:
  i_mempool_destroy(&mp);
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
read_rgb_8_verbatim(i_img *img, i_mempool *mp, io_glue *ig) {
  i_color *linebuf;
  unsigned char *databuf;
  int c, y;
  int savemask;
  
  linebuf   = i_mempool_alloc(mp, img->xsize * sizeof(i_color));
  databuf   = i_mempool_alloc(mp, img->xsize);

  savemask = i_img_getmask(img);

  for(c = 0; c < img->channels; c++) {
    i_img_setmask(img, 1<<c);
    for(y = 0; y < img->ysize; y++) {
      int x;
      
      if (ig->readcb(ig, databuf, img->xsize) != img->xsize) {
	i_push_error(0, "SGI rgb: cannot read");
	i_img_destroy(img);
	return NULL;
      }
      
      for(x = 0; x < img->xsize; x++)
	linebuf[x].channel[c] = databuf[x];
      
      i_plin(img, 0, img->xsize, img->ysize-1-y, linebuf);
    }
  }
  i_img_setmask(img, savemask);
  
  return img;
}

static int
read_rle_tables(io_glue *ig, i_img *img, i_mempool *mp,
		unsigned long **pstart_tab, unsigned long **plength_tab, 
		unsigned long *pmax_length) {
  i_img_dim height = img->ysize;
  int channels = img->channels;
  unsigned char *databuf;
  unsigned long *start_tab, *length_tab;
  unsigned long max_length = 0;
  int i;

  /* assumption: that the lengths are in bytes rather than in pixels */

  databuf    = i_mempool_alloc(mp, height * channels * 4);
  start_tab  = i_mempool_alloc(mp, height*channels*sizeof(unsigned long));
  length_tab = i_mempool_alloc(mp, height*channels*sizeof(unsigned long));
    
    /* Read offset table */
  if (ig->readcb(ig, databuf, height * channels * 4) != height * channels * 4) {
    i_push_error(0, "short read reading RLE tables");
    return 0;
  }

  for(i = 0; i < height * channels; i++) 
    start_tab[i] = (databuf[i*4] << 24) | (databuf[i*4+1] << 16) | 
      (databuf[i*4+2] << 8) | (databuf[i*4+3]);


  /* Read length table */
  if (ig->readcb(ig, databuf, height*channels*4) != height*channels*4) {
    i_push_error(0, "short read reading RLE tables");
    return 0;
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

  return 1;
}

static i_img *
read_rgb_8_rle(i_img *img, i_mempool *mp, io_glue *ig) {
  i_color *linebuf;
  unsigned char *databuf;
  unsigned long *start_tab, *length_tab;
  unsigned long max_length;
  i_img_dim width = img->xsize;
  i_img_dim height = img->ysize;
  int channels = img->channels;
  i_img_dim y;
  int c;

  if (!read_rle_tables(ig, img, mp, 
		       &start_tab, &length_tab, &max_length)) {
    i_img_destroy(img);
    return NULL;
  }

  mm_log((1, "maxlen for an rle buffer: %d\n", max_length));

  if (max_length > (img->xsize + 1) * 2) {
    i_push_errorf(0, "SGI rgb: ridiculous RLE line length %uld", max_length);
    i_img_destroy(img);
    return NULL;
  }

  linebuf = i_mempool_alloc(mp, width*sizeof(i_color));
  databuf = i_mempool_alloc(mp, max_length);

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
	i_push_error(0, "SGI rgb: cannot seek");
	i_img_destroy(img);
	return NULL;
      }
      if (ig->readcb(ig, databuf, datalen) != datalen) {
	i_push_error(0, "SGI rgb: cannot read");
	i_img_destroy(img);
	return NULL;
      }
      
      inp = databuf;
      outp = linebuf;
      while (data_left && pixels_left) {
	int code = *inp++;
	int count = code & 0x7f;
	--data_left;
	if (count == 0)
	  break;
	if (code & 0x80) {
	  /* literal run */
	  /* sanity checks */
	  if (count > pixels_left) {
	    i_push_error(0, "SGI rgb: literal run overflows scanline");
	    i_img_destroy(img);
	    return NULL;
	  }
	  if (count > data_left) {
	    i_push_error(0, "SGI rgb: literal run consume more data than available");
	    i_img_destroy(img);
	    return NULL;
	  }
	  /* copy the run */
	  pixels_left -= count;
	  data_left -= count;
	  while (count-- > 0) {
	    outp->channel[c] = *inp++;
	    ++outp;
	  }
	}
	else {
	  /* RLE run */
	  if (count > pixels_left) {
	    i_push_error(0, "SGI rgb: RLE run overflows scanline");
	    i_img_destroy(img);
	    return NULL;
	  }
	  if (data_left < 1) {
	    i_push_error(0, "SGI rgb: RLE run has no data for pixel");
	    i_img_destroy(img);
	    return NULL;
	  }
	  sample = *inp++;
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
	i_push_error(0, "SGI rgb: incomplete RLE scanline");
	i_img_destroy(img);
	return NULL;
      }
      /* must have used all of the data */
      if (data_left) {
	i_push_error(0, "SGI rgb: unused RLE data");
	i_img_destroy(img);
	return NULL;
      }
    }
    i_plin(img, 0, width, height-1-y, linebuf);
  }

  return img;
}

static i_img *
read_rgb_16_verbatim(i_img *img, i_mempool *mp, io_glue *ig) {
  i_push_error(0, "SGI rgb: 16 bit uncompressed images not supported");
  i_img_destroy(img);
  return NULL;
}

static i_img *
read_rgb_16_rle(i_img *img, i_mempool *mp, io_glue *ig) {
  i_push_error(0, "SGI rgb: 16 bit RLE images not supported");
  i_img_destroy(img);
  return NULL;
}
