#include "image.h"
#include "io.h"
#include "log.h"
#include "iolayer.h"

#include <stdlib.h>
#include <errno.h>


/*
=head1 NAME

tga.c - implements reading and writing targa files, uses io layer.

=head1 SYNOPSIS

   io_glue *ig = io_new_fd( fd );
   i_img *im   = i_readtga_wiol(ig, -1); // no limit on how much is read
   // or 
   io_glue *ig = io_new_fd( fd );
   return_code = i_writetga_wiol(im, ig); 

=head1 DESCRIPTION

tga.c implements the basic functions to read and write portable targa
files.  It uses the iolayer and needs either a seekable source or an
entire memory mapped buffer.

=head1 FUNCTION REFERENCE

Some of these functions are internal.

=over 4

=cut
*/




typedef struct {
  char  idlength;
  char  colourmaptype;
  char  datatypecode;
  short int colourmaporigin;
  short int colourmaplength;
  char  colourmapdepth;
  short int x_origin;
  short int y_origin;
  short width;
  short height;
  char  bitsperpixel;
  char  imagedescriptor;
} tga_header;


typedef struct {
  int compressed;
  int bytepp;
  enum { NoInit, Raw, Rle } state;
  unsigned char cval[4];
  int len;
  unsigned char hdr;
  io_glue *ig;
} tga_source;


static
int
bpp_to_bytes(unsigned int bpp) {
  switch (bpp) {
  case 8:
    return 1;
  case 15:
  case 16:
    return 2;
  case 24:
    return 3;
  case 32:
    return 4;
  }
  return 0;
}

static
int
bpp_to_channels(unsigned int bpp) {
  switch (bpp) {
  case 8:
    return 1;
  case 15:
    return 3;
  case 16:
    return 4;
  case 24:
    return 3;
  case 32:
    return 4;
  }
  return 0;
}



/* color_unpack

Unpacks bytes into colours, for 2 byte type the first byte coming from
the file will actually be GGGBBBBB, and the second will be ARRRRRGG.
"A" represents an attribute bit.  The 3 byte entry contains 1 byte
each of blue, green, and red.  The 4 byte entry contains 1 byte each
of blue, green, red, and attribute.
*/

static
void
color_unpack(unsigned char *buf, int bytepp, i_color *val) {
  switch (bytepp) {
  case 1:
    val->gray.gray_color = buf[0];
    break;
  case 2:
    val->rgba.r = (buf[1] & 0x7c) << 1;
    val->rgba.g = ((buf[1] & 0x03) << 6) | ((buf[0] & 0xe0) >> 2);
    val->rgba.b = (buf[0] & 0x1f) << 3;
    val->rgba.a = (buf[1] & 0x80);
    break;
  case 3:
    val->rgb.b = buf[0];
    val->rgb.g = buf[1];
    val->rgb.r = buf[2];
    break;
  case 4:
    val->rgba.b = buf[0];
    val->rgba.g = buf[1];
    val->rgba.r = buf[2];
    val->rgba.a = buf[3];
    break;
  default:
  }
}

/* 
   tga_source_read
   
   Does not byte reorder,  returns true on success, 0 otherwise 
*/

static
int
tga_source_read(tga_source *s, unsigned char *buf, size_t pixels) {
  int cp = 0, j, k;
  if (!s->compressed) {
    if (s->ig->readcb(s->ig, buf, pixels*s->bytepp) != pixels*s->bytepp) return 0;
    return 1;
  }
  
  while(cp < pixels) {
    int ml;
    if (s->len == 0) s->state = NoInit;
    switch (s->state) {
    case NoInit:
      if (s->ig->readcb(s->ig, &s->hdr, 1) != 1) return 0;

      s->len = (s->hdr &~(1<<7))+1;
      s->state = (s->hdr & (1<<7)) ? Rle : Raw;
      if (s->state == Rle && s->ig->readcb(s->ig, s->cval, s->bytepp) != s->bytepp) return 0;

      break;
    case Rle:
      ml = min(s->len, pixels-cp);
      for(k=0; k<ml; k++) for(j=0; j<s->bytepp; j++) 
	buf[(cp+k)*s->bytepp+j] = s->cval[j];
      //      memset(buf+cp, s->cidx, ml);
      cp     += ml;
      s->len -= ml;
      break;
    case Raw:
      ml = min(s->len, pixels-cp);
      if (s->ig->readcb(s->ig, buf+cp*s->bytepp, ml*s->bytepp) != ml*s->bytepp) return 0;
      cp     += ml;
      s->len -= ml;
      break;
    }
  }
  return 1;
}

