#include "pluginst.h"


char evalstr[]="Fancy lines";

/* input parameters
   image is the image object.
*/



unsigned char
static
saturate(int in) {
  if (in>255) { return 255; }
  else if (in>0) return in;
  return 0;
}



void
flines(void *INP) {
  i_img *im;
  i_color vl;
  int x,y;
  
  if ( !getOBJ("image","Imager::ImgRaw",&im) ) {
		fprintf(stderr,"Error: image is missing\n"); 
		return;
	}
  
  fprintf(stderr, "flines: parameters: (im %p)\n",im);
  fprintf(stderr, "flines: image info:\n size (%d,%d)\n channels (%d)\n",
	  im->xsize,im->ysize,im->channels);

  for(y = 0; y < im->ysize; y ++) {
    for(x = 0; x < im->xsize; x ++ ) {
      i_gpix(im,x,y,&vl); 
			if (!(y%2)) {
				float yf = y/(float)im->ysize;
				float mf = 1.2-0.8*yf;
				vl.rgb.r = saturate(vl.rgb.r*mf);
				vl.rgb.g = saturate(vl.rgb.g*mf);
				vl.rgb.b = saturate(vl.rgb.b*mf);
			} else {
				float yf = (im->ysize-y)/(float)im->ysize;
				float mf = 1.2-0.8*yf;
				vl.rgb.r = saturate(vl.rgb.r*mf);
				vl.rgb.g = saturate(vl.rgb.g*mf);
				vl.rgb.b = saturate(vl.rgb.b*mf);
			} 
     i_ppix(im,x,y,&vl); 
    }
  }
}



func_ptr function_list[]={
  {
    "flines",
    flines,
    "callseq => ['image'], \
    callsub => sub { my %hsh=@_; DSO_call($DSO_handle,0,\\%hsh); } \
    "
  },
  {NULL,NULL,NULL}};


/* Remember to double backslash backslashes within Double quotes in C */

