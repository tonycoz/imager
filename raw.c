#include "image.h"
#include <stdio.h>
#include "iolayer.h"
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>



/*

 Image loader for raw files.

 This is a barebones raw loader...

             fd: filedescriptor
              x: xsize
              y: ysize
   datachannels: the number of channels the file contains
  storechannels: the bitmap of channels we will read
          intrl: interlace flag,
                       0 = sample interleaving
                       1 = line interleaving
                       2 = image interleaving

*/

static
void
interleave(unsigned char *inbuffer,unsigned char *outbuffer,int rowsize,int channels) {
  int ch,ind,i;
  i=0;
  if (inbuffer == outbuffer) return; /* Check if data is already in interleaved format */
  for (ind=0; ind<rowsize; ind++) 
    for (ch=0; ch<channels; ch++) 
      outbuffer[i++] = inbuffer[rowsize*ch+ind]; 
}

static
void
expandchannels(unsigned char *inbuffer, unsigned char *outbuffer, 
	       int chunks, int datachannels, int storechannels) {
  int ch,i;
  if (inbuffer == outbuffer) return; /* Check if data is already in expanded format */
  for(ch=0; ch<chunks; ch++) 
    for (i=0; i<storechannels; i++) 
      outbuffer[ch*storechannels+i] = inbuffer[ch*datachannels+i];
}

i_img *
i_readraw_wiol(io_glue *ig, int x, int y, int datachannels, int storechannels, int intrl) {
  i_img* im;
  int rc,k;
  
  unsigned char *inbuffer;
  unsigned char *ilbuffer;
  unsigned char *exbuffer;
  
  int inbuflen,ilbuflen,exbuflen;

  io_glue_commit_types(ig);
  mm_log((1, "i_readraw(ig %p,x %d,y %d,datachannels %d,storechannels %d,intrl %d)\n",
	  ig, x, y, datachannels, storechannels, intrl));
  
  im = i_img_empty_ch(NULL,x,y,storechannels);
  if (!im)
    return NULL;
  
  inbuflen = im->xsize*datachannels;
  ilbuflen = inbuflen;
  exbuflen = im->xsize*storechannels;
  inbuffer = (unsigned char*)mymalloc(inbuflen);
  mm_log((1,"inbuflen: %d, ilbuflen: %d, exbuflen: %d.\n",inbuflen,ilbuflen,exbuflen));

  if (intrl==0) ilbuffer = inbuffer; 
  else ilbuffer=mymalloc(inbuflen);

  if (datachannels==storechannels) exbuffer=ilbuffer; 
  else exbuffer= mymalloc(exbuflen);
  
  k=0;
  while( k<im->ysize ) {
    rc = ig->readcb(ig, inbuffer, inbuflen);
    if (rc != inbuflen) { fprintf(stderr,"Premature end of file.\n"); exit(2); }
    interleave(inbuffer,ilbuffer,im->xsize,datachannels);
    expandchannels(ilbuffer,exbuffer,im->xsize,datachannels,storechannels);
    /* FIXME: Do we ever want to save to a virtual image? */
    memcpy(&(im->idata[im->xsize*storechannels*k]),exbuffer,exbuflen);
    k++;
  }

  myfree(inbuffer);
  if (intrl != 0) myfree(ilbuffer);
  if (datachannels != storechannels) myfree(exbuffer);

  i_tags_add(&im->tags, "i_format", 0, "raw", -1, 0);

  return im;
}



undef_int
i_writeraw_wiol(i_img* im, io_glue *ig) {
  int rc;

  io_glue_commit_types(ig);
  i_clear_error();
  mm_log((1,"writeraw(im %p,ig %p)\n", im, ig));
  
  if (im == NULL) { mm_log((1,"Image is empty\n")); return(0); }
  if (!im->virtual) {
    rc = ig->writecb(ig,im->idata,im->bytes);
    if (rc != im->bytes) { 
      i_push_error(errno, "Could not write to file");
      mm_log((1,"i_writeraw: Couldn't write to file\n")); 
      return(0);
    }
  } else {
    int y;
    
    if (im->type == i_direct_type) {
      /* just save it as 8-bits, maybe support saving higher bit count
         raw images later */
      int line_size = im->xsize * im->channels;
      unsigned char *data = mymalloc(line_size);

      int y = 0;
      rc = line_size;
      while (rc == line_size && y < im->ysize) {
	i_gsamp(im, 0, im->xsize, y, data, NULL, im->channels);
	rc = ig->writecb(ig, data, line_size);
	++y;
      }
      if (rc != line_size) {
        i_push_error(errno, "write error");
        return 0;
      }
      myfree(data);
    } else {
      /* paletted image - assumes the caller puts the palette somewhere 
         else
      */
      int line_size = sizeof(i_palidx) * im->xsize;
      i_palidx *data = mymalloc(sizeof(i_palidx) * im->xsize);

      int y = 0;
      rc = line_size;
      while (rc == line_size && y < im->ysize) {
	i_gpal(im, 0, im->xsize, y, data);
	rc = ig->writecb(ig, data, line_size);
	++y;
      }
      myfree(data);
      if (rc != line_size) {
        i_push_error(errno, "write error");
        return 0;
      }
    }
  }

  ig->closecb(ig);

  return(1);
}
