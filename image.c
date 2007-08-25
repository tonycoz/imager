#include "imager.h"
#include "imageri.h"

/*
=head1 NAME

image.c - implements most of the basic functions of Imager and much of the rest

=head1 SYNOPSIS

  i_img *i;
  i_color *c;
  c = i_color_new(red, green, blue, alpha);
  ICL_DESTROY(c);
  i = i_img_new();
  i_img_destroy(i);
  // and much more

=head1 DESCRIPTION

image.c implements the basic functions to create and destroy image and
color objects for Imager.

=head1 FUNCTION REFERENCE

Some of these functions are internal.

=over

=cut
*/

#define XAXIS 0
#define YAXIS 1
#define XYAXIS 2

#define minmax(a,b,i) ( ((a>=i)?a: ( (b<=i)?b:i   )) )

/* Hack around an obscure linker bug on solaris - probably due to builtin gcc thingies */
static void fake(void) { ceil(1); }

static int i_ppix_d(i_img *im, int x, int y, const i_color *val);
static int i_gpix_d(i_img *im, int x, int y, i_color *val);
static int i_glin_d(i_img *im, int l, int r, int y, i_color *vals);
static int i_plin_d(i_img *im, int l, int r, int y, const i_color *vals);
static int i_ppixf_d(i_img *im, int x, int y, const i_fcolor *val);
static int i_gpixf_d(i_img *im, int x, int y, i_fcolor *val);
static int i_glinf_d(i_img *im, int l, int r, int y, i_fcolor *vals);
static int i_plinf_d(i_img *im, int l, int r, int y, const i_fcolor *vals);
static int i_gsamp_d(i_img *im, int l, int r, int y, i_sample_t *samps, const int *chans, int chan_count);
static int i_gsampf_d(i_img *im, int l, int r, int y, i_fsample_t *samps, const int *chans, int chan_count);
/*static int i_psamp_d(i_img *im, int l, int r, int y, i_sample_t *samps, int *chans, int chan_count);
  static int i_psampf_d(i_img *im, int l, int r, int y, i_fsample_t *samps, int *chans, int chan_count);*/

/* 
=item ICL_new_internal(r, g, b, a)

Return a new color object with values passed to it.

   r - red   component (range: 0 - 255)
   g - green component (range: 0 - 255)
   b - blue  component (range: 0 - 255)
   a - alpha component (range: 0 - 255)

=cut
*/

i_color *
ICL_new_internal(unsigned char r,unsigned char g,unsigned char b,unsigned char a) {
  i_color *cl = NULL;

  mm_log((1,"ICL_new_internal(r %d,g %d,b %d,a %d)\n", r, g, b, a));

  if ( (cl=mymalloc(sizeof(i_color))) == NULL) i_fatal(2,"malloc() error\n");
  cl->rgba.r = r;
  cl->rgba.g = g;
  cl->rgba.b = b;
  cl->rgba.a = a;
  mm_log((1,"(%p) <- ICL_new_internal\n",cl));
  return cl;
}


/*
=item ICL_set_internal(cl, r, g, b, a)

 Overwrite a color with new values.

   cl - pointer to color object
   r - red   component (range: 0 - 255)
   g - green component (range: 0 - 255)
   b - blue  component (range: 0 - 255)
   a - alpha component (range: 0 - 255)

=cut
*/

i_color *
ICL_set_internal(i_color *cl,unsigned char r,unsigned char g,unsigned char b,unsigned char a) {
  mm_log((1,"ICL_set_internal(cl* %p,r %d,g %d,b %d,a %d)\n",cl,r,g,b,a));
  if (cl == NULL)
    if ( (cl=mymalloc(sizeof(i_color))) == NULL)
      i_fatal(2,"malloc() error\n");
  cl->rgba.r=r;
  cl->rgba.g=g;
  cl->rgba.b=b;
  cl->rgba.a=a;
  mm_log((1,"(%p) <- ICL_set_internal\n",cl));
  return cl;
}


/* 
=item ICL_add(dst, src, ch)

Add src to dst inplace - dst is modified.

   dst - pointer to destination color object
   src - pointer to color object that is added
   ch - number of channels

=cut
*/

void
ICL_add(i_color *dst,i_color *src,int ch) {
  int tmp,i;
  for(i=0;i<ch;i++) {
    tmp=dst->channel[i]+src->channel[i];
    dst->channel[i]= tmp>255 ? 255:tmp;
  }
}

/* 
=item ICL_info(cl)

Dump color information to log - strictly for debugging.

   cl - pointer to color object

=cut
*/

void
ICL_info(i_color const *cl) {
  mm_log((1,"i_color_info(cl* %p)\n",cl));
  mm_log((1,"i_color_info: (%d,%d,%d,%d)\n",cl->rgba.r,cl->rgba.g,cl->rgba.b,cl->rgba.a));
}

/* 
=item ICL_DESTROY

Destroy ancillary data for Color object.

   cl - pointer to color object

=cut
*/

void
ICL_DESTROY(i_color *cl) {
  mm_log((1,"ICL_DESTROY(cl* %p)\n",cl));
  myfree(cl);
}

/*
=item i_fcolor_new(double r, double g, double b, double a)

=cut
*/
i_fcolor *i_fcolor_new(double r, double g, double b, double a) {
  i_fcolor *cl = NULL;

  mm_log((1,"i_fcolor_new(r %g,g %g,b %g,a %g)\n", r, g, b, a));

  if ( (cl=mymalloc(sizeof(i_fcolor))) == NULL) i_fatal(2,"malloc() error\n");
  cl->rgba.r = r;
  cl->rgba.g = g;
  cl->rgba.b = b;
  cl->rgba.a = a;
  mm_log((1,"(%p) <- i_fcolor_new\n",cl));

  return cl;
}

/*
=item i_fcolor_destroy(i_fcolor *cl) 

=cut
*/
void i_fcolor_destroy(i_fcolor *cl) {
  myfree(cl);
}

/*
=item IIM_base_8bit_direct (static)

A static i_img object used to initialize direct 8-bit per sample images.

=cut
*/
static i_img IIM_base_8bit_direct =
{
  0, /* channels set */
  0, 0, 0, /* xsize, ysize, bytes */
  ~0U, /* ch_mask */
  i_8_bits, /* bits */
  i_direct_type, /* type */
  0, /* virtual */
  NULL, /* idata */
  { 0, 0, NULL }, /* tags */
  NULL, /* ext_data */

  i_ppix_d, /* i_f_ppix */
  i_ppixf_d, /* i_f_ppixf */
  i_plin_d, /* i_f_plin */
  i_plinf_d, /* i_f_plinf */
  i_gpix_d, /* i_f_gpix */
  i_gpixf_d, /* i_f_gpixf */
  i_glin_d, /* i_f_glin */
  i_glinf_d, /* i_f_glinf */
  i_gsamp_d, /* i_f_gsamp */
  i_gsampf_d, /* i_f_gsampf */

  NULL, /* i_f_gpal */
  NULL, /* i_f_ppal */
  NULL, /* i_f_addcolors */
  NULL, /* i_f_getcolors */
  NULL, /* i_f_colorcount */
  NULL, /* i_f_maxcolors */
  NULL, /* i_f_findcolor */
  NULL, /* i_f_setcolors */

  NULL, /* i_f_destroy */
};

/*static void set_8bit_direct(i_img *im) {
  im->i_f_ppix = i_ppix_d;
  im->i_f_ppixf = i_ppixf_d;
  im->i_f_plin = i_plin_d;
  im->i_f_plinf = i_plinf_d;
  im->i_f_gpix = i_gpix_d;
  im->i_f_gpixf = i_gpixf_d;
  im->i_f_glin = i_glin_d;
  im->i_f_glinf = i_glinf_d;
  im->i_f_gpal = NULL;
  im->i_f_ppal = NULL;
  im->i_f_addcolor = NULL;
  im->i_f_getcolor = NULL;
  im->i_f_colorcount = NULL;
  im->i_f_findcolor = NULL;
  }*/

