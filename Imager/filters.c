#include "image.h"
#include <stdlib.h>
#include <math.h>


/*
=head1 NAME

filters.c - implements filters that operate on images

=head1 SYNOPSIS

  
  i_contrast(im, 0.8);
  i_hardinvert(im);
  // and more

=head1 DESCRIPTION

filters.c implements basic filters for Imager.  These filters
should be accessible from the filter interface as defined in 
the pod for Imager.

=head1 FUNCTION REFERENCE

Some of these functions are internal.

=over 4

=cut
*/







/* 
=item i_contrast(im, intensity)

Scales the pixel values by the amount specified.

  im        - image object
  intensity - scalefactor

=cut
*/

void
i_contrast(i_img *im, float intensity) {
  int x, y;
  unsigned char ch;
  unsigned int new_color;
  i_color rcolor;
  
  mm_log((1,"i_contrast(im %p, intensity %f)\n", im, intensity));
  
  if(intensity < 0) return;
  
  for(y = 0; y < im->ysize; y++) for(x = 0; x < im->xsize; x++) {
    i_gpix(im, x, y, &rcolor);
      
    for(ch = 0; ch < im->channels; ch++) {
      new_color = (unsigned int) rcolor.channel[ch];
      new_color *= intensity;
	
      if(new_color > 255) {
	new_color = 255;
      }
      rcolor.channel[ch] = (unsigned char) new_color;
    }
    i_ppix(im, x, y, &rcolor);
  }
}


/* 
=item i_hardinvert(im)

Inverts the pixel values of the input image.

  im        - image object

=cut
*/

void
i_hardinvert(i_img *im) {
  int x, y;
  unsigned char ch;
  
  i_color rcolor;
  
    mm_log((1,"i_hardinvert(im %p)\n", im));

  for(y = 0; y < im->ysize; y++) {
    for(x = 0; x < im->xsize; x++) {
      i_gpix(im, x, y, &rcolor);
      
      for(ch = 0; ch < im->channels; ch++) {
	rcolor.channel[ch] = 255 - rcolor.channel[ch];
      }
      
      i_ppix(im, x, y, &rcolor);
    }
  }  
}



/*
=item i_noise(im, amount, type)

Inverts the pixel values by the amount specified.

  im     - image object
  amount - deviation in pixel values
  type   - noise individual for each channel if true

=cut
*/

#ifdef _MSC_VER
/* random() is non-ASCII, even if it is better than rand() */
#define random() rand()
#endif

void
i_noise(i_img *im, float amount, unsigned char type) {
  int x, y;
  unsigned char ch;
  int new_color;
  float damount = amount * 2;
  i_color rcolor;
  int color_inc = 0;
  
  mm_log((1,"i_noise(im %p, intensity %.2f\n", im, amount));
  
  if(amount < 0) return;
  
  for(y = 0; y < im->ysize; y++) for(x = 0; x < im->xsize; x++) {
    i_gpix(im, x, y, &rcolor);
    
    if(type == 0) {
      color_inc = (amount - (damount * ((float)random() / RAND_MAX)));
    }
    
    for(ch = 0; ch < im->channels; ch++) {
      new_color = (int) rcolor.channel[ch];
      
      if(type != 0) {
	new_color += (amount - (damount * ((float)random() / RAND_MAX)));
      } else {
	new_color += color_inc;
      }
      
      if(new_color < 0) {
	new_color = 0;
      }
      if(new_color > 255) {
	new_color = 255;
      }
      
      rcolor.channel[ch] = (unsigned char) new_color;
    }
    
    i_ppix(im, x, y, &rcolor);
  }
}


/* 
=item i_noise(im, amount, type)

Inverts the pixel values by the amount specified.

  im     - image object
  amount - deviation in pixel values
  type   - noise individual for each channel if true

=cut
*/


/*
=item i_applyimage(im, add_im, mode)

Apply's an image to another image

  im     - target image
  add_im - image that is applied to target
  mode   - what method is used in applying:

  0   Normal	
  1   Multiply
  2   Screen
  3   Overlay
  4   Soft Light
  5   Hard Light
  6   Color dodge
  7   Color Burn
  8   Darker
  9   Lighter
  10  Add
  11  Subtract
  12  Difference
  13  Exclusion
	
=cut
*/

