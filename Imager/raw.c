#include "image.h"
#include <stdio.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <string.h>

#define TRUE 1
#define FALSE 0

#define BSIZ 100*BUFSIZ

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

void
expandchannels(unsigned char *inbuffer,unsigned char *outbuffer,int chunks,int datachannels,int storechannels) {
  int ch,i;
  if (inbuffer==outbuffer) return; /* Check if data is already in expanded format */
  for(ch=0;ch<chunks;ch++) for (i=0;i<storechannels;i++) outbuffer[ch*storechannels+i]=inbuffer[ch*datachannels+i];
}

i_img *
i_readraw(int fd,int x,int y,int datachannels,int storechannels,int intrl) {
  i_img* im;
  int rc,k;
  
  unsigned char *inbuffer;
  unsigned char *ilbuffer;
  unsigned char *exbuffer;
  
  int inbuflen,ilbuflen,exbuflen;

  mm_log((1,"readraw(fd %d,x %d,y %d,datachannels %d,storechannels %d,intrl %d)\n",fd,x,y,datachannels,storechannels,intrl));
  
  im=i_img_empty_ch(NULL,x,y,storechannels);
  
  inbuflen=im->xsize*datachannels;
  ilbuflen=inbuflen;
  exbuflen=im->xsize*storechannels;
  inbuffer=(unsigned char*)mymalloc(inbuflen);
  mm_log((1,"inbuflen: %d, ilbuflen: %d, exbuflen: %d.\n",inbuflen,ilbuflen,exbuflen));

  if (intrl==0) ilbuffer=inbuffer; else ilbuffer=(unsigned char*)mymalloc(inbuflen);
  if (datachannels==storechannels) exbuffer=ilbuffer; else exbuffer=(unsigned char*)mymalloc(exbuflen);

  k=0;
  while(k<im->ysize) {
    rc=myread(fd,inbuffer,inbuflen);
    if (rc!=inbuflen) { fprintf(stderr,"Premature end of file.\n"); exit(2); }
    interleave(inbuffer,ilbuffer,im->xsize,datachannels);
    expandchannels(ilbuffer,exbuffer,im->xsize,datachannels,storechannels);
    memcpy(&(im->data[im->xsize*storechannels*k]),exbuffer,exbuflen);
    k++;
  }

  myfree(inbuffer);
  if (intrl!=0) myfree(ilbuffer);
  if (datachannels!=storechannels) myfree(exbuffer);
  return im;
}



undef_int
i_writeraw(i_img* im,int fd) {
  int rc;
  mm_log((1,"writeraw(im 0x%x,fd %d)\n",im,fd));
  
  if (im == NULL) { mm_log((1,"Image is empty\n")); return(0); }
  rc=mywrite(fd,im->data,im->bytes);
  if (rc!=im->bytes) { mm_log((1,"i_writeraw: Couldn't write to file\n")); return(0); }
  return(1);
}









