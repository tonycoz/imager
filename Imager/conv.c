#include "image.h"

/*
  General convolution for 2d decoupled filters
  end effects are acounted for by increasing
  scaling the result with the sum of used coefficients

  coeff: (float array) coefficients for filter
    len: length of filter.. number of coefficients
           note that this has to be an odd number
           (since the filter is even);
*/

void
i_conv(i_img *im,float *coeff,int len) {
  int i,l,c,ch,center;
  float pc;
  i_color rcolor;
  float res[11];
  i_img timg;

  mm_log((1,"i_conv(im* 0x%x,coeff 0x%x,len %d)\n",im,coeff,len));
 
  i_img_empty_ch(&timg,im->xsize,im->ysize,im->channels);

  center=(len-1)/2;


  for(l=0;l<im->ysize;l++) {
    for(i=0;i<im->xsize;i++) {
      pc=0.0;
      for(ch=0;ch<im->channels;ch++) res[ch]=0;
      for(c=0;c<len;c++)
	if (i_gpix(im,i+c-center,l,&rcolor)!=-1) {
	  for(ch=0;ch<im->channels;ch++) res[ch]+=(float)(rcolor.channel[ch])*coeff[c];
	  pc+=coeff[c];
	}
      for(ch=0;ch<im->channels;ch++) rcolor.channel[ch]=(unsigned char)(((res[ch]/pc>255.0)?255.0:res[ch]/pc));
      i_ppix(&timg,i,l,&rcolor);
    }
  }

  for(l=0;l<im->xsize;l++)
    {
      for(i=0;i<im->ysize;i++)
	{
	  pc=0.0;
	  for(ch=0;ch<im->channels;ch++) res[ch]=0;
	  for(c=0;c<len;c++)
	    if (i_gpix(&timg,l,i+c-center,&rcolor)!=-1)
	      {
		for(ch=0;ch<im->channels;ch++) res[ch]+=(float)(rcolor.channel[ch])*coeff[c];
		pc+=coeff[c];
	      }
	  for(ch=0;ch<im->channels;ch++) rcolor.channel[ch]=(unsigned char)(((res[ch]/(float)(pc)>255.0)?255.0:res[ch]/(float)(pc)));
	  i_ppix(im,l,i,&rcolor);
	}
    }
}