void i_applyimage(i_img *im, i_img *add_im, unsigned char mode) {
  int x, y;
  int mx, my;

  mm_log((1, "i_applyimage(im %p, add_im %p, mode %d", im, add_im, mode));
  
  mx = (add_im->xsize <= im->xsize) ? add_im->xsize : add_im->xsize;
  my = (add_im->ysize <= im->ysize) ? add_im->ysize : add_im->ysize;
  
  for(x = 0; x < mx; x++) {		
    for(y = 0; y < my; y++) {
    }
  }
}


/* 
=item i_bumpmap(im, bump, channel, light_x, light_y, st)

Makes a bumpmap on image im using the bump image as the elevation map.

  im      - target image
  bump    - image that contains the elevation info
  channel - to take the elevation information from
  light_x - x coordinate of light source
  light_y - y coordinate of light source
  st      - length of shadow

=cut
*/

void
i_bumpmap(i_img *im, i_img *bump, int channel, int light_x, int light_y, int st) {
  int x, y, ch;
  int mx, my;
  i_color x1_color, y1_color, x2_color, y2_color, dst_color;
  double nX, nY;
  double tX, tY, tZ;
  double aX, aY, aL;
  double fZ;
  unsigned char px1, px2, py1, py2;
  
  i_img new_im;

  mm_log((1, "i_bumpmap(im %p, add_im %p, channel %d, light_x %d, light_y %d, st %d)\n",
	  im, bump, channel, light_x, light_y, st));


  if(channel >= bump->channels) {
    mm_log((1, "i_bumpmap: channel = %d while bump image only has %d channels\n", channel, bump->channels));
    return;
  }
  
  mx = (bump->xsize <= im->xsize) ? bump->xsize : im->xsize;
  my = (bump->ysize <= im->ysize) ? bump->ysize : im->ysize;

  i_img_empty_ch(&new_im, im->xsize, im->ysize, im->channels);
 
  aX = (light_x > (mx >> 1)) ? light_x : mx - light_x;
  aY = (light_y > (my >> 1)) ? light_y : my - light_y;

  aL = sqrt((aX * aX) + (aY * aY));

  for(y = 1; y < my - 1; y++) {		
    for(x = 1; x < mx - 1; x++) {
      i_gpix(bump, x + st, y, &x1_color);
      i_gpix(bump, x, y + st, &y1_color);
      i_gpix(bump, x - st, y, &x2_color);
      i_gpix(bump, x, y - st, &y2_color);

      i_gpix(im, x, y, &dst_color);

      px1 = x1_color.channel[channel];
      py1 = y1_color.channel[channel];
      px2 = x2_color.channel[channel];
      py2 = y2_color.channel[channel];

      nX = px1 - px2;
      nY = py1 - py2;

      nX += 128;
      nY += 128;

      fZ = (sqrt((nX * nX) + (nY * nY)) / aL);
 
      tX = abs(x - light_x) / aL;
      tY = abs(y - light_y) / aL;

      tZ = 1 - (sqrt((tX * tX) + (tY * tY)) * fZ);
      
      if(tZ < 0) tZ = 0;
      if(tZ > 2) tZ = 2;

      for(ch = 0; ch < im->channels; ch++)
	dst_color.channel[ch] = (unsigned char) (float)(dst_color.channel[ch] * tZ);
      
      i_ppix(&new_im, x, y, &dst_color);
    }
  }

  i_copyto(im, &new_im, 0, 0, (int)im->xsize, (int)im->ysize, 0, 0);
  
  i_img_exorcise(&new_im);
}



/* 
=item i_postlevels(im, levels)

Quantizes Images to fewer levels.

  im      - target image
  levels  - number of levels

=cut
*/

void
i_postlevels(i_img *im, int levels) {
  int x, y, ch;
  float pv;
  int rv;
  float av;

  i_color rcolor;

  rv = (int) ((float)(256 / levels));
  av = (float)levels;

  for(y = 0; y < im->ysize; y++) for(x = 0; x < im->xsize; x++) {
    i_gpix(im, x, y, &rcolor);

    for(ch = 0; ch < im->channels; ch++) {
      pv = (((float)rcolor.channel[ch] / 255)) * av;
      pv = (int) ((int)pv * rv);

      if(pv < 0) pv = 0;
      else if(pv > 255) pv = 255;

      rcolor.channel[ch] = (unsigned char) pv;
    }
    i_ppix(im, x, y, &rcolor);
  }
}