static
int
tga_palette_read(io_glue *ig, i_img *img, int bytepp, int colourmaplength) {
  int i;
  size_t palbsize;
  unsigned char *palbuf;
  i_color val;

  palbsize = colourmaplength*bytepp;
  palbuf   = mymalloc(palbsize);
  
  if (ig->readcb(ig, palbuf, palbsize) != palbsize) {
    i_push_error(errno, "could not read targa colourmap");
    return 0;
  }
  
  /* populate the palette of the new image */
  for(i=0; i<colourmaplength; i++) {
    color_unpack(palbuf+i*bytepp, bytepp, &val);
    i_addcolors(img, &val, 1);
  }
  myfree(palbuf);
  return 1;
}





/*
=item i_readtga_wiol(ig, length)

Retrieve an image and stores in the iolayer object. Returns NULL on fatal error.

   ig     - io_glue object
   length - maximum length to read from data source, before closing it -1 
            signifies no limit.

=cut
*/

i_img *
i_readtga_wiol(io_glue *ig, int length) {
  i_img* img;
  int x, y, i;
  int width, height, channels;
  int mapped;
  char *idstring;

  tga_source src;
  tga_header header;
  unsigned char headbuf[18];
  unsigned char *databuf;
  unsigned char *reorderbuf;

  i_color *linebuf = NULL;
  i_clear_error();

  mm_log((1,"i_readtga(ig %p, length %d)\n", ig, length));
  
  io_glue_commit_types(ig);

  if (ig->readcb(ig, &headbuf, 18) != 18) {
    i_push_error(errno, "could not read targa header");
    return NULL;
  }

  header.idlength        = headbuf[0];
  header.colourmaptype   = headbuf[1];
  header.datatypecode    = headbuf[2];
  header.colourmaporigin = (headbuf[4] << 8) + headbuf[3];
  header.colourmaplength = (headbuf[6] << 8) + headbuf[5];
  header.colourmapdepth  = headbuf[7];
  header.x_origin        = (headbuf[9] << 8) + headbuf[8];
  header.y_origin        = (headbuf[11] << 8) + headbuf[10];
  header.width           = (headbuf[13] << 8) + headbuf[12];
  header.height          = (headbuf[15] << 8) + headbuf[14];
  header.bitsperpixel    = headbuf[16];
  header.imagedescriptor = headbuf[17];

  mm_log((1,"Id length:         %d\n",header.idlength));
  mm_log((1,"Colour map type:   %d\n",header.colourmaptype));
  mm_log((1,"Image type:        %d\n",header.datatypecode));
  mm_log((1,"Colour map offset: %d\n",header.colourmaporigin));
  mm_log((1,"Colour map length: %d\n",header.colourmaplength));
  mm_log((1,"Colour map depth:  %d\n",header.colourmapdepth));
  mm_log((1,"X origin:          %d\n",header.x_origin));
  mm_log((1,"Y origin:          %d\n",header.y_origin));
  mm_log((1,"Width:             %d\n",header.width));
  mm_log((1,"Height:            %d\n",header.height));
  mm_log((1,"Bits per pixel:    %d\n",header.bitsperpixel));
  mm_log((1,"Descriptor:        %d\n",header.imagedescriptor));


  if (header.idlength) {
    idstring = mymalloc(header.idlength+1);
    if (ig->readcb(ig, idstring, header.idlength) != header.idlength) {
      i_push_error(errno, "short read on targa idstring");
      return NULL;
    }
    myfree(idstring); /* Move this later, once this is stored in a tag */
  }

  width = header.width;
  height = header.height;
  
  /* Set tags here */
  
  switch (header.datatypecode) {
  case 0: /* No data in image */
    i_push_error(0, "Targa image contains no image data");
    return NULL;
    break;
  case 1:  /* Uncompressed, color-mapped images */
  case 9:  /* Compressed,   color-mapped images */
  case 3:  /* Uncompressed, grayscale images    */
  case 11: /* Compressed,   grayscale images    */
    if (header.bitsperpixel != 8) {
      i_push_error(0, "Targa: mapped/grayscale image's bpp is not 8, unsupported.");
      return NULL;
    }
    src.bytepp = 1;
    break;
  case 2:  /* Uncompressed, rgb images          */
  case 10: /* Compressed,   rgb images          */
    if ((src.bytepp = bpp_to_bytes(header.bitsperpixel)))
      break;
    i_push_error(0, "Targa: direct color image's bpp is not 15/16/24/32 - unsupported.");
    return NULL;
    break;
  case 32: /* Compressed color-mapped, Huffman, Delta and runlength */
  case 33: /* Compressed color-mapped, Huffman, Delta and runlength */
    i_push_error(0, "Unsupported Targa (Huffman/delta/rle/quadtree) subformat is not supported");
    return NULL;
    break;
  default: /* All others which we don't know which might be */
    i_push_error(0, "Unknown targa format");
    return NULL;
    break;
  }
  
  src.state = NoInit;
  src.len = 0;
  src.ig = ig;
  src.compressed = !!(header.datatypecode & (1<<3));

  /* Determine number of channels */
  
  mapped = 1;
  switch (header.datatypecode) {
    int tbpp;
  case 2:  /* Uncompressed, rgb images          */
  case 10: /* Compressed,   rgb images          */
    mapped = 0;
  case 1:  /* Uncompressed, color-mapped images */
  case 9:  /* Compressed,   color-mapped images */
    if ((channels = bpp_to_channels(mapped ? 
				   header.colourmapdepth : 
				   header.bitsperpixel))) break;
    i_push_error(0, "Targa Image has none of 15/16/24/32 pixel layout");
    return NULL;
    break;
  case 3:  /* Uncompressed, grayscale images    */
  case 11: /* Compressed,   grayscale images    */
    mapped = 0;
    channels = 1;
    break;
  }
  
  img = mapped ? 
    i_img_pal_new(width, height, channels, 256) :
    i_img_empty_ch(NULL, width, height, channels);
  
  if (mapped &&
      !tga_palette_read(ig,
			img,
			bpp_to_bytes(header.colourmapdepth),
			header.colourmaplength)
      ) {
    i_push_error(0, "Targa Image has none of 15/16/24/32 pixel layout");
    return NULL;
  }
  
  /* Allocate buffers */
  databuf = mymalloc(width*src.bytepp);
  if (!mapped) linebuf = mymalloc(width*sizeof(i_color));
  
  for(y=0; y<height; y++) {
    if (!tga_source_read(&src, databuf, width)) {
      i_push_error(errno, "read for targa data failed");
      myfree(databuf);
      return NULL;
    }
    if (mapped && header.colourmaporigin) for(x=0; x<width; x++) databuf[x] -= header.colourmaporigin;
    if (mapped) i_ppal(img, 0, width, header.imagedescriptor & (1<<5) ? y : height-1-y, databuf);
    else {
      for(x=0; x<width; x++) color_unpack(databuf+x*src.bytepp, src.bytepp, linebuf+x);
      i_plin(img, 0, width, header.imagedescriptor & (1<<5) ? y : height-1-y, linebuf);
    }
  }
  myfree(databuf);
  if (linebuf) myfree(linebuf);
  return img;
}









