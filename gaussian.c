#include "imager.h"

static float
gauss(int x,float std) {
  return 1.0/(sqrt(2.0*PI)*std)*exp(-(float)(x)*(float)(x)/(2*std*std));
}

/* Counters are as follows
 l:  lines
 i:  columns
 c:  filter coeffs
 ch: channels
 pc: coeff equalization
*/



void
i_gaussian(i_img *im,float stdev) {
  int i,l,c,ch;
  float pc;
  float coeff[21];
  i_color rcolor;
  float res[11];
  i_img timg;

  mm_log((1,"i_gaussian(im %p, stdev %.2f)\n",im,stdev));
  
  i_img_empty_ch(&timg,im->xsize,im->ysize,im->channels);
	      
  for(i=0;i<11;i++) coeff[10+i]=coeff[10-i]=gauss(i,stdev);
  pc=0;
  for(i=0;i<21;i++) pc+=coeff[i];
  for(i=0;i<21;i++) coeff[i]/=pc;


  for(l=0;l<im->ysize;l++) {
    for(i=0;i<im->xsize;i++) {
      pc=0.0;
      for(ch=0;ch<im->channels;ch++) res[ch]=0; 
      for(c=0;c<21;c++)
	if (i_gpix(im,i+c-10,l,&rcolor)!=-1) {
	  for(ch=0;ch<im->channels;ch++) res[ch]+=(float)(rcolor.channel[ch])*coeff[c];
	  pc+=coeff[c];
	}
      for(ch=0;ch<im->channels;ch++) rcolor.channel[ch]=(unsigned char)(((res[ch]/(float)(pc)>255.0)?255.0:res[ch]/(float)(pc)));
      i_ppix(&timg,i,l,&rcolor);
    }
  }
  
  for(l=0;l<im->xsize;l++) {
    for(i=0;i<im->ysize;i++) {
      pc=0.0;
      for(ch=0;ch<im->channels;ch++) res[ch]=0; 
      for(c=0;c<21;c++)
	if (i_gpix(&timg,l,i+c-10,&rcolor)!=-1) {
	  for(ch=0;ch<im->channels;ch++) res[ch]+=(float)(rcolor.channel[ch])*coeff[c];
	  pc+=coeff[c];
	}
      for(ch=0;ch<im->channels;ch++) rcolor.channel[ch]=(unsigned char)(((res[ch]/(float)(pc)>255.0)?255.0:res[ch]/(float)(pc)));
      i_ppix(im,l,i,&rcolor);
    }
  }
  i_img_exorcise(&timg);
}