/* 
=item i_mosaic(im, size)

Makes an image looks like a mosaic with tilesize of size

  im      - target image
  size    - size of tiles

=cut
*/

void
i_mosaic(i_img *im, int size) {
  int x, y, ch;
  int lx, ly, z;
  long sqrsize;

  i_color rcolor;
  long col[256];
  
  sqrsize = size * size;
  
  for(y = 0; y < im->ysize; y += size) for(x = 0; x < im->xsize; x += size) {
    for(z = 0; z < 256; z++) col[z] = 0;
    
    for(lx = 0; lx < size; lx++) {
      for(ly = 0; ly < size; ly++) {
	i_gpix(im, (x + lx), (y + ly), &rcolor);
	  
	for(ch = 0; ch < im->channels; ch++) {
	  col[ch] += rcolor.channel[ch];
	}
      }
    }
      
    for(ch = 0; ch < im->channels; ch++)
      rcolor.channel[ch] = (int) ((float)col[ch] / sqrsize);
    
    
    for(lx = 0; lx < size; lx++)
      for(ly = 0; ly < size; ly++)
      i_ppix(im, (x + lx), (y + ly), &rcolor);
    
  }
}

/*
=item saturate(in) 

Clamps the input value between 0 and 255. (internal)

  in - input integer

=cut
*/

static
unsigned char
saturate(int in) {
  if (in>255) { return 255; }
  else if (in>0) return in;
  return 0;
}


/*
=item i_watermark(im, wmark, tx, ty, pixdiff) 

Applies a watermark to the target image

  im      - target image
  wmark   - watermark image
  tx      - x coordinate of where watermark should be applied
  ty      - y coordinate of where watermark should be applied
  pixdiff - the magnitude of the watermark, controls how visible it is

=cut
*/

void
i_watermark(i_img *im, i_img *wmark, int tx, int ty, int pixdiff) {
  int vx, vy, ch;
  i_color val, wval;

  for(vx=0;vx<128;vx++) for(vy=0;vy<110;vy++) {
    
    i_gpix(im,    tx+vx, ty+vy,&val );
    i_gpix(wmark, vx,    vy,   &wval);
    
    for(ch=0;ch<im->channels;ch++) 
      val.channel[ch] = saturate( val.channel[ch] + (pixdiff* (wval.channel[0]-128) )/128 );
    
    i_ppix(im,tx+vx,ty+vy,&val);
  }
}


/*
=item i_autolevels(im, lsat, usat, skew)

Scales and translates each color such that it fills the range completely.
Skew is not implemented yet - purpose is to control the color skew that can
occur when changing the contrast.

  im   - target image
  lsat - fraction of pixels that will be truncated at the lower end of the spectrum
  usat - fraction of pixels that will be truncated at the higher end of the spectrum
  skew - not used yet

=cut
*/

void
i_autolevels(i_img *im, float lsat, float usat, float skew) {
  i_color val;
  int i, x, y, rhist[256], ghist[256], bhist[256];
  int rsum, rmin, rmax;
  int gsum, gmin, gmax;
  int bsum, bmin, bmax;
  int rcl, rcu, gcl, gcu, bcl, bcu;

  mm_log((1,"i_autolevels(im %p, lsat %f,usat %f,skew %f)\n", im, lsat,usat,skew));

  rsum=gsum=bsum=0;
  for(i=0;i<256;i++) rhist[i]=ghist[i]=bhist[i] = 0;
  /* create histogram for each channel */
  for(y = 0; y < im->ysize; y++) for(x = 0; x < im->xsize; x++) {
    i_gpix(im, x, y, &val);
    rhist[val.channel[0]]++;
    ghist[val.channel[1]]++;
    bhist[val.channel[2]]++;
  }

  for(i=0;i<256;i++) {
    rsum+=rhist[i];
    gsum+=ghist[i];
    bsum+=bhist[i];
  }
  
  rmin = gmin = bmin = 0;
  rmax = gmax = bmax = 255;
  
  rcu = rcl = gcu = gcl = bcu = bcl = 0;
  
  for(i=0; i<256; i++) { 
    rcl += rhist[i];     if ( (rcl<rsum*lsat) ) rmin=i;
    rcu += rhist[255-i]; if ( (rcu<rsum*usat) ) rmax=255-i;

    gcl += ghist[i];     if ( (gcl<gsum*lsat) ) gmin=i;
    gcu += ghist[255-i]; if ( (gcu<gsum*usat) ) gmax=255-i;

    bcl += bhist[i];     if ( (bcl<bsum*lsat) ) bmin=i;
    bcu += bhist[255-i]; if ( (bcu<bsum*usat) ) bmax=255-i;
  }

  for(y = 0; y < im->ysize; y++) for(x = 0; x < im->xsize; x++) {
    i_gpix(im, x, y, &val);
    val.channel[0]=saturate((val.channel[0]-rmin)*255/(rmax-rmin));
    val.channel[1]=saturate((val.channel[1]-gmin)*255/(gmax-gmin));
    val.channel[2]=saturate((val.channel[2]-bmin)*255/(bmax-bmin));
    i_ppix(im, x, y, &val);
  }
}