/*
=item IIM_new(x, y, ch)

=item i_img_8_new(x, y, ch)

=category Image creation

Creates a new image object I<x> pixels wide, and I<y> pixels high with
I<ch> channels.

=cut
*/


i_img *
IIM_new(int x,int y,int ch) {
  i_img *im;
  mm_log((1,"IIM_new(x %d,y %d,ch %d)\n",x,y,ch));

  im=i_img_empty_ch(NULL,x,y,ch);
  
  mm_log((1,"(%p) <- IIM_new\n",im));
  return im;
}


void
IIM_DESTROY(i_img *im) {
  mm_log((1,"IIM_DESTROY(im* %p)\n",im));
  i_img_destroy(im);
  /*   myfree(cl); */
}

/* 
=item i_img_new()

Create new image reference - notice that this isn't an object yet and
this should be fixed asap.

=cut
*/


i_img *
i_img_new() {
  i_img *im;
  
  mm_log((1,"i_img_struct()\n"));
  if ( (im=mymalloc(sizeof(i_img))) == NULL)
    i_fatal(2,"malloc() error\n");
  
  *im = IIM_base_8bit_direct;
  im->xsize=0;
  im->ysize=0;
  im->channels=3;
  im->ch_mask=MAXINT;
  im->bytes=0;
  im->idata=NULL;
  
  mm_log((1,"(%p) <- i_img_struct\n",im));
  return im;
}

/* 
=item i_img_empty(im, x, y)

Re-new image reference (assumes 3 channels)

   im - Image pointer
   x - xsize of destination image
   y - ysize of destination image

**FIXME** what happens if a live image is passed in here?

Should this just call i_img_empty_ch()?

=cut
*/

i_img *
i_img_empty(i_img *im,int x,int y) {
  mm_log((1,"i_img_empty(*im %p, x %d, y %d)\n",im, x, y));
  return i_img_empty_ch(im, x, y, 3);
}

/* 
=item i_img_empty_ch(im, x, y, ch)

Re-new image reference 

   im - Image pointer
   x  - xsize of destination image
   y  - ysize of destination image
   ch - number of channels

=cut
*/

i_img *
i_img_empty_ch(i_img *im,int x,int y,int ch) {
  int bytes;

  mm_log((1,"i_img_empty_ch(*im %p, x %d, y %d, ch %d)\n", im, x, y, ch));

  if (x < 1 || y < 1) {
    i_push_error(0, "Image sizes must be positive");
    return NULL;
  }
  if (ch < 1 || ch > MAXCHANNELS) {
    i_push_errorf(0, "channels must be between 1 and %d", MAXCHANNELS);
    return NULL;
  }
  /* check this multiplication doesn't overflow */
  bytes = x*y*ch;
  if (bytes / y / ch != x) {
    i_push_errorf(0, "integer overflow calculating image allocation");
    return NULL;
  }

  if (im == NULL)
    if ( (im=mymalloc(sizeof(i_img))) == NULL)
      i_fatal(2,"malloc() error\n");

  memcpy(im, &IIM_base_8bit_direct, sizeof(i_img));
  i_tags_new(&im->tags);
  im->xsize    = x;
  im->ysize    = y;
  im->channels = ch;
  im->ch_mask  = MAXINT;
  im->bytes=bytes;
  if ( (im->idata=mymalloc(im->bytes)) == NULL) 
    i_fatal(2,"malloc() error\n"); 
  memset(im->idata,0,(size_t)im->bytes);
  
  im->ext_data = NULL;
  
  mm_log((1,"(%p) <- i_img_empty_ch\n",im));
  return im;
}

/* 
=item i_img_exorcise(im)

Free image data.

   im - Image pointer

=cut
*/

void
i_img_exorcise(i_img *im) {
  mm_log((1,"i_img_exorcise(im* 0x%x)\n",im));
  i_tags_destroy(&im->tags);
  if (im->i_f_destroy)
    (im->i_f_destroy)(im);
  if (im->idata != NULL) { myfree(im->idata); }
  im->idata    = NULL;
  im->xsize    = 0;
  im->ysize    = 0;
  im->channels = 0;

  im->i_f_ppix=i_ppix_d;
  im->i_f_gpix=i_gpix_d;
  im->i_f_plin=i_plin_d;
  im->i_f_glin=i_glin_d;
  im->ext_data=NULL;
}

/* 
=item i_img_destroy(im)

=category Image

Destroy image and free data via exorcise.

   im - Image pointer

=cut
*/

void
i_img_destroy(i_img *im) {
  mm_log((1,"i_img_destroy(im %p)\n",im));
  i_img_exorcise(im);
  if (im) { myfree(im); }
}

/* 
=item i_img_info(im, info)

=category Image

Return image information

   im - Image pointer
   info - pointer to array to return data

info is an array of 4 integers with the following values:

 info[0] - width
 info[1] - height
 info[2] - channels
 info[3] - channel mask

=cut
*/


void
i_img_info(i_img *im,int *info) {
  mm_log((1,"i_img_info(im 0x%x)\n",im));
  if (im != NULL) {
    mm_log((1,"i_img_info: xsize=%d ysize=%d channels=%d mask=%ud\n",im->xsize,im->ysize,im->channels,im->ch_mask));
    mm_log((1,"i_img_info: idata=0x%d\n",im->idata));
    info[0] = im->xsize;
    info[1] = im->ysize;
    info[2] = im->channels;
    info[3] = im->ch_mask;
  } else {
    info[0] = 0;
    info[1] = 0;
    info[2] = 0;
    info[3] = 0;
  }
}

/*
=item i_img_setmask(im, ch_mask)

=synopsis // only channel 0 writeable 
=synopsis i_img_setmask(img, 0x01);

Set the image channel mask for I<im> to I<ch_mask>.

=cut
*/
void
i_img_setmask(i_img *im,int ch_mask) { im->ch_mask=ch_mask; }


/*
=item i_img_getmask(im)

=synopsis mask = i_img_getmask(img);

Get the image channel mask for I<im>.

=cut
*/
int
i_img_getmask(i_img *im) { return im->ch_mask; }

/*
=item i_img_getchannels(im)

=synopsis channels = i_img_getchannels(img);

Get the number of channels in I<im>.

=cut
*/
int
i_img_getchannels(i_img *im) { return im->channels; }

/*
=item i_img_get_width(im)

=synopsis width = i_img_get_width(im);

Returns the width in pixels of the image.

=cut
*/
i_img_dim
i_img_get_width(i_img *im) {
  return im->xsize;
}

/*
=item i_img_get_height(im)

=synopsis height = i_img_get_height(im);

Returns the height in pixels of the image.

=cut
*/
i_img_dim
i_img_get_height(i_img *im) {
  return im->ysize;
}

/*
=item i_copyto_trans(im, src, x1, y1, x2, y2, tx, ty, trans)

=category Image

(x1,y1) (x2,y2) specifies the region to copy (in the source coordinates)
(tx,ty) specifies the upper left corner for the target image.
pass NULL in trans for non transparent i_colors.

=cut
*/

void
i_copyto_trans(i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty,const i_color *trans) {
  i_color pv;
  int x,y,t,ttx,tty,tt,ch;

  mm_log((1,"i_copyto_trans(im* %p,src 0x%x, x1 %d, y1 %d, x2 %d, y2 %d, tx %d, ty %d, trans* 0x%x)\n",
	  im, src, x1, y1, x2, y2, tx, ty, trans));
  
  if (x2<x1) { t=x1; x1=x2; x2=t; }
  if (y2<y1) { t=y1; y1=y2; y2=t; }

  ttx=tx;
  for(x=x1;x<x2;x++)
    {
      tty=ty;
      for(y=y1;y<y2;y++)
	{
	  i_gpix(src,x,y,&pv);
	  if ( trans != NULL)
	  {
	    tt=0;
	    for(ch=0;ch<im->channels;ch++) if (trans->channel[ch]!=pv.channel[ch]) tt++;
	    if (tt) i_ppix(im,ttx,tty,&pv);
	  } else i_ppix(im,ttx,tty,&pv);
	  tty++;
	}
      ttx++;
    }
}

