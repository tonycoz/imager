#include "image.h"
#include "io.h"
#include "log.h"
#include "iolayer.h"

#include <stdlib.h>
#include <errno.h>


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

}








/*
=item i_readrgb_wiol(ig, length)

Read in an image from the iolayer data source and return the image structure to it.
Returns NULL on error.

   ig     - io_glue object
   length - maximum length to read from data source, before closing it -1 
            signifies no limit.

=cut
*/

i_img *
i_readrgb_wiol(io_glue *ig, int length) {
  i_img *img;
  int x, y, c,i;
  int width, height, channels;
  unsigned long maxlen;

  int savemask;
  
  char *idstring = NULL;

  rgb_header header;
  unsigned char headbuf[512];
  unsigned char *databuf;
  unsigned char *reorderbuf;
  unsigned long *starttab, *lengthtab;
  i_color *linebuf = NULL;
  i_mempool mp;

  mm_log((1,"i_readrgb(ig %p, length %d)\n", ig, length));
  i_clear_error();
  i_mempool_init(&mp);
  
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

  img = i_img_empty_ch(NULL, width, height, channels);
  
  i_tags_add(&img->tags, "rgb_namestr", 0, header.name, 80, 0);


  switch (header.storagetype) {
  case 0: /* uncompressed */
    
    linebuf   = i_mempool_alloc(&mp, width*sizeof(i_color));
    databuf   = i_mempool_alloc(&mp, width);

    savemask = i_img_getmask(img);

    for(c=0; c<channels; c++) {
      i_img_setmask(img, 1<<c);
      for(y=0; y<height; y++) {
	int x;
	
	if (ig->readcb(ig, databuf, width) != width) {
	  i_push_error(0, "SGI rgb: cannot read");
	  goto ErrorReturn;
	}

	for(x=0; x<width; x++)
	  linebuf[x].channel[c] = databuf[x];
	
	i_plin(img, 0, width, height-1-y, linebuf);
      }
    }
    i_img_setmask(img, savemask);
    break;
  case 1: /* RLE compressed */
    
    databuf   = i_mempool_alloc(&mp, height*channels*4);
    starttab  = i_mempool_alloc(&mp, height*channels*sizeof(unsigned long));
    lengthtab = i_mempool_alloc(&mp, height*channels*sizeof(unsigned long));
    linebuf   = i_mempool_alloc(&mp, width*sizeof(i_color));
    
    /* Read offset table */
    if (ig->readcb(ig, databuf, height*channels*4) != height*channels*4) goto ErrorReturn;
    for(i=0; i<height*channels; i++) starttab[i] = 
				       (databuf[i*4]<<24) | 
				       (databuf[i*4+1]<<16) | 
				       (databuf[i*4+2]<<8) |
				       (databuf[i*4+3]);


    /* Read length table */
    if (ig->readcb(ig, databuf, height*channels*4) != height*channels*4) goto ErrorReturn;
    for(i=0; i<height*channels; i++) lengthtab[i] = 
				       (databuf[i*4]<<24)+
				       (databuf[i*4+1]<<16)+
				       (databuf[i*4+2]<<8)+
				       (databuf[i*4+3]);

    mm_log((3, "Offset/length table:\n"));
    for(i=0; i<height*channels; i++)
      mm_log((3, "%d: %d/%d\n", i, starttab[i], lengthtab[i]));


    /* Find max spanlength if someone is making very badly formed RLE data */
    maxlen = 0;
    for(y=0; y<height; y++) maxlen = (maxlen>lengthtab[y])?maxlen:lengthtab[y];

    mm_log((1, "maxlen for an rle buffer: %d\n", maxlen));

    databuf = i_mempool_alloc(&mp, maxlen);

    for(y=0; y<height; y++) {
      for(c=0; c<channels; c++) {
	unsigned long iidx = 0, oidx = 0, span = 0;
	unsigned char cval;
	int rle = 0;
	int ci = height*c+y;
	int datalen = lengthtab[ci];

	if (ig->seekcb(ig, starttab[ci], SEEK_SET) != starttab[ci]) {
	  i_push_error(0, "SGI rgb: cannot seek");
	  goto ErrorReturn;
	}
	if (ig->readcb(ig, databuf, datalen) != datalen) {
	  i_push_error(0, "SGI rgb: cannot read");
	  goto ErrorReturn;
	}

	/*
	  mm_log((1, "Buffer length %d\n", datalen));
	  for(i=0; i<datalen; i++) 
	  mm_log((1, "0x%x\n", databuf[i]));
	*/

	while( iidx <= datalen && oidx < width ) {
	  if (!span) {
	    span = databuf[iidx] & 0x7f;
	    rle  = !(databuf[iidx++] & 0x80);
	    /*	    mm_log((1,"new span %d, rle %d\n", span, rle)); */
	    if (rle) {
	      if (iidx==datalen) {
		i_push_error(0, "SGI rgb: bad rle data");
		goto ErrorReturn;
	      }
	      cval = databuf[iidx++];
	      /* mm_log((1, "rle value %d\n", cval)); */
	    }
	  }
	  linebuf[oidx++].channel[c] = rle ? cval : databuf[iidx++];
	  span--;
	  /*
	    mm_log((1,"iidx=%d/%d, oidx=%d/%d, linebuf[%d].channel[%d] %d\n", iidx-1, datalen, oidx-1, width, oidx-1, c, linebuf[oidx-1].channel[c]));
	  */
	}
      }
      i_plin(img, 0, width, height-1-y, linebuf);
    }
    
    break;
  }

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
  



}