/*
=item Noise(x,y)

Pseudo noise utility function used to generate perlin noise. (internal)

  x - x coordinate
  y - y coordinate

=cut
*/

static
float
Noise(int x, int y) {
  int n = x + y * 57; 
  n = (n<<13) ^ n;
  return ( 1.0 - ( (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);
}

/*
=item SmoothedNoise1(x,y)

Pseudo noise utility function used to generate perlin noise. (internal)

  x - x coordinate
  y - y coordinate

=cut
*/

static
float
SmoothedNoise1(float x, float y) {
  float corners = ( Noise(x-1, y-1)+Noise(x+1, y-1)+Noise(x-1, y+1)+Noise(x+1, y+1) ) / 16;
  float sides   = ( Noise(x-1, y)  +Noise(x+1, y)  +Noise(x, y-1)  +Noise(x, y+1) ) /  8;
  float center  =  Noise(x, y) / 4;
  return corners + sides + center;
}


/*
=item G_Interpolate(a, b, x)

Utility function used to generate perlin noise. (internal)

=cut
*/

static
float C_Interpolate(float a, float b, float x) {
  /*  float ft = x * 3.1415927; */
  float ft = x * PI;
  float f = (1 - cos(ft)) * .5;
  return  a*(1-f) + b*f;
}


/*
=item InterpolatedNoise(x, y)

Utility function used to generate perlin noise. (internal)

=cut
*/

static
float
InterpolatedNoise(float x, float y) {

  int integer_X = x;
  float fractional_X = x - integer_X;
  int integer_Y = y;
  float fractional_Y = y - integer_Y;

  float v1 = SmoothedNoise1(integer_X,     integer_Y);
  float v2 = SmoothedNoise1(integer_X + 1, integer_Y);
  float v3 = SmoothedNoise1(integer_X,     integer_Y + 1);
  float v4 = SmoothedNoise1(integer_X + 1, integer_Y + 1);

  float i1 = C_Interpolate(v1 , v2 , fractional_X);
  float i2 = C_Interpolate(v3 , v4 , fractional_X);

  return C_Interpolate(i1 , i2 , fractional_Y);
}



/*
=item PerlinNoise_2D(x, y)

Utility function used to generate perlin noise. (internal)

=cut
*/

static
float
PerlinNoise_2D(float x, float y) {
  int i,frequency;
  float amplitude;
  float total = 0;
  int Number_Of_Octaves=6;
  int n = Number_Of_Octaves - 1;

  for(i=0;i<n;i++) {
    frequency = 2*i;
    amplitude = PI;
    total = total + InterpolatedNoise(x * frequency, y * frequency) * amplitude;
  }

  return total;
}


/*
=item i_radnoise(im, xo, yo, rscale, ascale)

Perlin-like radial noise.

  im     - target image
  xo     - x coordinate of center
  yo     - y coordinate of center
  rscale - radial scale
  ascale - angular scale

=cut
*/

void
i_radnoise(i_img *im, int xo, int yo, float rscale, float ascale) {
  int x, y, ch;
  i_color val;
  unsigned char v;
  float xc, yc, r;
  double a;
  
  for(y = 0; y < im->ysize; y++) for(x = 0; x < im->xsize; x++) {
    xc = (float)x-xo+0.5;
    yc = (float)y-yo+0.5;
    r = rscale*sqrt(xc*xc+yc*yc)+1.2;
    a = (PI+atan2(yc,xc))*ascale;
    v = saturate(128+100*(PerlinNoise_2D(a,r)));
    /* v=saturate(120+12*PerlinNoise_2D(xo+(float)x/scale,yo+(float)y/scale));  Good soft marble */ 
    for(ch=0; ch<im->channels; ch++) val.channel[ch]=v;
    i_ppix(im, x, y, &val);
  }
}


/*
=item i_turbnoise(im, xo, yo, scale)

Perlin-like 2d noise noise.

  im     - target image
  xo     - x coordinate translation
  yo     - y coordinate translation
  scale  - scale of noise

=cut
*/

void
i_turbnoise(i_img *im, float xo, float yo, float scale) {
  int x,y,ch;
  unsigned char v;
  i_color val;

  for(y = 0; y < im->ysize; y++) for(x = 0; x < im->xsize; x++) {
    /*    v=saturate(125*(1.0+PerlinNoise_2D(xo+(float)x/scale,yo+(float)y/scale))); */
    v = saturate(120*(1.0+sin(xo+(float)x/scale+PerlinNoise_2D(xo+(float)x/scale,yo+(float)y/scale))));
    for(ch=0; ch<im->channels; ch++) val.channel[ch] = v;
    i_ppix(im, x, y, &val);
  }
}



/*
=item i_gradgen(im, num, xo, yo, ival, dmeasure)

Gradient generating function.

  im     - target image
  num    - number of points given
  xo     - array of x coordinates
  yo     - array of y coordinates
  ival   - array of i_color objects
  dmeasure - distance measure to be used.  
    0 = Euclidean
    1 = Euclidean squared
    2 = Manhattan distance

=cut
*/


void
i_gradgen(i_img *im, int num, int *xo, int *yo, i_color *ival, int dmeasure) {
  
  i_color val;
  int p, x, y, ch;
  int channels = im->channels;
  int xsize    = im->xsize;
  int ysize    = im->ysize;

  float *fdist;

  mm_log((1,"i_gradgen(im %p, num %d, xo %p, yo %p, ival %p, dmeasure %d)\n", im, num, xo, yo, ival, dmeasure));
  
  for(p = 0; p<num; p++) {
    mm_log((1,"i_gradgen: (%d, %d)\n", xo[p], yo[p]));
    ICL_info(&ival[p]);
  }

  fdist = mymalloc( sizeof(float) * num );
  
  for(y = 0; y<ysize; y++) for(x = 0; x<xsize; x++) {
    float cs = 0;
    float csd = 0;
    for(p = 0; p<num; p++) {
      int xd    = x-xo[p];
      int yd    = y-yo[p];
      switch (dmeasure) {
      case 0: /* euclidean */
	fdist[p]  = sqrt(xd*xd + yd*yd); /* euclidean distance */
	break;
      case 1: /* euclidean squared */
	fdist[p]  = xd*xd + yd*yd; /* euclidean distance */
	break;
      case 2: /* euclidean squared */
	fdist[p]  = max(xd*xd, yd*yd); /* manhattan distance */
	break;
      default:
	m_fatal(3,"i_gradgen: Unknown distance measure\n");
      }
      cs += fdist[p];
    }
    
    csd = 1/((num-1)*cs);

    for(p = 0; p<num; p++) fdist[p] = (cs-fdist[p])*csd;
    
    for(ch = 0; ch<channels; ch++) {
      int tres = 0;
      for(p = 0; p<num; p++) tres += ival[p].channel[ch] * fdist[p];
      val.channel[ch] = saturate(tres);
    }
    i_ppix(im, x, y, &val); 
  }
  
}
















void
i_nearest_color_foo(i_img *im, int num, int *xo, int *yo, i_color *ival, int dmeasure) {

  int p, x, y;
  int xsize    = im->xsize;
  int ysize    = im->ysize;

  mm_log((1,"i_gradgen(im %p, num %d, xo %p, yo %p, ival %p, dmeasure %d)\n", im, num, xo, yo, ival, dmeasure));
  
  for(p = 0; p<num; p++) {
    mm_log((1,"i_gradgen: (%d, %d)\n", xo[p], yo[p]));
    ICL_info(&ival[p]);
  }

  for(y = 0; y<ysize; y++) for(x = 0; x<xsize; x++) {
    int   midx    = 0;
    float mindist = 0;
    float curdist = 0;

    int xd        = x-xo[0];
    int yd        = y-yo[0];

    switch (dmeasure) {
    case 0: /* euclidean */
      mindist = sqrt(xd*xd + yd*yd); /* euclidean distance */
      break;
    case 1: /* euclidean squared */
      mindist = xd*xd + yd*yd; /* euclidean distance */
      break;
    case 2: /* euclidean squared */
      mindist = max(xd*xd, yd*yd); /* manhattan distance */
      break;
    default:
      m_fatal(3,"i_nearest_color: Unknown distance measure\n");
    }

    for(p = 1; p<num; p++) {
      xd = x-xo[p];
      yd = y-yo[p];
      switch (dmeasure) {
      case 0: /* euclidean */
	curdist = sqrt(xd*xd + yd*yd); /* euclidean distance */
	break;
      case 1: /* euclidean squared */
	curdist = xd*xd + yd*yd; /* euclidean distance */
	break;
      case 2: /* euclidean squared */
	curdist = max(xd*xd, yd*yd); /* manhattan distance */
	break;
      default:
	m_fatal(3,"i_nearest_color: Unknown distance measure\n");
      }
      if (curdist < mindist) {
	mindist = curdist;
	midx = p;
      }
    }
    i_ppix(im, x, y, &ival[midx]); 
  }
}











void
i_nearest_color(i_img *im, int num, int *xo, int *yo, i_color *oval, int dmeasure) {
  i_color *ival;
  float *tval;
  float c1, c2;
  i_color val;
  int p, x, y, ch;
  int xsize    = im->xsize;
  int ysize    = im->ysize;
  int *cmatch;

  mm_log((1,"i_nearest_color(im %p, num %d, xo %p, yo %p, ival %p, dmeasure %d)\n", im, num, xo, yo, oval, dmeasure));

  tval   = mymalloc( sizeof(float)*num*im->channels );
  ival   = mymalloc( sizeof(i_color)*num );
  cmatch = mymalloc( sizeof(int)*num     );

  for(p = 0; p<num; p++) {
    for(ch = 0; ch<im->channels; ch++) tval[ p * im->channels + ch] = 0;
    cmatch[p] = 0;
  }

  
  for(y = 0; y<ysize; y++) for(x = 0; x<xsize; x++) {
    int   midx    = 0;
    float mindist = 0;
    float curdist = 0;
    
    int xd        = x-xo[0];
    int yd        = y-yo[0];

    switch (dmeasure) {
    case 0: /* euclidean */
      mindist = sqrt(xd*xd + yd*yd); /* euclidean distance */
      break;
    case 1: /* euclidean squared */
      mindist = xd*xd + yd*yd; /* euclidean distance */
      break;
    case 2: /* euclidean squared */
      mindist = max(xd*xd, yd*yd); /* manhattan distance */
      break;
    default:
      m_fatal(3,"i_nearest_color: Unknown distance measure\n");
    }
    
    for(p = 1; p<num; p++) {
      xd = x-xo[p];
      yd = y-yo[p];
      switch (dmeasure) {
      case 0: /* euclidean */
	curdist = sqrt(xd*xd + yd*yd); /* euclidean distance */
	break;
      case 1: /* euclidean squared */
	curdist = xd*xd + yd*yd; /* euclidean distance */
	break;
      case 2: /* euclidean squared */
	curdist = max(xd*xd, yd*yd); /* manhattan distance */
	break;
      default:
	m_fatal(3,"i_nearest_color: Unknown distance measure\n");
      }
      if (curdist < mindist) {
	mindist = curdist;
	midx = p;
      }
    }

    cmatch[midx]++;
    i_gpix(im, x, y, &val);
    c2 = 1.0/(float)(cmatch[midx]);
    c1 = 1.0-c2;
    
    for(ch = 0; ch<im->channels; ch++) 
      tval[midx*im->channels + ch] = c1*tval[midx*im->channels + ch] + c2 * (float) val.channel[ch];
  
    
  }

  for(p = 0; p<num; p++) for(ch = 0; ch<im->channels; ch++) ival[p].channel[ch] = tval[p*im->channels + ch];

  i_nearest_color_foo(im, num, xo, yo, ival, dmeasure);
}