/*
=item i_copyto(dest, src, x1, y1, x2, y2, tx, ty)

=category Image

Copies image data from the area (x1,y1)-[x2,y2] in the source image to
a rectangle the same size with it's top-left corner at (tx,ty) in the
destination image.

If x1 > x2 or y1 > y2 then the corresponding co-ordinates are swapped.

=cut
*/

void
i_copyto(i_img *im, i_img *src, int x1, int y1, int x2, int y2, int tx, int ty) {
  int x, y, t, ttx, tty;
  
  if (x2<x1) { t=x1; x1=x2; x2=t; }
  if (y2<y1) { t=y1; y1=y2; y2=t; }
  if (tx < 0) {
    /* adjust everything equally */
    x1 += -tx;
    x2 += -tx;
    tx = 0;
  }
  if (ty < 0) {
    y1 += -ty;
    y2 += -ty;
    ty = 0;
  }
  if (x1 >= src->xsize || y1 >= src->ysize)
    return; /* nothing to do */
  if (x2 > src->xsize)
    x2 = src->xsize;
  if (y2 > src->ysize)
    y2 = src->ysize;
  if (x1 == x2 || y1 == y2)
    return; /* nothing to do */

  mm_log((1,"i_copyto(im* %p, src %p, x1 %d, y1 %d, x2 %d, y2 %d, tx %d, ty %d)\n",
	  im, src, x1, y1, x2, y2, tx, ty));
  
  if (im->bits == i_8_bits) {
    i_color *row = mymalloc(sizeof(i_color) * (x2-x1));
    tty = ty;
    for(y=y1; y<y2; y++) {
      ttx = tx;
      i_glin(src, x1, x2, y, row);
      i_plin(im, tx, tx+x2-x1, tty, row);
      tty++;
    }
    myfree(row);
  }
  else {
    i_fcolor pv;
    tty = ty;
    for(y=y1; y<y2; y++) {
      ttx = tx;
      for(x=x1; x<x2; x++) {
        i_gpixf(src, x,   y,   &pv);
        i_ppixf(im,  ttx, tty, &pv);
        ttx++;
      }
      tty++;
    }
  }
}

/*
=item i_copy(src)

=category Image

Creates a new image that is a copy of src.

Tags are not copied, only the image data.

Returns: i_img *

=cut
*/

i_img *
i_copy(i_img *src) {
  int y, y1, x1;
  i_img *im = i_sametype(src, src->xsize, src->ysize);

  mm_log((1,"i_copy(src %p)\n", src));

  if (!im)
    return NULL;

  x1 = src->xsize;
  y1 = src->ysize;
  if (src->type == i_direct_type) {
    if (src->bits == i_8_bits) {
      i_color *pv;
      pv = mymalloc(sizeof(i_color) * x1);
      
      for (y = 0; y < y1; ++y) {
        i_glin(src, 0, x1, y, pv);
        i_plin(im, 0, x1, y, pv);
      }
      myfree(pv);
    }
    else {
      i_fcolor *pv;

      pv = mymalloc(sizeof(i_fcolor) * x1);
      for (y = 0; y < y1; ++y) {
        i_glinf(src, 0, x1, y, pv);
        i_plinf(im, 0, x1, y, pv);
      }
      myfree(pv);
    }
  }
  else {
    i_color temp;
    int index;
    int count;
    i_palidx *vals;

    /* paletted image */
    i_img_pal_new_low(im, x1, y1, src->channels, i_maxcolors(src));
    /* copy across the palette */
    count = i_colorcount(src);
    for (index = 0; index < count; ++index) {
      i_getcolors(src, index, &temp, 1);
      i_addcolors(im, &temp, 1);
    }

    vals = mymalloc(sizeof(i_palidx) * x1);
    for (y = 0; y < y1; ++y) {
      i_gpal(src, 0, x1, y, vals);
      i_ppal(im, 0, x1, y, vals);
    }
    myfree(vals);
  }

  return im;
}


/*
=item i_flipxy(im, axis)

Flips the image inplace around the axis specified.
Returns 0 if parameters are invalid.

   im   - Image pointer
   axis - 0 = x, 1 = y, 2 = both

=cut
*/

undef_int
i_flipxy(i_img *im, int direction) {
  int x, x2, y, y2, xm, ym;
  int xs = im->xsize;
  int ys = im->ysize;
  
  mm_log((1, "i_flipxy(im %p, direction %d)\n", im, direction ));

  if (!im) return 0;

  switch (direction) {
  case XAXIS: /* Horizontal flip */
    xm = xs/2;
    ym = ys;
    for(y=0; y<ym; y++) {
      x2 = xs-1;
      for(x=0; x<xm; x++) {
	i_color val1, val2;
	i_gpix(im, x,  y,  &val1);
	i_gpix(im, x2, y,  &val2);
	i_ppix(im, x,  y,  &val2);
	i_ppix(im, x2, y,  &val1);
	x2--;
      }
    }
    break;
  case YAXIS: /* Vertical flip */
    xm = xs;
    ym = ys/2;
    y2 = ys-1;
    for(y=0; y<ym; y++) {
      for(x=0; x<xm; x++) {
	i_color val1, val2;
	i_gpix(im, x,  y,  &val1);
	i_gpix(im, x,  y2, &val2);
	i_ppix(im, x,  y,  &val2);
	i_ppix(im, x,  y2, &val1);
      }
      y2--;
    }
    break;
  case XYAXIS: /* Horizontal and Vertical flip */
    xm = xs/2;
    ym = ys/2;
    y2 = ys-1;
    for(y=0; y<ym; y++) {
      x2 = xs-1;
      for(x=0; x<xm; x++) {
	i_color val1, val2;
	i_gpix(im, x,  y,  &val1);
	i_gpix(im, x2, y2, &val2);
	i_ppix(im, x,  y,  &val2);
	i_ppix(im, x2, y2, &val1);

	i_gpix(im, x2, y,  &val1);
	i_gpix(im, x,  y2, &val2);
	i_ppix(im, x2, y,  &val2);
	i_ppix(im, x,  y2, &val1);
	x2--;
      }
      y2--;
    }
    if (xm*2 != xs) { /* odd number of column */
      mm_log((1, "i_flipxy: odd number of columns\n"));
      x = xm;
      y2 = ys-1;
      for(y=0; y<ym; y++) {
	i_color val1, val2;
	i_gpix(im, x,  y,  &val1);
	i_gpix(im, x,  y2, &val2);
	i_ppix(im, x,  y,  &val2);
	i_ppix(im, x,  y2, &val1);
	y2--;
      }
    }
    if (ym*2 != ys) { /* odd number of rows */
      mm_log((1, "i_flipxy: odd number of rows\n"));
      y = ym;
      x2 = xs-1;
      for(x=0; x<xm; x++) {
	i_color val1, val2;
	i_gpix(im, x,  y,  &val1);
	i_gpix(im, x2, y,  &val2);
	i_ppix(im, x,  y,  &val2);
	i_ppix(im, x2, y,  &val1);
	x2--;
      }
    }
    break;
  default:
    mm_log((1, "i_flipxy: direction is invalid\n" ));
    return 0;
  }
  return 1;
}





static
float
Lanczos(float x) {
  float PIx, PIx2;
  
  PIx = PI * x;
  PIx2 = PIx / 2.0;
  
  if ((x >= 2.0) || (x <= -2.0)) return (0.0);
  else if (x == 0.0) return (1.0);
  else return(sin(PIx) / PIx * sin(PIx2) / PIx2);
}


/*
=item i_scaleaxis(im, value, axis)

Returns a new image object which is I<im> scaled by I<value> along
wither the x-axis (I<axis> == 0) or the y-axis (I<axis> == 1).

=cut
*/

