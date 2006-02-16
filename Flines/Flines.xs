#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
#ifdef __cplusplus
}
#endif

#include "imext.h"
#include "imperl.h"

unsigned char
static
saturate(int in) {
  if (in>255) { return 255; }
  else if (in>0) return in;
  return 0;
}



void
flines(i_img *im) {
  i_color vl;
  int x,y;
  
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


DEFINE_IMAGER_CALLBACKS;

MODULE = Imager::Filter::Flines   PACKAGE = Imager::Filter::Flines

void
flines(im)
        Imager::ImgRaw im

BOOT:
        PERL_INITIALIZE_IMAGER_CALLBACKS;