undef_int
i_writetga_wiol(i_img *img, io_glue *ig) {
  int rc;
  static int rgb_chan[] = { 2, 1, 0, 3 };
  tga_header header;
  unsigned char headbuf[18];
  unsigned char *data;
  int compress = 0;
  char *idstring = "testing";
  int idlen = strlen(idstring);
  int mapped = img->type == i_palette_type;

  mm_log((1,"i_writetga_wiol(img %p, ig %p)\n", img, ig));
  i_clear_error();

  io_glue_commit_types(ig);

  mm_log((1, "virtual %d, paletted %d\n", img->virtual, mapped));
  mm_log((1, "channels %d\n", img->channels));

  header.idlength;
  header.idlength = idlen;
  header.colourmaptype   = mapped ? 1 : 0;
  header.datatypecode    = mapped ? 1 : img->channels == 1 ? 3 : 2;
  mm_log((1, "datatypecode %d\n", header.datatypecode));
  header.datatypecode   += compress ? 8 : 0;
  header.colourmaporigin = 0;
  header.colourmaplength = mapped ? i_colorcount(img) : 0;
  header.colourmapdepth  = mapped ? img->channels*8 : 0;
  header.x_origin        = 0;
  header.y_origin        = 0;
  header.width           = img->xsize;
  header.height          = img->ysize;
  header.bitsperpixel    = mapped ? 8 : 8*img->channels;
  header.imagedescriptor = (1<<5); /* normal order instead of upside down */

  headbuf[0] = header.idlength;
  headbuf[1] = header.colourmaptype;
  headbuf[2] = header.datatypecode;
  headbuf[3] = header.colourmaporigin & 0xff;
  headbuf[4] = header.colourmaporigin >> 8;
  headbuf[5] = header.colourmaplength & 0xff;
  headbuf[6] = header.colourmaplength >> 8;
  headbuf[7] = header.colourmapdepth;
  headbuf[8] = header.x_origin & 0xff;
  headbuf[9] = header.x_origin >> 8;
  headbuf[10] = header.y_origin & 0xff;
  headbuf[11] = header.y_origin >> 8;
  headbuf[12] = header.width & 0xff;
  headbuf[13] = header.width >> 8;
  headbuf[14] = header.height & 0xff;
  headbuf[15] = header.height >> 8;
  headbuf[16] = header.bitsperpixel;
  headbuf[17] = header.imagedescriptor;

  if (ig->writecb(ig, &headbuf, sizeof(headbuf)) != sizeof(headbuf)) {
    i_push_error(errno, "could not write targa header");
    return 0;
  }

  if (idlen) {
    if (ig->writecb(ig, idstring, idlen) != idlen) {
      i_push_error(errno, "could not write targa idstring");
      return 0;
    }
  }
  
  if (img->type == i_palette_type) {
    int i;
    size_t palbsize = i_colorcount(img)*img->channels;
    unsigned char *palbuf = mymalloc(palbsize);

    for(i=0; i<header.colourmaplength; i++) {
      int ch;
      i_color val;
      i_getcolors(img, i, &val, 1);
      if (img->channels>1) for(ch=0; ch<img->channels; ch++) 
	palbuf[i*img->channels+ch] = val.channel[rgb_chan[ch]];
      else for(ch=0; ch<img->channels; ch++)
	palbuf[i] = val.channel[ch];
    }

    if (ig->writecb(ig, palbuf, palbsize) != palbsize) {
      i_push_error(errno, "could not write targa colourmap");
      return 0;
    }
    myfree(palbuf);

    /* write palette */
    if (!img->virtual) {
      if (ig->writecb(ig, img->idata, img->bytes) != img->bytes) {
	i_push_error(errno, "could not write targa image data");
	return 0;
      }
    } else {
      int y;
      i_palidx *vals = mymalloc(sizeof(i_palidx)*img->xsize);
      for(y=0; y<img->ysize; y++) {
	i_gpal(img, 0, img->xsize, y, vals);
	if (ig->writecb(ig, vals, img->xsize) != img->xsize) {
	  i_push_error(errno, "could not write targa data to file");
	  myfree(vals);
	  return 0;
	}
      }
      myfree(vals);
    }
  } else {
    int y, lsize = img->channels * img->xsize;
    data = mymalloc(lsize);
    for(y=0; y<img->ysize; y++) {
      if (img->channels>1) i_gsamp(img, 0, img->xsize, y, data, rgb_chan, img->channels);
      else {
	int gray_chan[] = {0};
	i_gsamp(img, 0, img->xsize, y, data, gray_chan, img->channels);
      }
      if ( ig->writecb(ig, data, lsize) != lsize ) {
	i_push_error(errno, "could not write targa data to file");
	myfree(data);
	return 0;
      }
    }
    myfree(data);
  }
  return 1;
}