i_img*
i_scaleaxis(i_img *im, float Value, int Axis) {
  int hsize, vsize, i, j, k, l, lMax, iEnd, jEnd;
  int LanczosWidthFactor;
  float *l0, *l1, OldLocation;
  int T; 
  float t;
  float F, PictureValue[MAXCHANNELS];
  short psave;
  i_color val,val1,val2;
  i_img *new_img;

  mm_log((1,"i_scaleaxis(im %p,Value %.2f,Axis %d)\n",im,Value,Axis));


  if (Axis == XAXIS) {
    hsize = (int)(0.5 + im->xsize * Value);
    if (hsize < 1) {
      hsize = 1;
      Value = 1.0 / im->xsize;
    }
    vsize = im->ysize;
    
    jEnd = hsize;
    iEnd = vsize;
  } else {
    hsize = im->xsize;
    vsize = (int)(0.5 + im->ysize * Value);

    if (vsize < 1) {
      vsize = 1;
      Value = 1.0 / im->ysize;
    }

    jEnd = vsize;
    iEnd = hsize;
  }
  
  new_img = i_img_empty_ch(NULL, hsize, vsize, im->channels);
  
  /* 1.4 is a magic number, setting it to 2 will cause rather blurred images */
  LanczosWidthFactor = (Value >= 1) ? 1 : (int) (1.4/Value); 
  lMax = LanczosWidthFactor << 1;
  
  l0 = mymalloc(lMax * sizeof(float));
  l1 = mymalloc(lMax * sizeof(float));
  
  for (j=0; j<jEnd; j++) {
    OldLocation = ((float) j) / Value;
    T = (int) (OldLocation);
    F = OldLocation - (float) T;
    
    for (l = 0; l<lMax; l++) {
      l0[lMax-l-1] = Lanczos(((float) (lMax-l-1) + F) / (float) LanczosWidthFactor);
      l1[l]        = Lanczos(((float) (l+1)      - F) / (float) LanczosWidthFactor);
    }
    
    /* Make sure filter is normalized */
    t = 0.0;
    for(l=0; l<lMax; l++) {
      t+=l0[l];
      t+=l1[l];
    }
    t /= (float)LanczosWidthFactor;
    
    for(l=0; l<lMax; l++) {
      l0[l] /= t;
      l1[l] /= t;
    }

    if (Axis == XAXIS) {
      
      for (i=0; i<iEnd; i++) {
	for (k=0; k<im->channels; k++) PictureValue[k] = 0.0;
	for (l=0; l<lMax; l++) {
	  int mx = T-lMax+l+1;
	  int Mx = T+l+1;
	  mx = (mx < 0) ? 0 : mx;
	  Mx = (Mx >= im->xsize) ? im->xsize-1 : Mx;
	  
	  i_gpix(im, Mx, i, &val1);
	  i_gpix(im, mx, i, &val2);
	  
	  for (k=0; k<im->channels; k++) {
	    PictureValue[k] += l1[l]        * val1.channel[k];
	    PictureValue[k] += l0[lMax-l-1] * val2.channel[k];
	  }
	}
	for(k=0;k<im->channels;k++) {
	  psave = (short)(0.5+(PictureValue[k] / LanczosWidthFactor));
	  val.channel[k]=minmax(0,255,psave);
	}
	i_ppix(new_img, j, i, &val);
      }
      
    } else {
      
      for (i=0; i<iEnd; i++) {
	for (k=0; k<im->channels; k++) PictureValue[k] = 0.0;
	for (l=0; l < lMax; l++) {
	  int mx = T-lMax+l+1;
	  int Mx = T+l+1;
	  mx = (mx < 0) ? 0 : mx;
	  Mx = (Mx >= im->ysize) ? im->ysize-1 : Mx;

	  i_gpix(im, i, Mx, &val1);
	  i_gpix(im, i, mx, &val2);
	  for (k=0; k<im->channels; k++) {
	    PictureValue[k] += l1[l]        * val1.channel[k];
	    PictureValue[k] += l0[lMax-l-1] * val2.channel[k]; 
	  }
	}
	for (k=0; k<im->channels; k++) {
	  psave = (short)(0.5+(PictureValue[k] / LanczosWidthFactor));
	  val.channel[k] = minmax(0, 255, psave);
	}
	i_ppix(new_img, i, j, &val);
      }
      
    }
  }
  myfree(l0);
  myfree(l1);

  mm_log((1,"(%p) <- i_scaleaxis\n", new_img));

  return new_img;
}


/* 
=item i_scale_nn(im, scx, scy)

Scale by using nearest neighbor 
Both axes scaled at the same time since 
nothing is gained by doing it in two steps 

=cut
*/


i_img*
i_scale_nn(i_img *im, float scx, float scy) {

  int nxsize,nysize,nx,ny;
  i_img *new_img;
  i_color val;

  mm_log((1,"i_scale_nn(im 0x%x,scx %.2f,scy %.2f)\n",im,scx,scy));

  nxsize = (int) ((float) im->xsize * scx);
  if (nxsize < 1) {
    nxsize = 1;
    scx = 1 / im->xsize;
  }
  nysize = (int) ((float) im->ysize * scy);
  if (nysize < 1) {
    nysize = 1;
    scy = 1 / im->ysize;
  }
    
  new_img=i_img_empty_ch(NULL,nxsize,nysize,im->channels);
  
  for(ny=0;ny<nysize;ny++) for(nx=0;nx<nxsize;nx++) {
    i_gpix(im,((float)nx)/scx,((float)ny)/scy,&val);
    i_ppix(new_img,nx,ny,&val);
  }

  mm_log((1,"(0x%x) <- i_scale_nn\n",new_img));

  return new_img;
}

/*
=item i_sametype(i_img *im, int xsize, int ysize)

=category Image creation

Returns an image of the same type (sample size, channels, paletted/direct).

For paletted images the palette is copied from the source.

=cut
*/

i_img *i_sametype(i_img *src, int xsize, int ysize) {
  if (src->type == i_direct_type) {
    if (src->bits == 8) {
      return i_img_empty_ch(NULL, xsize, ysize, src->channels);
    }
    else if (src->bits == i_16_bits) {
      return i_img_16_new(xsize, ysize, src->channels);
    }
    else if (src->bits == i_double_bits) {
      return i_img_double_new(xsize, ysize, src->channels);
    }
    else {
      i_push_error(0, "Unknown image bits");
      return NULL;
    }
  }
  else {
    i_color col;
    int i;

    i_img *targ = i_img_pal_new(xsize, ysize, src->channels, i_maxcolors(src));
    for (i = 0; i < i_colorcount(src); ++i) {
      i_getcolors(src, i, &col, 1);
      i_addcolors(targ, &col, 1);
    }

    return targ;
  }
}

/*
=item i_sametype_chans(i_img *im, int xsize, int ysize, int channels)

=category Image creation

Returns an image of the same type (sample size).

For paletted images the equivalent direct type is returned.

=cut
*/

i_img *i_sametype_chans(i_img *src, int xsize, int ysize, int channels) {
  if (src->bits == 8) {
    return i_img_empty_ch(NULL, xsize, ysize, channels);
  }
  else if (src->bits == i_16_bits) {
    return i_img_16_new(xsize, ysize, channels);
  }
  else if (src->bits == i_double_bits) {
    return i_img_double_new(xsize, ysize, channels);
  }
  else {
    i_push_error(0, "Unknown image bits");
    return NULL;
  }
}

