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


#define BSIZ 1024



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
  int type;
  int y, i;
  int width, height, channels;
  char *idstring;
  unsigned char *uc;
  i_color val;

  tga_header header;
  size_t palbsize;
  unsigned char *palbuf;
  unsigned char headbuf[18];
  unsigned char *linebuf;

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
  header.colourmaporigin = headbuf[4] << 8 + headbuf[3];
  header.colourmaplength = headbuf[6] << 8 + headbuf[5];
  header.colourmapdepth  = headbuf[7];
  header.x_origin        = headbuf[9] << 8 + headbuf[8];
  header.y_origin        = headbuf[11] << 8 + headbuf[10];
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

  /* Only RGB for now, eventually support all formats */
  switch (header.datatypecode) {

  case 0: /* No data in image */
    i_push_error(0, "Targa image contains no image data");
    return NULL;
    break;

  case 1: /* Uncompressed, color-mapped images */
    if (header.imagedescriptor != 0) {
      i_push_error(0, "Targa: Imagedescriptor not 0, not supported internal format");
      return NULL;
    }
    
    if (header.bitsperpixel != 8) {
      i_push_error(0, "Targa: bpp is not 8, unsupported.");
      return NULL;
    }
    
    mm_log((1, "Uncompressed color-mapped image\n"));
    if (header.colourmapdepth != 24) {
      i_push_error(0, "Colourmap is not 24 bit");
      return NULL;
    }
    channels = 3;

    img = i_img_pal_new(width, height, channels, 256);

    /* Read in the palette */

    palbsize = header.colourmaplength*channels;
    palbuf = mymalloc(palbsize);
    
    if (ig->readcb(ig, palbuf, palbsize) != palbsize) {
      i_push_error(errno, "could not read targa colourmap");
      return NULL;
    }
    
    /* populate the palette of the new image */
    for(i=0; i<header.colourmaplength; i++) {
      i_color col;
      col.rgba.r = palbuf[i*channels+2];
      col.rgba.g = palbuf[i*channels+1];
      col.rgba.b = palbuf[i*channels+0];
      i_addcolors(img, &col, 1);
    }
    myfree(palbuf);


    /* read image data in */
    linebuf = mymalloc(width);
    for(y=height-1;y>=0;y--) {
      if (ig->readcb(ig, linebuf, width) != width) {
	i_push_error(errno, "read for targa data failed");
	myfree(linebuf);
	return NULL;
      }
      i_ppal(img, 0, width, y, linebuf);
    }
    
    return img;
    break;
    
  case 2:  /* Uncompressed, RGB images */
    mm_log((1, "Uncompressed RGB image\n"));
    channels = 3;
    /* Do stuff */
    break;
  case 3: /* Uncompressed, black and white images */
    i_push_error(0, "Targa black and white subformat is not supported");
    return NULL;
    break;
  case 9: /* Runlength encoded, color-mapped images */
    i_push_error(0, "Targa runlength coded colormapped subformat is not supported");
    return NULL;
    break;
  case 10: /* Runlength encoded, RGB images */
    channels = 3;
    /* Do stuff */
    break;
  case 11: /* Compressed, black and white images */
    i_push_error(0, "Targa compressed black and white subformat is not supported");
    return NULL;
    break;
  case 32: /* Compressed color-mapped, Huffman, Delta and runlength */
    i_push_error(0, "Targa Huffman/delta/rle subformat is not supported");
    return NULL;
    break;
  case 33: /* Compressed color-mapped, Huffman, Delta and runlength */
    i_push_error(0, "Targa Huffman/delta/rle/quadtree subformat is not supported");
    return NULL;
    break;
  default: /* All others which we don't know which might be */
    i_push_error(0, "Unknown targa format");
    return NULL;
    break;
  }
  return NULL;
}









undef_int
i_writetga_wiol(i_img *im, io_glue *ig) {
  char header[255];
  int rc;
  writep write_func;

  mm_log((1,"i_writeppm(im %p, ig %p)\n", im, ig));
  i_clear_error();

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */

  io_glue_commit_types(ig);

  if (im->channels == 3) {
    sprintf(header,"P6\n#CREATOR: Imager\n%d %d\n255\n",im->xsize,im->ysize);
    if (ig->writecb(ig,header,strlen(header))<0) {
      i_push_error(errno, "could not write ppm header");
      mm_log((1,"i_writeppm: unable to write ppm header.\n"));
      return(0);
    }

    if (!im->virtual && im->bits == i_8_bits && im->type == i_direct_type) {
      rc = ig->writecb(ig,im->idata,im->bytes);
    }
    else {
      unsigned char *data = mymalloc(3 * im->xsize);
      if (data != NULL) {
        int y = 0;
        int x, ch;
        unsigned char *p;
        static int rgb_chan[3] = { 0, 1, 2 };

        rc = 0;
        while (y < im->ysize && rc >= 0) {
          i_gsamp(im, 0, im->xsize, y, data, rgb_chan, 3);
          rc = ig->writecb(ig, data, im->xsize * 3);
        }
        myfree(data);
      }
      else {
        i_push_error(0, "Out of memory");
        return 0;
      }
    }
    if (rc<0) {
      i_push_error(errno, "could not write ppm data");
      mm_log((1,"i_writeppm: unable to write ppm data.\n"));
      return(0);
    }
  }
  else if (im->channels == 1) {
    sprintf(header, "P5\n#CREATOR: Imager\n%d %d\n255\n",
	    im->xsize, im->ysize);
    if (ig->writecb(ig,header, strlen(header)) < 0) {
      i_push_error(errno, "could not write pgm header");
      mm_log((1,"i_writeppm: unable to write pgm header.\n"));
      return(0);
    }

    if (!im->virtual && im->bits == i_8_bits && im->type == i_direct_type) {
      rc=ig->writecb(ig,im->idata,im->bytes);
    }
    else {
      unsigned char *data = mymalloc(im->xsize);
      if (data != NULL) {
        int y = 0;
        int x, ch;
        int chan = 0;
        unsigned char *p;

        rc = 0;
        while (y < im->ysize && rc >= 0) {
          i_gsamp(im, 0, im->xsize, y, data, &chan, 1);
          rc = ig->writecb(ig, data, im->xsize);
        }
        myfree(data);
      }
      else {
        i_push_error(0, "Out of memory");
        return 0;
      }
    }
    if (rc<0) {
      i_push_error(errno, "could not write pgm data");
      mm_log((1,"i_writeppm: unable to write pgm data.\n"));
      return(0);
    }
  }
  else {
    i_push_error(0, "can only save 1 or 3 channel images to pnm");
    mm_log((1,"i_writeppm: ppm/pgm is 1 or 3 channel only (current image is %d)\n",im->channels));
    return(0);
  }

  return(1);
}
