#include "image.h"
#include "imagei.h"
#include <stdlib.h>
#include <math.h>


/*
=head1 NAME

filters.c - implements filters that operate on images

=head1 SYNOPSIS

  
  i_contrast(im, 0.8);
  i_hardinvert(im);
  i_unsharp_mask(im, 2.0, 1.0);
  // and more

=head1 DESCRIPTION

filters.c implements basic filters for Imager.  These filters
should be accessible from the filter interface as defined in 
the pod for Imager.

=head1 FUNCTION REFERENCE

Some of these functions are internal.

=over

=cut
*/




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




typedef struct {
  float x,y,z;
} fvec;


static
float
dotp(fvec *a, fvec *b) {
  return a->x*b->x+a->y*b->y+a->z*b->z;
}

static
void
normalize(fvec *a) {
  double d = sqrt(dotp(a,a));
  a->x /= d;
  a->y /= d;
  a->z /= d;
}


/*
  positive directions:
  
  x - right, 
  y - down
  z - out of the plane
  
  I = Ia + Ip*( cd*Scol(N.L) + cs*(R.V)^n )
  
  Here, the variables are:
  
  * Ia   - ambient colour
  * Ip   - intensity of the point light source
  * cd   - diffuse coefficient
  * Scol - surface colour
  * cs   - specular coefficient
  * n    - objects shinyness
  * N    - normal vector
  * L    - lighting vector
  * R    - reflection vector
  * V    - vision vector

  static void fvec_dump(fvec *x) {
    printf("(%.2f %.2f %.2f)", x->x, x->y, x->z);
  }
*/

/* XXX: Should these return a code for success? */




/* 
=item i_bumpmap_complex(im, bump, channel, tx, ty, Lx, Ly, Lz, Ip, cd, cs, n, Ia, Il, Is)

Makes a bumpmap on image im using the bump image as the elevation map.

  im      - target image
  bump    - image that contains the elevation info
  channel - to take the elevation information from
  tx      - shift in x direction of where to start applying bumpmap
  ty      - shift in y direction of where to start applying bumpmap
  Lx      - x position/direction of light
  Ly      - y position/direction of light
  Lz      - z position/direction of light
  Ip      - light intensity
  cd      - diffuse coefficient
  cs      - specular coefficient
  n       - surface shinyness
  Ia      - ambient colour
  Il      - light colour
  Is      - specular colour

if z<0 then the L is taken to be the direction the light is shining in.  Otherwise
the L is taken to be the position of the Light, Relative to the image.

=cut
*/