/*
=item i_transform(im, opx, opxl, opy, opyl, parm, parmlen)

Spatially transforms I<im> returning a new image.

opx for a length of opxl and opy for a length of opy are arrays of
operators that modify the x and y positions to retreive the pixel data from.

parm and parmlen define extra parameters that the operators may use.

Note that this function is largely superseded by the more flexible
L<transform.c/i_transform2>.

Returns the new image.

The operators for this function are defined in L<stackmach.c>.

=cut
*/
i_img*
i_transform(i_img *im, int *opx,int opxl,int *opy,int opyl,double parm[],int parmlen) {
  double rx,ry;
  int nxsize,nysize,nx,ny;
  i_img *new_img;
  i_color val;
  
  mm_log((1,"i_transform(im 0x%x, opx 0x%x, opxl %d, opy 0x%x, opyl %d, parm 0x%x, parmlen %d)\n",im,opx,opxl,opy,opyl,parm,parmlen));

  nxsize = im->xsize;
  nysize = im->ysize ;
  
  new_img=i_img_empty_ch(NULL,nxsize,nysize,im->channels);
  /*   fprintf(stderr,"parm[2]=%f\n",parm[2]);   */
  for(ny=0;ny<nysize;ny++) for(nx=0;nx<nxsize;nx++) {
    /*     parm[parmlen-2]=(double)nx;
	   parm[parmlen-1]=(double)ny; */

    parm[0]=(double)nx;
    parm[1]=(double)ny;

    /*     fprintf(stderr,"(%d,%d) ->",nx,ny);  */
    rx=i_op_run(opx,opxl,parm,parmlen);
    ry=i_op_run(opy,opyl,parm,parmlen);
    /*    fprintf(stderr,"(%f,%f)\n",rx,ry); */
    i_gpix(im,rx,ry,&val);
    i_ppix(new_img,nx,ny,&val);
  }

  mm_log((1,"(0x%x) <- i_transform\n",new_img));
  return new_img;
}

/*
=item i_img_diff(im1, im2)

Calculates the sum of the squares of the differences between
correspoding channels in two images.

If the images are not the same size then only the common area is 
compared, hence even if images are different sizes this function 
can return zero.

=cut
*/
float
i_img_diff(i_img *im1,i_img *im2) {
  int x,y,ch,xb,yb,chb;
  float tdiff;
  i_color val1,val2;

  mm_log((1,"i_img_diff(im1 0x%x,im2 0x%x)\n",im1,im2));

  xb=(im1->xsize<im2->xsize)?im1->xsize:im2->xsize;
  yb=(im1->ysize<im2->ysize)?im1->ysize:im2->ysize;
  chb=(im1->channels<im2->channels)?im1->channels:im2->channels;

  mm_log((1,"i_img_diff: xb=%d xy=%d chb=%d\n",xb,yb,chb));

  tdiff=0;
  for(y=0;y<yb;y++) for(x=0;x<xb;x++) {
    i_gpix(im1,x,y,&val1);
    i_gpix(im2,x,y,&val2);

    for(ch=0;ch<chb;ch++) tdiff+=(val1.channel[ch]-val2.channel[ch])*(val1.channel[ch]-val2.channel[ch]);
  }
  mm_log((1,"i_img_diff <- (%.2f)\n",tdiff));
  return tdiff;
}

/* just a tiny demo of haar wavelets */

i_img*
i_haar(i_img *im) {
  int mx,my;
  int fx,fy;
  int x,y;
  int ch,c;
  i_img *new_img,*new_img2;
  i_color val1,val2,dval1,dval2;
  
  mx=im->xsize;
  my=im->ysize;
  fx=(mx+1)/2;
  fy=(my+1)/2;


  /* horizontal pass */
  
  new_img=i_img_empty_ch(NULL,fx*2,fy*2,im->channels);
  new_img2=i_img_empty_ch(NULL,fx*2,fy*2,im->channels);

  c=0; 
  for(y=0;y<my;y++) for(x=0;x<fx;x++) {
    i_gpix(im,x*2,y,&val1);
    i_gpix(im,x*2+1,y,&val2);
    for(ch=0;ch<im->channels;ch++) {
      dval1.channel[ch]=(val1.channel[ch]+val2.channel[ch])/2;
      dval2.channel[ch]=(255+val1.channel[ch]-val2.channel[ch])/2;
    }
    i_ppix(new_img,x,y,&dval1);
    i_ppix(new_img,x+fx,y,&dval2);
  }

  for(y=0;y<fy;y++) for(x=0;x<mx;x++) {
    i_gpix(new_img,x,y*2,&val1);
    i_gpix(new_img,x,y*2+1,&val2);
    for(ch=0;ch<im->channels;ch++) {
      dval1.channel[ch]=(val1.channel[ch]+val2.channel[ch])/2;
      dval2.channel[ch]=(255+val1.channel[ch]-val2.channel[ch])/2;
    }
    i_ppix(new_img2,x,y,&dval1);
    i_ppix(new_img2,x,y+fy,&dval2);
  }

  i_img_destroy(new_img);
  return new_img2;
}

/* 
=item i_count_colors(im, maxc)

returns number of colors or -1 
to indicate that it was more than max colors

=cut
*/
/* This function has been changed and is now faster. It's using
 * i_gsamp instead of i_gpix */
int
i_count_colors(i_img *im,int maxc) {
  struct octt *ct;
  int x,y;
  int colorcnt;
  int channels[3];
  int *samp_chans;
  i_sample_t * samp;
  int xsize = im->xsize; 
  int ysize = im->ysize;
  int samp_cnt = 3 * xsize;

  if (im->channels >= 3) {
    samp_chans = NULL;
  }
  else {
    channels[0] = channels[1] = channels[2] = 0;
    samp_chans = channels;
  }

  ct = octt_new();

  samp = (i_sample_t *) mymalloc( xsize * 3 * sizeof(i_sample_t));

  colorcnt = 0;
  for(y = 0; y < ysize; ) {
      i_gsamp(im, 0, xsize, y++, samp, samp_chans, 3);
      for(x = 0; x < samp_cnt; ) {
          colorcnt += octt_add(ct, samp[x], samp[x+1], samp[x+2]);
          x += 3;
          if (colorcnt > maxc) { 
              octt_delete(ct); 
              return -1; 
          }
      }
  }
  myfree(samp);
  octt_delete(ct);
  return colorcnt;
}

/* sorts the array ra[0..n-1] into increasing order using heapsort algorithm 
 * (adapted from the Numerical Recipes)
 */
/* Needed by get_anonymous_color_histo */
static void
hpsort(unsigned int n, unsigned *ra) {
    unsigned int i,
                 ir,
                 j,
                 l, 
                 rra;

    if (n < 2) return;
    l = n >> 1;
    ir = n - 1;
    for(;;) {
        if (l > 0) {
            rra = ra[--l];
        }
        else {
            rra = ra[ir];
            ra[ir] = ra[0];
            if (--ir == 0) {
                ra[0] = rra;
                break;
            }
        }
        i = l;
        j = 2 * l + 1;
        while (j <= ir) {
            if (j < ir && ra[j] < ra[j+1]) j++;
            if (rra < ra[j]) {
                ra[i] = ra[j];
                i = j;
                j++; j <<= 1; j--;
            }
            else break;
        }
        ra[i] = rra;
    }
}

/* This function constructs an ordered list which represents how much the
 * different colors are used. So for instance (100, 100, 500) means that one
 * color is used for 500 pixels, another for 100 pixels and another for 100
 * pixels. It's tuned for performance. You might not like the way I've hardcoded
 * the maxc ;-) and you might want to change the name... */
/* Uses octt_histo */
int
i_get_anonymous_color_histo(i_img *im, unsigned int **col_usage, int maxc) {
  struct octt *ct;
  int x,y;
  int colorcnt;
  unsigned int *col_usage_it;
  i_sample_t * samp;
  int channels[3];
  int *samp_chans;
  
  int xsize = im->xsize; 
  int ysize = im->ysize;
  int samp_cnt = 3 * xsize;
  ct = octt_new();
  
  samp = (i_sample_t *) mymalloc( xsize * 3 * sizeof(i_sample_t));
  
  if (im->channels >= 3) {
    samp_chans = NULL;
  }
  else {
    channels[0] = channels[1] = channels[2] = 0;
    samp_chans = channels;
  }

  colorcnt = 0;
  for(y = 0; y < ysize; ) {
    i_gsamp(im, 0, xsize, y++, samp, samp_chans, 3);
    for(x = 0; x < samp_cnt; ) {
      colorcnt += octt_add(ct, samp[x], samp[x+1], samp[x+2]);
      x += 3;
      if (colorcnt > maxc) { 
	octt_delete(ct); 
	return -1; 
      }
    }
  }
  myfree(samp);
  /* Now that we know the number of colours... */
  col_usage_it = *col_usage = (unsigned int *) mymalloc(colorcnt * sizeof(unsigned int));
  octt_histo(ct, &col_usage_it);
  hpsort(colorcnt, *col_usage);
  octt_delete(ct);
  return colorcnt;
}

/*
=back

=head2 8-bit per sample image internal functions

These are the functions installed in an 8-bit per sample image.

=over

=item i_ppix_d(im, x, y, col)

Internal function.

This is the function kept in the i_f_ppix member of an i_img object.
It does a normal store of a pixel into the image with range checking.

Returns 0 if the pixel could be set, -1 otherwise.

=cut
*/
static
int
i_ppix_d(i_img *im, int x, int y, const i_color *val) {
  int ch;
  
  if ( x>-1 && x<im->xsize && y>-1 && y<im->ysize ) {
    for(ch=0;ch<im->channels;ch++)
      if (im->ch_mask&(1<<ch)) 
	im->idata[(x+y*im->xsize)*im->channels+ch]=val->channel[ch];
    return 0;
  }
  return -1; /* error was clipped */
}

/*
=item i_gpix_d(im, x, y, &col)

Internal function.

This is the function kept in the i_f_gpix member of an i_img object.
It does normal retrieval of a pixel from the image with range checking.

Returns 0 if the pixel could be set, -1 otherwise.

=cut
*/
static
int 
i_gpix_d(i_img *im, int x, int y, i_color *val) {
  int ch;
  if (x>-1 && x<im->xsize && y>-1 && y<im->ysize) {
    for(ch=0;ch<im->channels;ch++) 
      val->channel[ch]=im->idata[(x+y*im->xsize)*im->channels+ch];
    return 0;
  }
  for(ch=0;ch<im->channels;ch++) val->channel[ch] = 0;
  return -1; /* error was cliped */
}

/*
=item i_glin_d(im, l, r, y, vals)

Reads a line of data from the image, storing the pixels at vals.

The line runs from (l,y) inclusive to (r,y) non-inclusive

vals should point at space for (r-l) pixels.

l should never be less than zero (to avoid confusion about where to
put the pixels in vals).

Returns the number of pixels copied (eg. if r, l or y is out of range)

=cut
*/
static
int
i_glin_d(i_img *im, int l, int r, int y, i_color *vals) {
  int ch, count, i;
  unsigned char *data;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch)
	vals[i].channel[ch] = *data++;
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_plin_d(im, l, r, y, vals)

Writes a line of data into the image, using the pixels at vals.

The line runs from (l,y) inclusive to (r,y) non-inclusive

vals should point at (r-l) pixels.

l should never be less than zero (to avoid confusion about where to
get the pixels in vals).

Returns the number of pixels copied (eg. if r, l or y is out of range)

=cut
*/
static
int
i_plin_d(i_img *im, int l, int r, int y, const i_color *vals) {
  int ch, count, i;
  unsigned char *data;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
	if (im->ch_mask & (1 << ch)) 
	  *data = vals[i].channel[ch];
	++data;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_ppixf_d(im, x, y, val)

=cut
*/
static
int
i_ppixf_d(i_img *im, int x, int y, const i_fcolor *val) {
  int ch;
  
  if ( x>-1 && x<im->xsize && y>-1 && y<im->ysize ) {
    for(ch=0;ch<im->channels;ch++)
      if (im->ch_mask&(1<<ch)) {
	im->idata[(x+y*im->xsize)*im->channels+ch] = 
          SampleFTo8(val->channel[ch]);
      }
    return 0;
  }
  return -1; /* error was clipped */
}

/*
=item i_gpixf_d(im, x, y, val)

=cut
*/
static
int
i_gpixf_d(i_img *im, int x, int y, i_fcolor *val) {
  int ch;
  if (x>-1 && x<im->xsize && y>-1 && y<im->ysize) {
    for(ch=0;ch<im->channels;ch++) {
      val->channel[ch] = 
        Sample8ToF(im->idata[(x+y*im->xsize)*im->channels+ch]);
    }
    return 0;
  }
  return -1; /* error was cliped */
}

/*
=item i_glinf_d(im, l, r, y, vals)

Reads a line of data from the image, storing the pixels at vals.

The line runs from (l,y) inclusive to (r,y) non-inclusive

vals should point at space for (r-l) pixels.

l should never be less than zero (to avoid confusion about where to
put the pixels in vals).

Returns the number of pixels copied (eg. if r, l or y is out of range)

=cut
*/
static
int
i_glinf_d(i_img *im, int l, int r, int y, i_fcolor *vals) {
  int ch, count, i;
  unsigned char *data;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch)
	vals[i].channel[ch] = Sample8ToF(*data++);
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_plinf_d(im, l, r, y, vals)

Writes a line of data into the image, using the pixels at vals.

The line runs from (l,y) inclusive to (r,y) non-inclusive

vals should point at (r-l) pixels.

l should never be less than zero (to avoid confusion about where to
get the pixels in vals).

Returns the number of pixels copied (eg. if r, l or y is out of range)

=cut
*/
static
int
i_plinf_d(i_img *im, int l, int r, int y, const i_fcolor *vals) {
  int ch, count, i;
  unsigned char *data;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
	if (im->ch_mask & (1 << ch)) 
	  *data = SampleFTo8(vals[i].channel[ch]);
	++data;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_gsamp_d(i_img *im, int l, int r, int y, i_sample_t *samps, int *chans, int chan_count)

Reads sample values from im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples read (which should be (r-l) * bits_set(chan_mask)

=cut
*/
static
int
i_gsamp_d(i_img *im, int l, int r, int y, i_sample_t *samps, 
              const int *chans, int chan_count) {
  int ch, count, i, w;
  unsigned char *data;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
          i_push_errorf(0, "No channel %d in this image", chans[ch]);
          return 0;
        }
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = data[chans[ch]];
          ++count;
        }
        data += im->channels;
      }
    }
    else {
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = data[ch];
          ++count;
        }
        data += im->channels;
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_gsampf_d(i_img *im, int l, int r, int y, i_fsample_t *samps, int *chans, int chan_count)

Reads sample values from im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chan_mask, where bit 0 is the first
channel.

Returns the number of samples read (which should be (r-l) * bits_set(chan_mask)

=cut
*/
static
int
i_gsampf_d(i_img *im, int l, int r, int y, i_fsample_t *samps, 
           const int *chans, int chan_count) {
  int ch, count, i, w;
  unsigned char *data;
  for (ch = 0; ch < chan_count; ++ch) {
    if (chans[ch] < 0 || chans[ch] >= im->channels) {
      i_push_errorf(0, "No channel %d in this image", chans[ch]);
    }
  }
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
          i_push_errorf(0, "No channel %d in this image", chans[ch]);
          return 0;
        }
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = Sample8ToF(data[chans[ch]]);
          ++count;
        }
        data += im->channels;
      }
    }
    else {
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = Sample8ToF(data[ch]);
          ++count;
        }
        data += im->channels;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=back

=head2 Image method wrappers

These functions provide i_fsample_t functions in terms of their
i_sample_t versions.

=over

=item i_ppixf_fp(i_img *im, int x, int y, i_fcolor *pix)

=cut
*/

int i_ppixf_fp(i_img *im, int x, int y, const i_fcolor *pix) {
  i_color temp;
  int ch;

  for (ch = 0; ch < im->channels; ++ch)
    temp.channel[ch] = SampleFTo8(pix->channel[ch]);
  
  return i_ppix(im, x, y, &temp);
}

/*
=item i_gpixf_fp(i_img *im, int x, int y, i_fcolor *pix)

=cut
*/
int i_gpixf_fp(i_img *im, int x, int y, i_fcolor *pix) {
  i_color temp;
  int ch;

  if (i_gpix(im, x, y, &temp)) {
    for (ch = 0; ch < im->channels; ++ch)
      pix->channel[ch] = Sample8ToF(temp.channel[ch]);
    return 0;
  }
  else 
    return -1;
}