void
i_bumpmap_complex(i_img *im,
		  i_img *bump,
		  int channel,
		  int tx,
		  int ty,
		  float Lx,
		  float Ly,
		  float Lz,
		  float cd,
		  float cs,
		  float n,
		  i_color *Ia,
		  i_color *Il,
		  i_color *Is) {
  i_img new_im;
  
  int inflight;
  int x, y, ch;
  int mx, Mx, my, My;
  
  float cdc[MAXCHANNELS];
  float csc[MAXCHANNELS];

  i_color x1_color, y1_color, x2_color, y2_color;

  i_color Scol;   /* Surface colour       */

  fvec L;         /* Light vector */
  fvec N;         /* surface normal       */
  fvec R;         /* Reflection vector    */
  fvec V;         /* Vision vector        */

  mm_log((1, "i_bumpmap_complex(im %p, bump %p, channel %d, tx %d, ty %d, Lx %.2f, Ly %.2f, Lz %.2f, cd %.2f, cs %.2f, n %.2f, Ia %p, Il %p, Is %p)\n",
	  im, bump, channel, tx, ty, Lx, Ly, Lz, cd, cs, n, Ia, Il, Is));
  
  if (channel >= bump->channels) {
    mm_log((1, "i_bumpmap_complex: channel = %d while bump image only has %d channels\n", channel, bump->channels));
    return;
  }

  for(ch=0; ch<im->channels; ch++) {
    cdc[ch] = (float)Il->channel[ch]*cd/255.f;
    csc[ch] = (float)Is->channel[ch]*cs/255.f;
  }

  mx = 1;
  my = 1;
  Mx = bump->xsize-1;
  My = bump->ysize-1;
  
  V.x = 0;
  V.y = 0;
  V.z = 1;
  
  if (Lz < 0) { /* Light specifies a direction vector, reverse it to get the vector from surface to light */
    L.x = -Lx;
    L.y = -Ly;
    L.z = -Lz;
    normalize(&L);
  } else {      /* Light is the position of the light source */
    inflight = 0;
    L.x = -0.2;
    L.y = -0.4;
    L.z =  1;
    normalize(&L);
  }

  i_img_empty_ch(&new_im, im->xsize, im->ysize, im->channels);

  for(y = 0; y < im->ysize; y++) {		
    for(x = 0; x < im->xsize; x++) {
      double dp1, dp2;
      double dx = 0, dy = 0;

      /* Calculate surface normal */
      if (mx<x && x<Mx && my<y && y<My) {
	i_gpix(bump, x + 1, y,     &x1_color);
	i_gpix(bump, x - 1, y,     &x2_color);
	i_gpix(bump, x,     y + 1, &y1_color);
	i_gpix(bump, x,     y - 1, &y2_color);
	dx = x2_color.channel[channel] - x1_color.channel[channel];
	dy = y2_color.channel[channel] - y1_color.channel[channel];
      } else {
	dx = 0;
	dy = 0;
      }
      N.x = -dx * 0.015;
      N.y = -dy * 0.015;
      N.z = 1;
      normalize(&N);

      /* Calculate Light vector if needed */
      if (Lz>=0) {
	L.x = Lx - x;
	L.y = Ly - y;
	L.z = Lz;
	normalize(&L);
      }
      
      dp1 = dotp(&L,&N);
      R.x = -L.x + 2*dp1*N.x;
      R.y = -L.y + 2*dp1*N.y;
      R.z = -L.z + 2*dp1*N.z;
      
      dp2 = dotp(&R,&V);

      dp1 = dp1<0 ?0 : dp1;
      dp2 = pow(dp2<0 ?0 : dp2,n);

      i_gpix(im, x, y, &Scol);

      for(ch = 0; ch < im->channels; ch++)
	Scol.channel[ch] = 
	  saturate( Ia->channel[ch] + cdc[ch]*Scol.channel[ch]*dp1 + csc[ch]*dp2 );
      
      i_ppix(&new_im, x, y, &Scol);
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
  myfree(fdist);
  
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

/*
=item i_unsharp_mask(im, stddev, scale)

Perform an usharp mask, which is defined as subtracting the blurred
image from double the original.

=cut
*/
void i_unsharp_mask(i_img *im, double stddev, double scale) {
  i_img copy;
  int x, y, ch;

  if (scale < 0)
    return;
  /* it really shouldn't ever be more than 1.0, but maybe ... */
  if (scale > 100)
    scale = 100;

  i_copy(&copy, im);
  i_gaussian(&copy, stddev);
  if (im->bits == i_8_bits) {
    i_color *blur = mymalloc(im->xsize * sizeof(i_color) * 2);
    i_color *out = blur + im->xsize;

    for (y = 0; y < im->ysize; ++y) {
      i_glin(&copy, 0, copy.xsize, y, blur);
      i_glin(im, 0, im->xsize, y, out);
      for (x = 0; x < im->xsize; ++x) {
        for (ch = 0; ch < im->channels; ++ch) {
          /*int temp = out[x].channel[ch] + 
            scale * (out[x].channel[ch] - blur[x].channel[ch]);*/
          int temp = out[x].channel[ch] * 2 - blur[x].channel[ch];
          if (temp < 0)
            temp = 0;
          else if (temp > 255)
            temp = 255;
          out[x].channel[ch] = temp;
        }
      }
      i_plin(im, 0, im->xsize, y, out);
    }

    myfree(blur);
  }
  else {
    i_fcolor *blur = mymalloc(im->xsize * sizeof(i_fcolor) * 2);
    i_fcolor *out = blur + im->xsize;

    for (y = 0; y < im->ysize; ++y) {
      i_glinf(&copy, 0, copy.xsize, y, blur);
      i_glinf(im, 0, im->xsize, y, out);
      for (x = 0; x < im->xsize; ++x) {
        for (ch = 0; ch < im->channels; ++ch) {
          double temp = out[x].channel[ch] +
            scale * (out[x].channel[ch] - blur[x].channel[ch]);
          if (temp < 0)
            temp = 0;
          else if (temp > 1.0)
            temp = 1.0;
          out[x].channel[ch] = temp;
        }
      }
      i_plinf(im, 0, im->xsize, y, out);
    }

    myfree(blur);
  }
  i_img_exorcise(&copy);
}

struct fount_state;
static double linear_fount_f(double x, double y, struct fount_state *state);
static double bilinear_fount_f(double x, double y, struct fount_state *state);
static double radial_fount_f(double x, double y, struct fount_state *state);
static double square_fount_f(double x, double y, struct fount_state *state);
static double revolution_fount_f(double x, double y, 
                                 struct fount_state *state);
static double conical_fount_f(double x, double y, struct fount_state *state);

typedef double (*fount_func)(double, double, struct fount_state *);
static fount_func fount_funcs[] =
{
  linear_fount_f,
  bilinear_fount_f,
  radial_fount_f,
  square_fount_f,
  revolution_fount_f,
  conical_fount_f,
};

static double linear_interp(double pos, i_fountain_seg *seg);
static double sine_interp(double pos, i_fountain_seg *seg);
static double sphereup_interp(double pos, i_fountain_seg *seg);
static double spheredown_interp(double pos, i_fountain_seg *seg);
typedef double (*fount_interp)(double pos, i_fountain_seg *seg);
static fount_interp fount_interps[] =
{
  linear_interp,
  linear_interp,
  sine_interp,
  sphereup_interp,
  spheredown_interp,
};

static void direct_cinterp(i_fcolor *out, double pos, i_fountain_seg *seg);
static void hue_up_cinterp(i_fcolor *out, double pos, i_fountain_seg *seg);
static void hue_down_cinterp(i_fcolor *out, double pos, i_fountain_seg *seg);
typedef void (*fount_cinterp)(i_fcolor *out, double pos, i_fountain_seg *seg);
static fount_cinterp fount_cinterps[] =
{
  direct_cinterp,
  hue_up_cinterp,
  hue_down_cinterp,
};

typedef double (*fount_repeat)(double v);
static double fount_r_none(double v);
static double fount_r_sawtooth(double v);
static double fount_r_triangle(double v);
static double fount_r_saw_both(double v);
static double fount_r_tri_both(double v);
static fount_repeat fount_repeats[] =
{
  fount_r_none,
  fount_r_sawtooth,
  fount_r_triangle,
  fount_r_saw_both,
  fount_r_tri_both,
};

static int simple_ssample(i_fcolor *out, double x, double y, 
                           struct fount_state *state);
static int random_ssample(i_fcolor *out, double x, double y, 
                           struct fount_state *state);
static int circle_ssample(i_fcolor *out, double x, double y, 
                           struct fount_state *state);
typedef int (*fount_ssample)(i_fcolor *out, double x, double y, 
                              struct fount_state *state);
static fount_ssample fount_ssamples[] =
{
  NULL,
  simple_ssample,
  random_ssample,
  circle_ssample,
};

static int
fount_getat(i_fcolor *out, double x, double y, struct fount_state *state);

/*
  Keep state information used by each type of fountain fill
*/
struct fount_state {
  /* precalculated for the equation of the line perpendicular to the line AB */
  double lA, lB, lC;
  double AB;
  double sqrtA2B2;
  double mult;
  double cos;
  double sin;
  double theta;
  int xa, ya;
  void *ssample_data;
  fount_func ffunc;
  fount_repeat rpfunc;
  fount_ssample ssfunc;
  double parm;
  i_fountain_seg *segs;
  int count;
};

static void
fount_init_state(struct fount_state *state, double xa, double ya, 
                 double xb, double yb, i_fountain_type type, 
                 i_fountain_repeat repeat, int combine, int super_sample, 
                 double ssample_param, int count, i_fountain_seg *segs);

static void
fount_finish_state(struct fount_state *state);

#define EPSILON (1e-6)

/*
=item i_fountain(im, xa, ya, xb, yb, type, repeat, combine, super_sample, ssample_param, count, segs)

Draws a fountain fill using A(xa, ya) and B(xb, yb) as reference points.

I<type> controls how the reference points are used:

=over

=item i_ft_linear

linear, where A is 0 and B is 1.

=item i_ft_bilinear

linear in both directions from A.

=item i_ft_radial

circular, where A is the centre of the fill, and B is a point
on the radius.

=item i_ft_radial_square

where A is the centre of the fill and B is the centre of
one side of the square.

=item i_ft_revolution

where A is the centre of the fill and B defines the 0/1.0
angle of the fill.

=item i_ft_conical

similar to i_ft_revolution, except that the revolution goes in both
directions

=back

I<repeat> can be one of:

=over

=item i_fr_none

values < 0 are treated as zero, values > 1 are treated as 1.

=item i_fr_sawtooth

negative values are treated as 0, positive values are modulo 1.0

=item i_fr_triangle

negative values are treated as zero, if (int)value is odd then the value is treated as 1-(value
mod 1.0), otherwise the same as for sawtooth.

=item i_fr_saw_both

like i_fr_sawtooth, except that the sawtooth pattern repeats into
negative values.

=item i_fr_tri_both

Like i_fr_triangle, except that negative values are handled as their
absolute values.

=back

If combine is non-zero then non-opaque values are combined with the
underlying color.

I<super_sample> controls super sampling, if any.  At some point I'll
probably add a adaptive super-sampler.  Current possible values are:

=over

=item i_fts_none

No super-sampling is done.

=item i_fts_grid

A square grid of points withing the pixel are sampled.

=item i_fts_random

Random points within the pixel are sampled.

=item i_fts_circle

Points on the radius of a circle are sampled.  This produces fairly
good results, but is fairly slow since sin() and cos() are evaluated
for each point.

=back

I<ssample_param> is intended to be roughly the number of points
sampled within the pixel.

I<count> and I<segs> define the segments of the fill.

=cut

*/

void
i_fountain(i_img *im, double xa, double ya, double xb, double yb, 
           i_fountain_type type, i_fountain_repeat repeat, 
           int combine, int super_sample, double ssample_param, 
           int count, i_fountain_seg *segs) {
  struct fount_state state;
  int x, y;
  i_fcolor *line = mymalloc(sizeof(i_fcolor) * im->xsize);
  i_fcolor *work = NULL;

  i_fountain_seg *my_segs;
  i_fill_combine_f combine_func = NULL;
  i_fill_combinef_f combinef_func = NULL;

  i_get_combine(combine, &combine_func, &combinef_func);
  if (combinef_func)
    work = mymalloc(sizeof(i_fcolor) * im->xsize);

  fount_init_state(&state, xa, ya, xb, yb, type, repeat, combine, 
                   super_sample, ssample_param, count, segs);
  my_segs = state.segs;

  for (y = 0; y < im->ysize; ++y) {
    i_glinf(im, 0, im->xsize, y, line);
    for (x = 0; x < im->xsize; ++x) {
      i_fcolor c;
      int got_one;
      if (super_sample == i_fts_none)
        got_one = fount_getat(&c, x, y, &state);
      else
        got_one = state.ssfunc(&c, x, y, &state);
      if (got_one) {
        if (combine)
          work[x] = c;
        else 
          line[x] = c;
      }
    }
    if (combine)
      combinef_func(line, work, im->channels, im->xsize);
    i_plinf(im, 0, im->xsize, y, line);
  }
  fount_finish_state(&state);
  if (work) myfree(work);
  myfree(line);
}

typedef struct {
  i_fill_t base;
  struct fount_state state;
} i_fill_fountain_t;

static void
fill_fountf(i_fill_t *fill, int x, int y, int width, int channels, 
            i_fcolor *data, i_fcolor *work);
static void
fount_fill_destroy(i_fill_t *fill);

/*
=item i_new_fount(xa, ya, xb, yb, type, repeat, combine, super_sample, ssample_param, count, segs)

Creates a new general fill which fills with a fountain fill.

=cut
*/

i_fill_t *
i_new_fill_fount(double xa, double ya, double xb, double yb, 
                 i_fountain_type type, i_fountain_repeat repeat, 
                 int combine, int super_sample, double ssample_param, 
                 int count, i_fountain_seg *segs) {
  i_fill_fountain_t *fill = mymalloc(sizeof(i_fill_fountain_t));
  
  fill->base.fill_with_color = NULL;
  fill->base.fill_with_fcolor = fill_fountf;
  fill->base.destroy = fount_fill_destroy;
  if (combine)
    i_get_combine(combine, &fill->base.combine, &fill->base.combinef);
  else {
    fill->base.combine = NULL;
    fill->base.combinef = NULL;
  }
  fount_init_state(&fill->state, xa, ya, xb, yb, type, repeat, combine, 
                   super_sample, ssample_param, count, segs);

  return &fill->base;
}

/*
=back

=head1 INTERNAL FUNCTIONS

=over

=item fount_init_state(...)

Used by both the fountain fill filter and the fountain fill.

=cut
*/

static void
fount_init_state(struct fount_state *state, double xa, double ya, 
                 double xb, double yb, i_fountain_type type, 
                 i_fountain_repeat repeat, int combine, int super_sample, 
                 double ssample_param, int count, i_fountain_seg *segs) {
  int i, j;
  i_fountain_seg *my_segs = mymalloc(sizeof(i_fountain_seg) * count);
  /*int have_alpha = im->channels == 2 || im->channels == 4;*/
  
  memset(state, 0, sizeof(*state));
  /* we keep a local copy that we can adjust for speed */
  for (i = 0; i < count; ++i) {
    i_fountain_seg *seg = my_segs + i;

    *seg = segs[i];
    if (seg->type < 0 || seg->type >= i_fst_end)
      seg->type = i_fst_linear;
    if (seg->color < 0 || seg->color >= i_fc_end)
      seg->color = i_fc_direct;
    if (seg->color == i_fc_hue_up || seg->color == i_fc_hue_down) {
      /* so we don't have to translate to HSV on each request, do it here */
      for (j = 0; j < 2; ++j) {
        i_rgb_to_hsvf(seg->c+j);
      }
      if (seg->color == i_fc_hue_up) {
        if (seg->c[1].channel[0] <= seg->c[0].channel[0])
          seg->c[1].channel[0] += 1.0;
      }
      else {
        if (seg->c[0].channel[0] <= seg->c[0].channel[1])
          seg->c[0].channel[0] += 1.0;
      }
    }
    /*printf("start %g mid %g end %g c0(%g,%g,%g,%g) c1(%g,%g,%g,%g) type %d color %d\n", 
           seg->start, seg->middle, seg->end, seg->c[0].channel[0], 
           seg->c[0].channel[1], seg->c[0].channel[2], seg->c[0].channel[3],
           seg->c[1].channel[0], seg->c[1].channel[1], seg->c[1].channel[2], 
           seg->c[1].channel[3], seg->type, seg->color);*/
           
  }

  /* initialize each engine */
  /* these are so common ... */
  state->lA = xb - xa;
  state->lB = yb - ya;
  state->AB = sqrt(state->lA * state->lA + state->lB * state->lB);
  state->xa = xa;
  state->ya = ya;
  switch (type) {
  default:
    type = i_ft_linear; /* make the invalid value valid */
  case i_ft_linear:
  case i_ft_bilinear:
    state->lC = ya * ya - ya * yb + xa * xa - xa * xb;
    state->mult = 1;
    state->mult = 1/linear_fount_f(xb, yb, state);
    break;

  case i_ft_radial:
    state->mult = 1.0 / sqrt((double)(xb-xa)*(xb-xa) 
                             + (double)(yb-ya)*(yb-ya));
    break;

  case i_ft_radial_square:
    state->cos = state->lA / state->AB;
    state->sin = state->lB / state->AB;
    state->mult = 1.0 / state->AB;
    break;

  case i_ft_revolution:
    state->theta = atan2(yb-ya, xb-xa);
    state->mult = 1.0 / (PI * 2);
    break;

  case i_ft_conical:
    state->theta = atan2(yb-ya, xb-xa);
    state->mult = 1.0 / PI;
    break;
  }
  state->ffunc = fount_funcs[type];
  if (super_sample < 0 
      || super_sample >= (int)(sizeof(fount_ssamples)/sizeof(*fount_ssamples))) {
    super_sample = 0;
  }
  state->ssample_data = NULL;
  switch (super_sample) {
  case i_fts_grid:
    ssample_param = floor(0.5 + sqrt(ssample_param));
    state->ssample_data = mymalloc(sizeof(i_fcolor) * ssample_param * ssample_param);
    break;

  case i_fts_random:
  case i_fts_circle:
    ssample_param = floor(0.5+ssample_param);
    state->ssample_data = mymalloc(sizeof(i_fcolor) * ssample_param);
    break;
  }
  state->parm = ssample_param;
  state->ssfunc = fount_ssamples[super_sample];
  if (repeat < 0 || repeat >= (sizeof(fount_repeats)/sizeof(*fount_repeats)))
    repeat = 0;
  state->rpfunc = fount_repeats[repeat];
  state->segs = my_segs;
  state->count = count;
}

static void
fount_finish_state(struct fount_state *state) {
  if (state->ssample_data)
    myfree(state->ssample_data);
  myfree(state->segs);
}


/*
=item fount_getat(out, x, y, ffunc, rpfunc, state, segs, count)

Evaluates the fountain fill at the given point.

This is called by both the non-super-sampling and super-sampling code.

You might think that it would make sense to sample the fill parameter
instead, and combine those, but this breaks badly.

=cut
*/

static int
fount_getat(i_fcolor *out, double x, double y, struct fount_state *state) {
  double v = (state->rpfunc)((state->ffunc)(x, y, state));
  int i;

  i = 0;
  while (i < state->count 
         && (v < state->segs[i].start || v > state->segs[i].end)) {
    ++i;
  }
  if (i < state->count) {
    v = (fount_interps[state->segs[i].type])(v, state->segs+i);
    (fount_cinterps[state->segs[i].color])(out, v, state->segs+i);
    return 1;
  }
  else
    return 0;
}

/*
=item linear_fount_f(x, y, state)

Calculate the fill parameter for a linear fountain fill.

Uses the point to line distance function, with some precalculation
done in i_fountain().

=cut
*/
static double
linear_fount_f(double x, double y, struct fount_state *state) {
  return (state->lA * x + state->lB * y + state->lC) / state->AB * state->mult;
}

/*
=item bilinear_fount_f(x, y, state)

Calculate the fill parameter for a bi-linear fountain fill.

=cut
*/
static double
bilinear_fount_f(double x, double y, struct fount_state *state) {
  return fabs((state->lA * x + state->lB * y + state->lC) / state->AB * state->mult);
}

/*
=item radial_fount_f(x, y, state)

Calculate the fill parameter for a radial fountain fill.

Simply uses the distance function.

=cut
 */
static double
radial_fount_f(double x, double y, struct fount_state *state) {
  return sqrt((double)(state->xa-x)*(state->xa-x) 
              + (double)(state->ya-y)*(state->ya-y)) * state->mult;
}

/*
=item square_fount_f(x, y, state)

Calculate the fill parameter for a square fountain fill.

Works by rotating the reference co-ordinate around the centre of the
square.

=cut
*/
static double
square_fount_f(double x, double y, struct fount_state *state) {
  int xc, yc; /* centred on A */
  double xt, yt; /* rotated by theta */
  xc = x - state->xa;
  yc = y - state->ya;
  xt = fabs(xc * state->cos + yc * state->sin);
  yt = fabs(-xc * state->sin + yc * state->cos);
  return (xt > yt ? xt : yt) * state->mult;
}

/*
=item revolution_fount_f(x, y, state)

Calculates the fill parameter for the revolution fountain fill.

=cut
*/
static double
revolution_fount_f(double x, double y, struct fount_state *state) {
  double angle = atan2(y - state->ya, x - state->xa);
  
  angle -= state->theta;
  if (angle < 0) {
    angle = fmod(angle+ PI * 4, PI*2);
  }

  return angle * state->mult;
}

/*
=item conical_fount_f(x, y, state)

Calculates the fill parameter for the conical fountain fill.

=cut
*/
static double
conical_fount_f(double x, double y, struct fount_state *state) {
  double angle = atan2(y - state->ya, x - state->xa);
  
  angle -= state->theta;
  if (angle < -PI)
    angle += PI * 2;
  else if (angle > PI) 
    angle -= PI * 2;

  return fabs(angle) * state->mult;
}

/*
=item linear_interp(pos, seg)

Calculates linear interpolation on the fill parameter.  Breaks the
segment into 2 regions based in the I<middle> value.

=cut
*/
static double
linear_interp(double pos, i_fountain_seg *seg) {
  if (pos < seg->middle) {
    double len = seg->middle - seg->start;
    if (len < EPSILON)
      return 0.0;
    else
      return (pos - seg->start) / len / 2;
  }
  else {
    double len = seg->end - seg->middle;
    if (len < EPSILON)
      return 1.0;
    else
      return 0.5 + (pos - seg->middle) / len / 2;
  }
}

/*
=item sine_interp(pos, seg)

Calculates sine function interpolation on the fill parameter.

=cut
*/
static double
sine_interp(double pos, i_fountain_seg *seg) {
  /* I wonder if there's a simple way to smooth the transition for this */
  double work = linear_interp(pos, seg);

  return (1-cos(work * PI))/2;
}

/*
=item sphereup_interp(pos, seg)

Calculates spherical interpolation on the fill parameter, with the cusp 
at the low-end.

=cut
*/
static double
sphereup_interp(double pos, i_fountain_seg *seg) {
  double work = linear_interp(pos, seg);

  return sqrt(1.0 - (1-work) * (1-work));
}

/*
=item spheredown_interp(pos, seg)

Calculates spherical interpolation on the fill parameter, with the cusp 
at the high-end.

=cut
*/
static double
spheredown_interp(double pos, i_fountain_seg *seg) {
  double work = linear_interp(pos, seg);

  return 1-sqrt(1.0 - work * work);
}

/*
=item direct_cinterp(out, pos, seg)

Calculates the fountain color based on direct scaling of the channels
of the color channels.

=cut
*/
static void
direct_cinterp(i_fcolor *out, double pos, i_fountain_seg *seg) {
  int ch;
  for (ch = 0; ch < MAXCHANNELS; ++ch) {
    out->channel[ch] = seg->c[0].channel[ch] * (1 - pos) 
      + seg->c[1].channel[ch] * pos;
  }
}

/*
=item hue_up_cinterp(put, pos, seg)

Calculates the fountain color based on scaling a HSV value.  The hue
increases as the fill parameter increases.

=cut
*/
static void
hue_up_cinterp(i_fcolor *out, double pos, i_fountain_seg *seg) {
  int ch;
  for (ch = 0; ch < MAXCHANNELS; ++ch) {
    out->channel[ch] = seg->c[0].channel[ch] * (1 - pos) 
      + seg->c[1].channel[ch] * pos;
  }
  i_hsv_to_rgbf(out);
}

/*
=item hue_down_cinterp(put, pos, seg)

Calculates the fountain color based on scaling a HSV value.  The hue
decreases as the fill parameter increases.

=cut
*/
static void
hue_down_cinterp(i_fcolor *out, double pos, i_fountain_seg *seg) {
  int ch;
  for (ch = 0; ch < MAXCHANNELS; ++ch) {
    out->channel[ch] = seg->c[0].channel[ch] * (1 - pos) 
      + seg->c[1].channel[ch] * pos;
  }
  i_hsv_to_rgbf(out);
}

/*
=item simple_ssample(out, parm, x, y, state, ffunc, rpfunc, segs, count)

Simple grid-based super-sampling.

=cut
*/
static int
simple_ssample(i_fcolor *out, double x, double y, struct fount_state *state) {
  i_fcolor *work = state->ssample_data;
  int dx, dy;
  int grid = state->parm;
  double base = -0.5 + 0.5 / grid;
  double step = 1.0 / grid;
  int ch, i;
  int samp_count = 0;

  for (dx = 0; dx < grid; ++dx) {
    for (dy = 0; dy < grid; ++dy) {
      if (fount_getat(work+samp_count, x + base + step * dx, 
                      y + base + step * dy, state)) {
        ++samp_count;
      }
    }
  }
  for (ch = 0; ch < MAXCHANNELS; ++ch) {
    out->channel[ch] = 0;
    for (i = 0; i < samp_count; ++i) {
      out->channel[ch] += work[i].channel[ch];
    }
    /* we divide by 4 rather than samp_count since if there's only one valid
       sample it should be mostly transparent */
    out->channel[ch] /= grid * grid;
  }
  return samp_count;
}

/*
=item random_ssample(out, parm, x, y, state, ffunc, rpfunc, segs, count)

Random super-sampling.

=cut
*/
static int
random_ssample(i_fcolor *out, double x, double y, 
               struct fount_state *state) {
  i_fcolor *work = state->ssample_data;
  int i, ch;
  int maxsamples = state->parm;
  double rand_scale = 1.0 / RAND_MAX;
  int samp_count = 0;
  for (i = 0; i < maxsamples; ++i) {
    if (fount_getat(work+samp_count, x - 0.5 + rand() * rand_scale, 
                    y - 0.5 + rand() * rand_scale, state)) {
      ++samp_count;
    }
  }
  for (ch = 0; ch < MAXCHANNELS; ++ch) {
    out->channel[ch] = 0;
    for (i = 0; i < samp_count; ++i) {
      out->channel[ch] += work[i].channel[ch];
    }
    /* we divide by maxsamples rather than samp_count since if there's
       only one valid sample it should be mostly transparent */
    out->channel[ch] /= maxsamples;
  }
  return samp_count;
}

/*
=item circle_ssample(out, parm, x, y, state, ffunc, rpfunc, segs, count)

Super-sampling around the circumference of a circle.

I considered saving the sin()/cos() values and transforming step-size
around the circle, but that's inaccurate, though it may not matter
much.

=cut
 */
static int
circle_ssample(i_fcolor *out, double x, double y, 
               struct fount_state *state) {
  i_fcolor *work = state->ssample_data;
  int i, ch;
  int maxsamples = state->parm;
  double angle = 2 * PI / maxsamples;
  double radius = 0.3; /* semi-random */
  int samp_count = 0;
  for (i = 0; i < maxsamples; ++i) {
    if (fount_getat(work+samp_count, x + radius * cos(angle * i), 
                    y + radius * sin(angle * i), state)) {
      ++samp_count;
    }
  }
  for (ch = 0; ch < MAXCHANNELS; ++ch) {
    out->channel[ch] = 0;
    for (i = 0; i < samp_count; ++i) {
      out->channel[ch] += work[i].channel[ch];
    }
    /* we divide by maxsamples rather than samp_count since if there's
       only one valid sample it should be mostly transparent */
    out->channel[ch] /= maxsamples;
  }
  return samp_count;
}

/*
=item fount_r_none(v)

Implements no repeats.  Simply clamps the fill value.

=cut
*/
static double
fount_r_none(double v) {
  return v < 0 ? 0 : v > 1 ? 1 : v;
}

/*
=item fount_r_sawtooth(v)

Implements sawtooth repeats.  Clamps negative values and uses fmod()
on others.

=cut
*/
static double
fount_r_sawtooth(double v) {
  return v < 0 ? 0 : fmod(v, 1.0);
}

/*
=item fount_r_triangle(v)

Implements triangle repeats.  Clamps negative values, uses fmod to get
a range 0 through 2 and then adjusts values > 1.

=cut
*/
static double
fount_r_triangle(double v) {
  if (v < 0)
    return 0;
  else {
    v = fmod(v, 2.0);
    return v > 1.0 ? 2.0 - v : v;
  }
}

/*
=item fount_r_saw_both(v)

Implements sawtooth repeats in the both postive and negative directions.

Adjusts the value to be postive and then just uses fmod().

=cut
*/
static double
fount_r_saw_both(double v) {
  if (v < 0)
    v += 1+(int)(-v);
  return fmod(v, 1.0);
}

/*
=item fount_r_tri_both(v)

Implements triangle repeats in the both postive and negative directions.

Uses fmod on the absolute value, and then adjusts values > 1.

=cut
*/
static double
fount_r_tri_both(double v) {
  v = fmod(fabs(v), 2.0);
  return v > 1.0 ? 2.0 - v : v;
}

/*
=item fill_fountf(fill, x, y, width, channels, data)

The fill function for fountain fills.

=cut
*/
static void
fill_fountf(i_fill_t *fill, int x, int y, int width, int channels, 
            i_fcolor *data, i_fcolor *work) {
  i_fill_fountain_t *f = (i_fill_fountain_t *)fill;
  
  if (fill->combinef) {
    i_fcolor *wstart = work;
    int count = width;

    while (width--) {
      i_fcolor c;
      int got_one;

      if (f->state.ssfunc)
        got_one = f->state.ssfunc(&c, x, y, &f->state);
      else
        got_one = fount_getat(&c, x, y, &f->state);
      
      *work++ = c;
      
      ++x;
    }
    (fill->combinef)(data, wstart, channels, count);
  }
  else {
    while (width--) {
      i_fcolor c;
      int got_one;

      if (f->state.ssfunc)
        got_one = f->state.ssfunc(&c, x, y, &f->state);
      else
        got_one = fount_getat(&c, x, y, &f->state);
      
      *data++ = c;
      
      ++x;
    }
  }
}

/*
=item fount_fill_destroy(fill)

=cut
*/
static void
fount_fill_destroy(i_fill_t *fill) {
  i_fill_fountain_t *f = (i_fill_fountain_t *)fill;
  fount_finish_state(&f->state);
}

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>

Tony Cook <tony@develop-help.com> (i_fountain())

=head1 SEE ALSO

Imager(3)

=cut
*/