/*
=item i_plinf_fp(i_img *im, int l, int r, int y, i_fcolor *pix)

=cut
*/
int i_plinf_fp(i_img *im, int l, int r, int y, const i_fcolor *pix) {
  i_color *work;

  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    if (r > l) {
      int ret;
      int i, ch;
      work = mymalloc(sizeof(i_color) * (r-l));
      for (i = 0; i < r-l; ++i) {
        for (ch = 0; ch < im->channels; ++ch) 
          work[i].channel[ch] = SampleFTo8(pix[i].channel[ch]);
      }
      ret = i_plin(im, l, r, y, work);
      myfree(work);

      return ret;
    }
    else {
      return 0;
    }
  }
  else {
    return 0;
  }
}

/*
=item i_glinf_fp(i_img *im, int l, int r, int y, i_fcolor *pix)

=cut
*/
int i_glinf_fp(i_img *im, int l, int r, int y, i_fcolor *pix) {
  i_color *work;

  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    if (r > l) {
      int ret;
      int i, ch;
      work = mymalloc(sizeof(i_color) * (r-l));
      ret = i_plin(im, l, r, y, work);
      for (i = 0; i < r-l; ++i) {
        for (ch = 0; ch < im->channels; ++ch) 
          pix[i].channel[ch] = Sample8ToF(work[i].channel[ch]);
      }
      myfree(work);

      return ret;
    }
    else {
      return 0;
    }
  }
  else {
    return 0;
  }
}

/*
=item i_gsampf_fp(i_img *im, int l, int r, int y, i_fsample_t *samp, int *chans, int chan_count)

=cut
*/
int i_gsampf_fp(i_img *im, int l, int r, int y, i_fsample_t *samp, 
                int const *chans, int chan_count) {
  i_sample_t *work;

  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    if (r > l) {
      int ret;
      int i;
      work = mymalloc(sizeof(i_sample_t) * (r-l));
      ret = i_gsamp(im, l, r, y, work, chans, chan_count);
      for (i = 0; i < ret; ++i) {
          samp[i] = Sample8ToF(work[i]);
      }
      myfree(work);

      return ret;
    }
    else {
      return 0;
    }
  }
  else {
    return 0;
  }
}

/*
=back

=head2 Palette wrapper functions

Used for virtual images, these forward palette calls to a wrapped image, 
assuming the wrapped image is the first pointer in the structure that 
im->ext_data points at.

=over

=item i_addcolors_forward(i_img *im, const i_color *colors, int count)

=cut
*/
int i_addcolors_forward(i_img *im, const i_color *colors, int count) {
  return i_addcolors(*(i_img **)im->ext_data, colors, count);
}

/*
=item i_getcolors_forward(i_img *im, int i, i_color *color, int count)

=cut
*/
int i_getcolors_forward(i_img *im, int i, i_color *color, int count) {
  return i_getcolors(*(i_img **)im->ext_data, i, color, count);
}

/*
=item i_setcolors_forward(i_img *im, int i, const i_color *color, int count)

=cut
*/
int i_setcolors_forward(i_img *im, int i, const i_color *color, int count) {
  return i_setcolors(*(i_img **)im->ext_data, i, color, count);
}

/*
=item i_colorcount_forward(i_img *im)

=cut
*/
int i_colorcount_forward(i_img *im) {
  return i_colorcount(*(i_img **)im->ext_data);
}

/*
=item i_maxcolors_forward(i_img *im)

=cut
*/
int i_maxcolors_forward(i_img *im) {
  return i_maxcolors(*(i_img **)im->ext_data);
}

/*
=item i_findcolor_forward(i_img *im, const i_color *color, i_palidx *entry)

=cut
*/
int i_findcolor_forward(i_img *im, const i_color *color, i_palidx *entry) {
  return i_findcolor(*(i_img **)im->ext_data, color, entry);
}

/*
=back

=head2 Stream reading and writing wrapper functions

=over

=item i_gen_reader(i_gen_read_data *info, char *buf, int length)

Performs general read buffering for file readers that permit reading
to be done through a callback.

The final callback gets two parameters, a I<need> value, and a I<want>
value, where I<need> is the amount of data that the file library needs
to read, and I<want> is the amount of space available in the buffer
maintained by these functions.

This means if you need to read from a stream that you don't know the
length of, you can return I<need> bytes, taking the performance hit of
possibly expensive callbacks (eg. back to perl code), or if you are
reading from a stream where it doesn't matter if some data is lost, or
if the total length of the stream is known, you can return I<want>
bytes.

=cut 
*/

int
i_gen_reader(i_gen_read_data *gci, char *buf, int length) {
  int total;

  if (length < gci->length - gci->cpos) {
    /* simplest case */
    memcpy(buf, gci->buffer+gci->cpos, length);
    gci->cpos += length;
    return length;
  }
  
  total = 0;
  memcpy(buf, gci->buffer+gci->cpos, gci->length-gci->cpos);
  total  += gci->length - gci->cpos;
  length -= gci->length - gci->cpos;
  buf    += gci->length - gci->cpos;
  if (length < (int)sizeof(gci->buffer)) {
    int did_read;
    int copy_size;
    while (length
	   && (did_read = (gci->cb)(gci->userdata, gci->buffer, length, 
				    sizeof(gci->buffer))) > 0) {
      gci->cpos = 0;
      gci->length = did_read;

      copy_size = i_min(length, gci->length);
      memcpy(buf, gci->buffer, copy_size);
      gci->cpos += copy_size;
      buf += copy_size;
      total += copy_size;
      length -= copy_size;
    }
  }
  else {
    /* just read the rest - too big for our buffer*/
    int did_read;
    while ((did_read = (gci->cb)(gci->userdata, buf, length, length)) > 0) {
      length -= did_read;
      total += did_read;
      buf += did_read;
    }
  }
  return total;
}

/*
=item i_gen_read_data_new(i_read_callback_t cb, char *userdata)

For use by callback file readers to initialize the reader buffer.

Allocates, initializes and returns the reader buffer.

See also L<image.c/free_gen_read_data> and L<image.c/i_gen_reader>.

=cut
*/
i_gen_read_data *
i_gen_read_data_new(i_read_callback_t cb, char *userdata) {
  i_gen_read_data *self = mymalloc(sizeof(i_gen_read_data));
  self->cb = cb;
  self->userdata = userdata;
  self->length = 0;
  self->cpos = 0;

  return self;
}

/*
=item i_free_gen_read_data(i_gen_read_data *)

Cleans up.

=cut
*/
void i_free_gen_read_data(i_gen_read_data *self) {
  myfree(self);
}

/*
=item i_gen_writer(i_gen_write_data *info, char const *data, int size)

Performs write buffering for a callback based file writer.

Failures are considered fatal, if a write fails then data will be
dropped.

=cut
*/
int 
i_gen_writer(
i_gen_write_data *self, 
char const *data, 
int size)
{
  if (self->filledto && self->filledto+size > self->maxlength) {
    if (self->cb(self->userdata, self->buffer, self->filledto)) {
      self->filledto = 0;
    }
    else {
      self->filledto = 0;
      return 0;
    }
  }
  if (self->filledto+size <= self->maxlength) {
    /* just save it */
    memcpy(self->buffer+self->filledto, data, size);
    self->filledto += size;
    return 1;
  }
  /* doesn't fit - hand it off */
  return self->cb(self->userdata, data, size);
}

/*
=item i_gen_write_data_new(i_write_callback_t cb, char *userdata, int max_length)

Allocates and initializes the data structure used by i_gen_writer.

This should be released with L<image.c/i_free_gen_write_data>

=cut
*/
i_gen_write_data *i_gen_write_data_new(i_write_callback_t cb, 
				       char *userdata, int max_length)
{
  i_gen_write_data *self = mymalloc(sizeof(i_gen_write_data));
  self->cb = cb;
  self->userdata = userdata;
  self->maxlength = i_min(max_length, sizeof(self->buffer));
  if (self->maxlength < 0)
    self->maxlength = sizeof(self->buffer);
  self->filledto = 0;

  return self;
}

/*
=item i_free_gen_write_data(i_gen_write_data *info, int flush)

Cleans up the write buffer.

Will flush any left-over data if I<flush> is non-zero.

Returns non-zero if flush is zero or if info->cb() returns non-zero.

Return zero only if flush is non-zero and info->cb() returns zero.
ie. if it fails.

=cut
*/

int i_free_gen_write_data(i_gen_write_data *info, int flush)
{
  int result = !flush || 
    info->filledto == 0 ||
    info->cb(info->userdata, info->buffer, info->filledto);
  myfree(info);

  return result;
}

struct magic_entry {
  unsigned char *magic;
  size_t magic_size;
  char *name;
  unsigned char *mask;  
};

static int
test_magic(unsigned char *buffer, size_t length, struct magic_entry const *magic) {
  if (length < magic->magic_size)
    return 0;
  if (magic->mask) {
    int i;
    unsigned char *bufp = buffer, 
      *maskp = magic->mask, 
      *magicp = magic->magic;

    for (i = 0; i < magic->magic_size; ++i) {
      int mask = *maskp == 'x' ? 0xFF : *maskp == ' ' ? 0 : *maskp;
      ++maskp;

      if ((*bufp++ & mask) != (*magicp++ & mask)) 
	return 0;
    }

    return 1;
  }
  else {
    return !memcmp(magic->magic, buffer, magic->magic_size);
  }
}

/*
=item i_test_format_probe(io_glue *data, int length)

Check the beginning of the supplied file for a 'magic number'

=cut
*/

#define FORMAT_ENTRY(magic, type) \
  { (unsigned char *)(magic ""), sizeof(magic)-1, type }
#define FORMAT_ENTRY2(magic, type, mask) \
  { (unsigned char *)(magic ""), sizeof(magic)-1, type, (unsigned char *)(mask) }

const char *
i_test_format_probe(io_glue *data, int length) {
  static const struct magic_entry formats[] = {
    FORMAT_ENTRY("\xFF\xD8", "jpeg"),
    FORMAT_ENTRY("GIF87a", "gif"),
    FORMAT_ENTRY("GIF89a", "gif"),
    FORMAT_ENTRY("MM\0*", "tiff"),
    FORMAT_ENTRY("II*\0", "tiff"),
    FORMAT_ENTRY("BM", "bmp"),
    FORMAT_ENTRY("\x89PNG\x0d\x0a\x1a\x0a", "png"),
    FORMAT_ENTRY("P1", "pnm"),
    FORMAT_ENTRY("P2", "pnm"),
    FORMAT_ENTRY("P3", "pnm"),
    FORMAT_ENTRY("P4", "pnm"),
    FORMAT_ENTRY("P5", "pnm"),
    FORMAT_ENTRY("P6", "pnm"),
    FORMAT_ENTRY("/* XPM", "xpm"),
    FORMAT_ENTRY("\x8aMNG", "mng"),
    FORMAT_ENTRY("\x8aJNG", "jng"),
    /* SGI RGB - with various possible parameters to avoid false positives
       on similar files 
       values are: 2 byte magic, rle flags (0 or 1), bytes/sample (1 or 2)
    */
    FORMAT_ENTRY("\x01\xDA\x00\x01", "sgi"),
    FORMAT_ENTRY("\x01\xDA\x00\x02", "sgi"),
    FORMAT_ENTRY("\x01\xDA\x01\x01", "sgi"),
    FORMAT_ENTRY("\x01\xDA\x01\x02", "sgi"),
    
    FORMAT_ENTRY2("FORM    ILBM", "ilbm", "xxxx    xxxx"),

    /* different versions of PCX format 
       http://www.fileformat.info/format/pcx/
    */
    FORMAT_ENTRY("\x0A\x00\x01", "pcx"),
    FORMAT_ENTRY("\x0A\x02\x01", "pcx"),
    FORMAT_ENTRY("\x0A\x03\x01", "pcx"),
    FORMAT_ENTRY("\x0A\x04\x01", "pcx"),
    FORMAT_ENTRY("\x0A\x05\x01", "pcx"),

    /* FITS - http://fits.gsfc.nasa.gov/ */
    FORMAT_ENTRY("SIMPLE  =", "fits"),

    /* PSD - Photoshop */
    FORMAT_ENTRY("8BPS\x00\x01", "psd"),
    
    /* EPS - Encapsulated Postscript */
    /* only reading 18 chars, so we don't include the F in EPSF */
    FORMAT_ENTRY("%!PS-Adobe-2.0 EPS", "eps"),

    /* Utah RLE */
    FORMAT_ENTRY("\x52\xCC", "utah"),

    /* GZIP compressed, only matching deflate for now */
    FORMAT_ENTRY("\x1F\x8B\x08", "gzip"),

    /* bzip2 compressed */
    FORMAT_ENTRY("BZh", "bzip2"),
  };
  static const struct magic_entry more_formats[] = {
    /* these were originally both listed as ico, but cur files can
       include hotspot information */
    FORMAT_ENTRY("\x00\x00\x01\x00", "ico"), /* Windows icon */
    FORMAT_ENTRY("\x00\x00\x02\x00", "cur"), /* Windows cursor */
    FORMAT_ENTRY2("\x00\x00\x00\x00\x00\x00\x00\x07", 
		  "xwd", "    xxxx"), /* X Windows Dump */
  };

  unsigned int i;
  unsigned char head[18];
  ssize_t rc;

  io_glue_commit_types(data);
  rc = data->readcb(data, head, 18);
  if (rc == -1) return NULL;
  data->seekcb(data, -rc, SEEK_CUR);

  for(i=0; i<sizeof(formats)/sizeof(formats[0]); i++) { 
    struct magic_entry const *entry = formats + i;

    if (test_magic(head, rc, entry)) 
      return entry->name;
  }

  if ((rc == 18) &&
      tga_header_verify(head))
    return "tga";

  for(i=0; i<sizeof(more_formats)/sizeof(more_formats[0]); i++) { 
    struct magic_entry const *entry = more_formats + i;

    if (test_magic(head, rc, entry)) 
      return entry->name;
  }

  return NULL;
}

/*
=item i_img_is_monochrome(img, &zero_is_white)

Tests an image to check it meets our monochrome tests.

The idea is that a file writer can use this to test where it should
write the image in whatever bi-level format it uses, eg. pbm for pnm.

For performance of encoders we require monochrome images:

=over

=item *

be paletted

=item *

have a palette of two colors, containing only (0,0,0) and
(255,255,255) in either order.

=back

zero_is_white is set to non-zero iff the first palette entry is white.

=cut
*/

int
i_img_is_monochrome(i_img *im, int *zero_is_white) {
  if (im->type == i_palette_type
      && i_colorcount(im) == 2) {
    i_color colors[2];
    i_getcolors(im, 0, colors, 2);
    if (im->channels == 3) {
      if (colors[0].rgb.r == 255 && 
          colors[0].rgb.g == 255 &&
          colors[0].rgb.b == 255 &&
          colors[1].rgb.r == 0 &&
          colors[1].rgb.g == 0 &&
          colors[1].rgb.b == 0) {
        *zero_is_white = 0;
        return 1;
      }
      else if (colors[0].rgb.r == 0 && 
               colors[0].rgb.g == 0 &&
               colors[0].rgb.b == 0 &&
               colors[1].rgb.r == 255 &&
               colors[1].rgb.g == 255 &&
               colors[1].rgb.b == 255) {
        *zero_is_white = 1;
        return 1;
      }
    }
    else if (im->channels == 1) {
      if (colors[0].channel[0] == 255 &&
          colors[1].channel[1] == 0) {
        *zero_is_white = 0;
        return 1;
      }
      else if (colors[0].channel[0] == 0 &&
               colors[0].channel[0] == 255) {
        *zero_is_white = 1;
        return 1;         
      }
    }
  }

  *zero_is_white = 0;
  return 0;
}

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

L<Imager>, L<gif.c>

=cut
*/
