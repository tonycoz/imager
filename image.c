#define IMAGER_NO_CONTEXT

#include "imager.h"
#include "imageri.h"
#include <limits.h>
#include <assert.h>

/*
=head1 NAME

image.c - implements most of the basic functions of Imager and much of the rest

=head1 SYNOPSIS

  i_img *i;
  i_color *c;
  c = i_color_new(red, green, blue, alpha);
  ICL_DESTROY(c);
  i = i_img_8_new();
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

im_context_t (*im_get_context)(void) = NULL;

/*
=item im_img_alloc()
X<im_img_alloc API>X<i_img_alloc API>
=category Image Implementation
=synopsis i_img *im = im_img_alloc(aIMCTX);
=synopsis i_img *im = i_img_alloc();

Allocates a new i_img structure.

When implementing a new image type perform the following steps in your
image object creation function:

=over

=item 1.

allocate the image with i_img_new(), supplying the virtual table
pointer, width, height, number of channels, number of extra channels
and bit depth.

=item 2.

make any other modifications needed, such as marking the image as
linear or virtual, allocating image data for non-virtual images.

=item 3.

initialize Imager's internal data by calling i_img_init() on the image
object.

=back

=cut
*/

i_img *
im_img_alloc(pIMCTX) {
  i_img *p = mymalloc(sizeof(i_img));
  memset(p, 0, sizeof(i_img));
  return p;
}

/*
=item im_img_new()
=synopsis i_img *im = im_img_new(aIMCTX, &myvtbl, xsize, ysize, chans, extrachans, i_8_bits);

Allocate a new image object, as with im_img_alloc(), and populate
common fields from the supplied parameters.

=cut
 */

i_img *
im_img_new(pIMCTX, const i_img_vtable *vtbl, i_img_dim xsize, i_img_dim ysize,
           int channels, int extra, i_img_bits_t bits) {
  i_img *im = im_img_alloc(aIMCTX);

  im->vtbl          = vtbl;
  im->xsize         = xsize;
  im->ysize         = ysize;
  im->channels      = channels;
  im->extrachannels = extra;
  im->bits          = bits;
  im->type          = i_direct_type;
  im->islinear      = 0;
  im->ch_mask       = ~0U;
  im->ref_count     = 1U;
  im->isvirtual     = 0;
  im->bytes         = 0;
  im->idata         = 0;
  im->ext_data      = 0;
  i_tags_new(&im->tags);

  return im;
}

/*
=item im_img_init(aIMCTX, image)
X<im_img_init API>X<i_img_init API>
=category Image Implementation
=synopsis im_img_init(aIMCTX, im);
=synopsis i_img_init(im);

Imager internal initialization of images.

See L</im_img_alloc()> for more information.

=cut
*/

void
im_img_init(pIMCTX, i_img *img) {
  img->im_data = NULL;
  img->context = aIMCTX;
  im_context_refinc(aIMCTX, "img_init");
}

/*
=item i_check_image_entries()

Validates that an image has all the entries required for its type.

  int ok = i_check_image_entries(img);

Returns true on success.

Pushes an error for each error found, if any.

=cut
*/

int
i_img_check_entries(i_img *im) {
  dIMCTXim(im);
  int ok = 1;
  const i_img_vtable *vtbl = im->vtbl;

  im_clear_error(aIMCTX);

  if (!vtbl) {
    im_push_error(aIMCTX, 0, "vtbl NULL");
    return 0;
  }
  if (!vtbl->i_f_gsamp) {
    im_push_error(aIMCTX, 0, "no i_f_gsamp");
    ok = 0;
  }
  if (!vtbl->i_f_gsampf) {
    im_push_error(aIMCTX, 0, "no i_f_gsampf");
    ok = 0;
  }
  if (!vtbl->i_f_psamp) {
    im_push_error(aIMCTX, 0, "no i_f_psamp");
    ok = 0;
  }
  if (!vtbl->i_f_psampf) {
    im_push_error(aIMCTX, 0, "no i_f_psampf");
    ok = 0;
  }
  /* i_f_destroy isn't required */
  if (!vtbl->i_f_gsamp_bits) {
    im_push_error(aIMCTX, 0, "no i_f_gsamp_bits");
    ok = 0;
  }
  if (!vtbl->i_f_psamp_bits) {
    im_push_error(aIMCTX, 0, "no i_f_psamp_bits");
    ok = 0;
  }
  if (!vtbl->i_f_gslin) {
    im_push_error(aIMCTX, 0, "no i_f_gslin");
    ok = 0;
  }
  if (!vtbl->i_f_gslinf) {
    im_push_error(aIMCTX, 0, "no i_f_gslinf");
    ok = 0;
  }
  if (!vtbl->i_f_pslin) {
    im_push_error(aIMCTX, 0, "no i_f_pslin");
    ok = 0;
  }
  if (!vtbl->i_f_pslinf) {
    im_push_error(aIMCTX, 0, "no i_f_pslinf");
    ok = 0;
  }
  if (!vtbl->i_f_data) {
    im_push_error(aIMCTX, 0, "no i_f_data");
    ok = 0;
  }
  /* i_f_imageop isn't required */
  if (im->type == i_palette_type) {
    if (!vtbl->i_f_gpal) {
      im_push_error(aIMCTX, 0, "no i_f_gpal for type == i_palette_type");
      ok = 0;
    }
    if (!vtbl->i_f_ppal) {
      im_push_error(aIMCTX, 0, "no i_f_ppal for type == i_palette_type");
      ok = 0;
    }
    if (!vtbl->i_f_addcolors) {
      im_push_error(aIMCTX, 0, "no i_f_addcolors for type == i_palette_type");
      ok = 0;
    }
    if (!vtbl->i_f_getcolors) {
      im_push_error(aIMCTX, 0, "no i_f_getcolors for type == i_palette_type");
      ok = 0;
    }
    if (!vtbl->i_f_colorcount) {
      im_push_error(aIMCTX, 0, "no i_f_colorcount for type == i_palette_type");
      ok = 0;
    }
    if (!vtbl->i_f_maxcolors) {
      im_push_error(aIMCTX, 0, "no i_f_maxcolors for type == i_palette_type");
      ok = 0;
    }
    if (!vtbl->i_f_findcolor) {
      im_push_error(aIMCTX, 0, "no i_f_findcolor for type == i_palette_type");
      ok = 0;
    }
    if (!vtbl->i_f_setcolors) {
      im_push_error(aIMCTX, 0, "no i_f_setcolors for type == i_palette_type");
      ok = 0;
    }
  }

  return ok;
}

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
  dIMCTX;

  im_log((aIMCTX,1,"ICL_new_internal(r %d,g %d,b %d,a %d)\n", r, g, b, a));

  if ( (cl=mymalloc(sizeof(i_color))) == NULL) im_fatal(aIMCTX, 2,"malloc() error\n");
  cl->rgba.r = r;
  cl->rgba.g = g;
  cl->rgba.b = b;
  cl->rgba.a = a;
  im_log((aIMCTX,1,"(%p) <- ICL_new_internal\n",cl));
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
  dIMCTX;
  im_log((aIMCTX,1,"ICL_set_internal(cl* %p,r %d,g %d,b %d,a %d)\n",cl,r,g,b,a));
  if (cl == NULL)
    if ( (cl=mymalloc(sizeof(i_color))) == NULL)
      im_fatal(aIMCTX, 2,"malloc() error\n");
  cl->rgba.r=r;
  cl->rgba.g=g;
  cl->rgba.b=b;
  cl->rgba.a=a;
  im_log((aIMCTX,1,"(%p) <- ICL_set_internal\n",cl));
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
  dIMCTX;
  im_log((aIMCTX, 1,"i_color_info(cl* %p)\n",cl));
  im_log((aIMCTX, 1,"i_color_info: (%d,%d,%d,%d)\n",cl->rgba.r,cl->rgba.g,cl->rgba.b,cl->rgba.a));
}

/* 
=item ICL_DESTROY

Destroy ancillary data for Color object.

   cl - pointer to color object

=cut
*/

void
ICL_DESTROY(i_color *cl) {
  dIMCTX;
  im_log((aIMCTX, 1,"ICL_DESTROY(cl* %p)\n",cl));
  myfree(cl);
}

/*
=item i_fcolor_new(double r, double g, double b, double a)

=cut
*/
i_fcolor *i_fcolor_new(double r, double g, double b, double a) {
  i_fcolor *cl = NULL;
  dIMCTX;

  im_log((aIMCTX, 1,"i_fcolor_new(r %g,g %g,b %g,a %g)\n", r, g, b, a));

  if ( (cl=mymalloc(sizeof(i_fcolor))) == NULL) im_fatal(aIMCTX, 2,"malloc() error\n");
  cl->rgba.r = r;
  cl->rgba.g = g;
  cl->rgba.b = b;
  cl->rgba.a = a;
  im_log((aIMCTX, 1,"(%p) <- i_fcolor_new\n",cl));

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
=item i_img_exorcise(im)

Free image data.

   im - Image pointer

=cut
*/

void
i_img_exorcise(i_img *im) {
  dIMCTXim(im);
  im_log((aIMCTX,1,"i_img_exorcise(im* %p)\n",im));
  i_tags_destroy(&im->tags);
  if (im->vtbl->i_f_destroy)
    (im->vtbl->i_f_destroy)(im);
  if (im->idata != NULL) { myfree(im->idata); }
  im->idata    = NULL;
  im->xsize    = 0;
  im->ysize    = 0;
  im->channels = 0;
  /* make most access crash */
  im->vtbl     = NULL;

  im->ext_data=NULL;
}

/* 
=item i_img_destroy()
=order 90
=category Image creation/destruction
=synopsis i_img_destroy(img)

Reduce the reference count of an image object by 1, and if the
reference count reaches zero, destroy the image object.

This is unrelated to Perl C<SV> reference counts.

=cut
*/

void
i_img_destroy(i_img *im) {
  dIMCTXim(im);
  im_log((aIMCTX, 1,"i_img_destroy(im %p)\n",im));

  if (im->ref_count > 1) {
    --im->ref_count;
    return;
  }
  assert(im->ref_count == 1);

  i_img_exorcise(im);
  myfree(im);
  im_context_refdec(aIMCTX, "img_destroy");
}

/*
=item i_img_refcnt_inc()

Increment the reference count of the image.

Aborts the program if the reference count overflows.

This is unrelated to Perl C<SV> reference counts.

=cut
*/

void
i_img_refcnt_inc(i_img *im) {
  if (im->ref_count == UINT_MAX) {
    dIMCTXim(im);
    im_fatal(aIMCTX, 3, "Integer overflow on image reference count");
  }
  ++im->ref_count;
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
i_img_info(i_img *im, i_img_dim *info) {
  dIMCTXim(im);
  im_log((aIMCTX,1,"i_img_info(im %p)\n",im));

  im_log((aIMCTX,1,"i_img_info: xsize=%" i_DF " ysize=%" i_DF " channels=%d "
	  "mask=%ud\n",
	  i_DFc(im->xsize), i_DFc(im->ysize), im->channels,im->ch_mask));
  im_log((aIMCTX,1,"i_img_info: idata=%p\n",im->idata));
  info[0] = im->xsize;
  info[1] = im->ysize;
  info[2] = im->channels;
  info[3] = im->ch_mask;
}

/*
=item i_img_setmask(C<im>, C<ch_mask>)
=category Image Information
=synopsis // only channel 0 writable 
=synopsis i_img_setmask(img, 0x01);

Set the image channel mask for C<im> to C<ch_mask>.

The image channel mask gives some control over which channels can be
written to in the image.

=cut
*/
void
i_img_setmask(i_img *im,int ch_mask) { im->ch_mask=ch_mask; }


/*
=item i_img_getmask(C<im>)
=category Image Information
=synopsis int mask = i_img_getmask(img);

Get the image channel mask for C<im>.

=cut
*/
int
i_img_getmask(i_img *im) { return im->ch_mask; }

/*
=item i_copyto_trans(C<im>, C<src>, C<x1>, C<y1>, C<x2>, C<y2>, C<tx>, C<ty>, C<trans>)

=category Image

(C<x1>,C<y1>) (C<x2>,C<y2>) specifies the region to copy (in the
source coordinates) (C<tx>,C<ty>) specifies the upper left corner for
the target image.  pass NULL in C<trans> for non transparent i_colors.

=cut
*/

void
i_copyto_trans(i_img *im,i_img *src,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,i_img_dim tx,i_img_dim ty,const i_color *trans) {
  i_color pv;
  i_img_dim x,y,t,ttx,tty,tt;
  int ch;
  dIMCTXim(im);

  im_log((aIMCTX, 1,"i_copyto_trans(im* %p,src %p, p1(" i_DFp "), p2(" i_DFp "), "
	  "to(" i_DFp "), trans* %p)\n",
	  im, src, i_DFcp(x1, y1), i_DFcp(x2, y2), i_DFcp(tx, ty), trans));
  
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
=item i_copy(source)

=category Image

Creates a new image that is a copy of the image C<source>.

Tags are not copied, only the image data.

Returns: i_img *

=cut
*/

i_img *
i_copy(i_img *src) {
  i_img_dim y, y1, x1;
  dIMCTXim(src);
  i_img *im = i_sametype(src, src->xsize, src->ysize);

  im_log((aIMCTX,1,"i_copy(src %p)\n", src));

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
    i_palidx *vals;

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
=item i_sametype(C<im>, C<xsize>, C<ysize>)

=category Image creation/destruction
=synopsis i_img *img = i_sametype(src, width, height);

Returns an image of the same type (sample size, channels, paletted/direct).

For paletted images the palette is copied from the source.

=cut
*/

i_img *
i_sametype(i_img *src, i_img_dim xsize, i_img_dim ysize) {
  dIMCTXim(src);

  if (src->type == i_direct_type) {
    if (src->bits == 8) {
      return i_img_8_new(xsize, ysize, src->channels);
    }
    else if (src->bits == i_16_bits) {
      return i_img_16_new(xsize, ysize, src->channels);
    }
    else if (src->bits == i_double_bits) {
      return i_img_double_new(xsize, ysize, src->channels);
    }
    else if (src->bits == i_float_bits) {
      return i_img_float_new(xsize, ysize, src->channels);
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
=item i_sametype_chans(C<im>, C<xsize>, C<ysize>, C<channels>)

=category Image creation/destruction
=synopsis i_img *img = i_sametype_chans(src, width, height, channels);

Returns an image of the same type (sample size).

For paletted images the equivalent direct type is returned.

=cut
*/

i_img *
i_sametype_chans(i_img *src, i_img_dim xsize, i_img_dim ysize, int channels) {
  dIMCTXim(src);

  if (src->bits == 8) {
    return i_img_8_new(xsize, ysize, channels);
  }
  else if (src->bits == i_16_bits) {
    return i_img_16_new(xsize, ysize, channels);
  }
  else if (src->bits == i_double_bits) {
    return i_img_double_new(xsize, ysize, channels);
  }
  else if (src->bits == i_float_bits) {
    return i_img_float_new(xsize, ysize, channels);
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
  i_img_dim nxsize,nysize,nx,ny;
  i_img *new_img;
  i_color val;
  dIMCTXim(im);
  
  im_log((aIMCTX, 1,"i_transform(im %p, opx %p, opxl %d, opy %p, opyl %d, parm %p, parmlen %d)\n",im,opx,opxl,opy,opyl,parm,parmlen));

  nxsize = im->xsize;
  nysize = im->ysize ;
  
  new_img=i_img_8_new(nxsize, nysize, im->channels);
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

  im_log((aIMCTX, 1,"(%p) <- i_transform\n",new_img));
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
  i_img_dim x, y, xb, yb;
  int ch, chb;
  float tdiff;
  i_color val1,val2;
  dIMCTXim(im1);

  im_log((aIMCTX, 1,"i_img_diff(im1 %p,im2 %p)\n",im1,im2));

  xb=(im1->xsize<im2->xsize)?im1->xsize:im2->xsize;
  yb=(im1->ysize<im2->ysize)?im1->ysize:im2->ysize;
  chb=(im1->channels<im2->channels)?im1->channels:im2->channels;

  im_log((aIMCTX, 1,"i_img_diff: b=(" i_DFp ") chb=%d\n",
	  i_DFcp(xb,yb), chb));

  tdiff=0;
  for(y=0;y<yb;y++) for(x=0;x<xb;x++) {
    i_gpix(im1,x,y,&val1);
    i_gpix(im2,x,y,&val2);

    for(ch=0;ch<chb;ch++) tdiff+=(val1.channel[ch]-val2.channel[ch])*(val1.channel[ch]-val2.channel[ch]);
  }
  im_log((aIMCTX, 1,"i_img_diff <- (%.2f)\n",tdiff));
  return tdiff;
}

/*
=item i_img_diffd(im1, im2)

Calculates the sum of the squares of the differences between
correspoding channels in two images.

If the images are not the same size then only the common area is 
compared, hence even if images are different sizes this function 
can return zero.

This is like i_img_diff() but looks at floating point samples instead.

=cut
*/

double
i_img_diffd(i_img *im1,i_img *im2) {
  i_img_dim x, y, xb, yb;
  int ch, chb;
  double tdiff;
  i_fcolor val1,val2;
  dIMCTXim(im1);

  im_log((aIMCTX, 1,"i_img_diffd(im1 %p,im2 %p)\n",im1,im2));

  xb=(im1->xsize<im2->xsize)?im1->xsize:im2->xsize;
  yb=(im1->ysize<im2->ysize)?im1->ysize:im2->ysize;
  chb=(im1->channels<im2->channels)?im1->channels:im2->channels;

  im_log((aIMCTX, 1,"i_img_diffd: b(" i_DFp ") chb=%d\n",
	  i_DFcp(xb, yb), chb));

  tdiff=0;
  for(y=0;y<yb;y++) for(x=0;x<xb;x++) {
    i_gpixf(im1,x,y,&val1);
    i_gpixf(im2,x,y,&val2);

    for(ch=0;ch<chb;ch++) {
      double sdiff = val1.channel[ch]-val2.channel[ch];
      tdiff += sdiff * sdiff;
    }
  }
  im_log((aIMCTX, 1,"i_img_diffd <- (%.2f)\n",tdiff));

  return tdiff;
}

int
i_img_samef(i_img *im1,i_img *im2, double epsilon, char const *what) {
  i_img_dim x,y,xb,yb;
  int ch, chb;
  i_fcolor val1,val2;
  dIMCTXim(im1);

  if (what == NULL)
    what = "(null)";

  im_log((aIMCTX,1,"i_img_samef(im1 %p,im2 %p, epsilon %g, what '%s')\n", im1, im2, epsilon, what));

  xb=(im1->xsize<im2->xsize)?im1->xsize:im2->xsize;
  yb=(im1->ysize<im2->ysize)?im1->ysize:im2->ysize;
  chb=(im1->channels<im2->channels)?im1->channels:im2->channels;

  im_log((aIMCTX, 1,"i_img_samef: b(" i_DFp ") chb=%d\n",
	  i_DFcp(xb, yb), chb));

  for(y = 0; y < yb; y++) {
    for(x = 0; x < xb; x++) {
      i_gpixf(im1, x, y, &val1);
      i_gpixf(im2, x, y, &val2);
      
      for(ch = 0; ch < chb; ch++) {
	double sdiff = val1.channel[ch] - val2.channel[ch];
	if (fabs(sdiff) > epsilon) {
	  im_log((aIMCTX, 1,"i_img_samef <- different %g @(" i_DFp ")\n",
		  sdiff, i_DFcp(x, y)));
	  return 0;
	}
      }
    }
  }
  im_log((aIMCTX, 1,"i_img_samef <- same\n"));

  return 1;
}

/* just a tiny demo of haar wavelets */

i_img*
i_haar(i_img *im) {
  i_img_dim mx,my;
  i_img_dim fx,fy;
  i_img_dim x,y;
  int ch;
  i_img *new_img,*new_img2;
  i_color val1,val2,dval1,dval2;
  dIMCTXim(im);
  
  mx=im->xsize;
  my=im->ysize;
  fx=(mx+1)/2;
  fy=(my+1)/2;


  /* horizontal pass */
  
  new_img  = i_img_8_new(fx*2, fy*2, im->channels);
  new_img2 = i_img_8_new(fx*2, fy*2, im->channels);

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
  i_img_dim x,y;
  int colorcnt;
  int channels[3];
  int *samp_chans;
  i_sample_t * samp;
  i_img_dim xsize = im->xsize; 
  i_img_dim ysize = im->ysize;
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
	      myfree(samp);
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
  i_img_dim x,y;
  int colorcnt;
  unsigned int *col_usage_it;
  i_sample_t * samp;
  int channels[3];
  int *samp_chans;
  
  i_img_dim xsize = im->xsize; 
  i_img_dim ysize = im->ysize;
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
	myfree(samp);
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

=head2 Image method wrappers

These functions provide i_fsample_t functions in terms of their
i_sample_t versions.

=over

=item i_gsampf_fp()

  count = i_gsampf_fp(im, left, right, y, samps, chans, chan_count);

Implements i_gsampf() for an image in terms of the image's
implementation of i_gsamp().

=cut
*/

i_img_dim
i_gsampf_fp(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samp, 
                int const *chans, int chan_count) {
  i_sample_t *work;

  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    if (r > l) {
      i_img_dim ret;
      i_img_dim i;
      work = mymalloc(sizeof(i_sample_t) * (r-l) * chan_count);
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
=item i_psampf_fp()

  count = i_psampf_fp(im, left, right, y, samps, chans, chan_count);

Implements i_psampf() for an image in terms of the image's
implementation of i_psamp().

=cut
*/

i_img_dim
i_psampf_fp(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samp, 
                int const *chans, int chan_count) {
  i_sample_t *work;

  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    if (r > l) {
      i_img_dim ret = (r - l) * chan_count;
      i_img_dim i;
      work = mymalloc(sizeof(i_sample_t) * (r-l) * chan_count);
      for (i = 0; i < ret; ++i) {
          work[i] = SampleFTo8(samp[i]);
      }
      ret = i_psamp(im, l, r, y, work, chans, chan_count);
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
=item i_gslin_fallback()

Fallback implementation for i_gslin() that fetches samples with
i_gsamp() and converts them to linear.

=cut
*/

i_img_dim
i_gslin_fallback(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
                 i_sample16_t *samp, const int *chans, int chan_count) {
  dIMCTXim(im);

  if (y >= 0 && y < im->ysize && x < im->xsize && x >= 0) {
    i_sample_t *work;
    i_img_dim result;
    int color_chans;
    const imcms_curve_t *curves = im_model_curves(aIMCTX, i_img_color_model(im), &color_chans);
    i_sample16_t *pout = samp;

    if (r > im->xsize)
      r = im->xsize;
    if (x >= r)
      return 0;

    work = mymalloc(sizeof(i_sample_t) * chan_count * (r - x));

    result = i_gsamp(im, x, r, y, work, chans, chan_count);
    if (result > 0) {
      i_sample_t *pwork = work;
      if (chans) {
        while (x < r) {
          int chi;
          for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[ch];
            if (ch < color_chans) {
              *pout++ = imcms_to_linear(curves[ch], *pwork++);
            }
            else {
              *pout++ = Sample8To16(*pwork++);
            }
          }
          ++x;
        }
      }
      else {
        while (x < r) {
          int ch;
          for (ch = 0; ch < chan_count; ++ch) {
            if (ch < color_chans) {
              *pout++ = imcms_to_linear(curves[ch], *pwork++);
            }
            else {
              *pout++ = Sample8To16(*pwork++);
            }
          }
          ++x;
        }
      }
    }

    myfree(work);

    return result;
  }
  else {
    i_push_error(0, "Image position outside of image");
    return 0;
  }
}

/*
=item i_gslinf_fallback()

Fallback implementation for i_gslinf() that fetches samples with
i_gsampf() and converts them to linear.

=cut
*/

i_img_dim
i_gslinf_fallback(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
                  i_fsample_t *samp, const int *chans, int chan_count) {
  dIMCTXim(im);

  if (y >= 0 && y < im->ysize && x < im->xsize && x >= 0) {
    i_fsample_t *work;
    i_img_dim result;
    int color_chans;
    const imcms_curve_t *curves = im_model_curves(aIMCTX, i_img_color_model(im), &color_chans);
    i_fsample_t *pout = samp;

    if (r > im->xsize)
      r = im->xsize;
    if (x >= r)
      return 0;

    work = mymalloc(sizeof(i_fsample_t) * chan_count * (r - x));

    result = i_gsampf(im, x, r, y, work, chans, chan_count);
    if (result > 0) {
      i_fsample_t *pwork = work;
      if (chans) {
        while (x < r) {
          int chi;
          for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[ch];
            if (ch < color_chans) {
              *pout++ = imcms_to_linearf(curves[ch], *pwork++);
            }
            else {
              *pout++ = *pwork++;
            }
          }
          ++x;
        }
      }
      else {
        while (x < r) {
          int ch;
          for (ch = 0; ch < chan_count; ++ch) {
            if (ch < color_chans) {
              *pout++ = imcms_to_linearf(curves[ch], *pwork++);
            }
            else {
              *pout++ = *pwork++;
            }
          }
          ++x;
        }
      }
    }

    myfree(work);

    return result;
  }
  else {
    i_push_error(0, "Image position outside of image");
    return 0;
  }
}

/*
=item i_pslin_fallback()

Fallback implementation for i_pslin() that converts the supplied
linear samples to gamma samples, and calls i_psamp().

=cut
*/

i_img_dim
i_pslin_fallback(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
                 const i_sample16_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);

  if (y >=0 && y < im->ysize && x < im->xsize && x >= 0) {
    i_img_dim i;
    int color_chans;
    const imcms_curve_t *curves = im_model_curves(aIMCTX, i_img_color_model(im), &color_chans);
    i_sample_t *work = NULL;
    i_img_dim result;
    int total_channels = i_img_totalchannels(im);

    if (r > im->xsize)
      r = im->xsize;
    if (x >= r)
      return 0;

    if (chan_count > MAXTOTALCHANNELS) {
      im_push_error(aIMCTX, 0, "Too many channels requested");
      return 0;
    }

    if (chan_count <= 0) {
      im_push_error(aIMCTX, 0, "pslin() must request at least one channel");
      return -1;
    }
    if (chans) {
      int chi;
      for (chi = 0; chi < chan_count; ++chi) {
        if (chans[chi] < 0 || chans[chi] >= total_channels) {
          im_push_errorf(aIMCTX, 0, "Channel %d at %d out of range", chans[chi], chi);
          return 0;
        }
      }
    }
    else {
      if (chan_count > total_channels) {
        im_push_error(aIMCTX, 0, "pslin(): chan_count greater than total channels");
        return -1;
      }
    }

    work = mymalloc(sizeof(i_sample_t) * chan_count * (r - x));
    if (chans) {
      const i_sample16_t *sampsp = samps;
      i_sample_t *workp = work;

      for (i = x; i < r; ++i) {
        int chi;
        for (chi = 0; chi < chan_count; ++chi) {
          int ch = chans[chi];
          if (ch < color_chans)
            *workp++ = imcms_from_linear(curves[ch], *sampsp++);
          else
            *workp++ = Sample16To8(*sampsp++);
        }
      }
    }
    else {
      const i_sample16_t *sampsp = samps;
      i_sample_t *workp = work;

      for (i = x; i < r; ++i) {
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
          if (ch < color_chans)
            *workp++ = imcms_from_linear(curves[ch], *sampsp++);
          else
            *workp++ = Sample16To8(*sampsp++);
        }
      }
    }

    result = i_psamp(im, x, r, y, work, chans, chan_count);

    myfree(work);

    return result;
  }
  else {
    im_push_error(aIMCTX, 0, "Image position outside of image");
    return -1;
  }
}

/*
=item i_pslinf_fallback()

Fallback implementation for i_pslin() that converts the supplied
linear samples to gamma samples, and calls i_psampf().

=cut
*/

i_img_dim
i_pslinf_fallback(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
                  const i_fsample_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);

  if (y >=0 && y < im->ysize && x < im->xsize && x >= 0) {
    i_img_dim i;
    int color_chans;
    const imcms_curve_t *curves = im_model_curves(aIMCTX, i_img_color_model(im), &color_chans);
    i_fsample_t *work = NULL;
    i_img_dim result;
    int total_channels = i_img_totalchannels(im);

    if (r > im->xsize)
      r = im->xsize;
    if (x >= r)
      return 0;

    if (chan_count > MAXTOTALCHANNELS) {
      im_push_error(aIMCTX, 0, "Too many channels requested");
      return 0;
    }

    if (chan_count <= 0) {
      im_push_error(aIMCTX, 0, "pslinf() must request at least one channel");
      return -1;
    }
    if (chans) {
      int chi;
      for (chi = 0; chi < chan_count; ++chi) {
        if (chans[chi] < 0 || chans[chi] >= total_channels) {
          im_push_errorf(aIMCTX, 0, "Channel %d at %d out of range", chans[chi], chi);
          return 0;
        }
      }
    }
    else {
      if (chan_count > total_channels) {
        im_push_error(aIMCTX, 0, "chan_count greater than total channels");
        return -1;
      }
    }

    work = mymalloc(sizeof(i_fsample_t) * chan_count * (r - x));
    if (chans) {
      const i_fsample_t *sampsp = samps;
      i_fsample_t *workp = work;

      for (i = x; i < r; ++i) {
        int chi;
        for (chi = 0; chi < chan_count; ++chi) {
          int ch = chans[chi];
          if (ch < color_chans)
            *workp++ = imcms_from_linearf(curves[ch], *sampsp++);
          else
            *workp++ = *sampsp++;
        }
      }
    }
    else {
      const i_fsample_t *sampsp = samps;
      i_fsample_t *workp = work;

      for (i = x; i < r; ++i) {
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
          if (ch < color_chans)
            *workp++ = imcms_from_linearf(curves[ch], *sampsp++);
          else
            *workp++ = *sampsp++;
        }
      }
    }

    result = i_psampf(im, x, r, y, work, chans, chan_count);

    myfree(work);

    return result;
  }
  else {
    im_push_error(aIMCTX, 0, "Image position outside of image");
    return -1;
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

=head2 Fallback handler

=over

=item i_gsamp_bits_fb

Provides a fallback for the i_gsamp_bits() entry in the image virtual table.

=cut
*/

i_img_dim
i_gsamp_bits_fb(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, unsigned *samps, 
		const int *chans, int chan_count, int bits) {
  dIMCTXim(im);

  if (bits < 1 || bits > 32) {
    i_push_error(0, "Invalid bits, must be 1..32");
    return -1;
  }

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    double scale;
    i_img_dim i;
    i_fsample_t *fsamps = NULL;
    size_t total_samps;
    size_t samps_size;
    i_fsample_t *pfsamps;

    if (r > im->xsize)
      r = im->xsize;

    if (l >= r)
      return 0;

    total_samps = (size_t)(r - l) * (size_t)chan_count;
    samps_size = total_samps * sizeof(i_fsample_t);

    if (samps_size / chan_count / sizeof(i_fsample_t) != (r - l)) {
      i_push_error(0, "i_gsamp_bits: integer overflow calculating working buffer size");
      return -1;
    }
    
    if (bits == 32)
      scale = 4294967295.0;
    else
      scale = (double)(1 << bits) - 1;

    if (chans) {
      int ch;
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
          im_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return -1;
        }
      }
    }
    else {
      if (chan_count <= 0 || chan_count > im->channels) {
	i_push_error(0, "Invalid channel count");
	return -1;
      }
    }
    fsamps = mymalloc(samps_size);
    if (i_gsampf(im, l, r, y, fsamps, chans, chan_count) != total_samps) {
      i_push_error(0, "didn't get expected number of samples from i_gsampf()");
      myfree(fsamps);
      return -1;
    }
    for (i = 0, pfsamps = fsamps; i < total_samps; ++i, ++samps, ++pfsamps) {
      *samps = (unsigned)(*pfsamps * scale + 0.5);
    }
    myfree(fsamps);

    return total_samps;
  }
  else {
    i_push_error(0, "Image position outside of image");
    return -1;
  }
}

/*
=item i_psamp_bits_fb

Provides a fallback for the i_psamp_bits() entry in the image virtual table.

=cut
*/

i_img_dim
i_psamp_bits_fb(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const unsigned *samps,
                const int *chans, int chan_count, int bits) {
  dIMCTXim(im);

  if (bits < 1 || bits > 32) {
    i_push_error(0, "Invalid bits, must be 1..32");
    return -1;
  }

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    double scale;
    i_img_dim i;
    i_img_dim count;
    i_fsample_t *fsamps = NULL;
    size_t total_samps;
    size_t samps_size;
    i_fsample_t *pfsamps;

    if (r > im->xsize)
      r = im->xsize;

    if (l >= r)
      return 0;

    total_samps = (size_t)(r - l) * (size_t)chan_count;
    samps_size = total_samps * sizeof(i_fsample_t);

    if (samps_size / chan_count / sizeof(i_fsample_t) != (r - l)) {
      i_push_error(0, "i_psamp_bits: integer overflow calculating working buffer size");
      return -1;
    }

    if (bits == 32)
      scale = 4294967295.0;
    else
      scale = (double)(1 << bits) - 1;

    if (chans) {
      int ch;
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
          im_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return -1;
        }
      }
    }
    else {
      if (chan_count <= 0 || chan_count > im->channels) {
        i_push_error(0, "Invalid channel count");
        return -1;
      }
    }
    fsamps = mymalloc(samps_size);
    for (i = 0, pfsamps = fsamps; i < total_samps; ++i, ++samps, ++pfsamps) {
      *pfsamps = *samps / scale;
    }
    count = i_psampf(im, l, r, y, fsamps, chans, chan_count);
    myfree(fsamps);

    return count;
  }
  else {
    i_push_error(0, "Image position outside of image");
    return -1;
  }
}

/*
=item i_plin()
=category Drawing

  i_img *im = ...;
  i_img_dim l, r, y;
  i_color vals[...] = { ... colors ... };
  i_img_dim count = i_plin(im, l, r, y, vals);

Sets (r-l) pixels starting from (l,y) using (r-l) values from
I<colors>.

Returns the number of pixels set.

=cut
*/

i_img_dim
i_plin(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_color *vals) {
  i_img_dim count;
  if (r - l < 10) {
    count = 0;
    while (l < r) {
      if (i_ppix(im, l, y, vals) == 0) {
        ++count;
      }
      ++l;
      ++vals;
    }
    return count;
  }
  else if (im->channels != 4) {
    i_sample_t *samps = mymalloc((r-l) * im->channels);
    i_sample_t *psamps = samps;
    i_img_dim i = l;
    int ch;
    while (i < r) {
      for (ch = 0; ch < im->channels; ++ch) {
        *psamps++ = vals->channel[ch];
      }
      ++vals;
      ++i;
    }
    count = i_psamp(im, l, r, y, samps, NULL, im->channels);

    myfree(samps);
  }
  else {
    count = i_psamp(im, l, r, y, (const i_sample_t *)vals, NULL, im->channels);
  }

  return count > 0 ? (count / im->channels) : count;
}

/*
=item i_plinf()
=category Drawing

  i_img *im = ...;
  i_img_dim l, r, y;
  i_fcolor vals[...] = { ... colors ... };
  i_img_dim count = i_plinf(im, l, r, y, vals);

Sets (r-l) pixels starting from (l,y) using (r-l) values from
I<colors>.

Returns the number of pixels set.

=cut
*/

i_img_dim
i_plinf(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fcolor *vals) {
  i_img_dim count;
  if (r - l < 10) {
    count = 0;
    while (l < r) {
      if (i_ppixf(im, l, y, vals) == 0) {
        ++count;
      }
      ++l;
      ++vals;
    }
    return count;
  }
  else if (im->channels != 4) {
    i_fsample_t *samps = mymalloc((r-l) * im->channels * sizeof(i_fsample_t));
    i_fsample_t *psamps = samps;
    i_img_dim i = l;
    int ch;
    while (i < r) {
      for (ch = 0; ch < im->channels; ++ch) {
        *psamps++ = vals->channel[ch];
      }
      ++vals;
      ++i;
    }
    count = i_psampf(im, l, r, y, samps, NULL, im->channels);

    myfree(samps);
  }
  else {
    count = i_psampf(im, l, r, y, (const i_fsample_t *)vals, NULL, im->channels);
  }

  return count > 0 ? (count / im->channels) : count;
}

/*
=item i_glin()

=category Drawing

Retrieves (r-l) pixels starting from (l,y) into C<colors>.

  count = i_glin(im, l, r, y, colors);

Returns the number of pixels retrieved.

=cut
*/

i_img_dim
i_glin(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_color *vals) {
  i_img_dim count;
  if (r - l < 10) {
    count = 0;
    while (l < r) {
      if (i_gpix(im, l, y, vals) == 0) {
        ++count;
      }
      ++l;
      ++vals;
    }
    return count;
  }
  else if (im->channels != 4) {
    i_sample_t *samps = mymalloc((r-l) * im->channels * sizeof(i_sample_t));
    i_sample_t *psamps = samps;
    i_img_dim i = l;
    int ch;

    count = i_gsamp(im, l, r, y, samps, NULL, im->channels);

    while (i < r) {
      for (ch = 0; ch < im->channels; ++ch) {
        vals->channel[ch] = *psamps++;
      }
      ++vals;
      ++i;
    }

    myfree(samps);
  }
  else {
    count = i_gsamp(im, l, r, y, (i_sample_t *)vals, NULL, im->channels);
  }

  return count > 0 ? (count / im->channels) : count;
}

/*
=item i_glinf()

=category Drawing

Retrieves (r-l) pixels starting from (l,y) into C<fcolors>.

  count = i_glinf(im, l, r, y, fcolors);

Returns the number of pixels retrieved.

=cut
*/

i_img_dim
i_glinf(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fcolor *vals) {
  i_img_dim count;
  if (r - l < 10) {
    count = 0;
    while (l < r) {
      if (i_gpixf(im, l, y, vals) == 0) {
        ++count;
      }
      ++l;
      ++vals;
    }
    return count;
  }
  else if (im->channels != 4) {
    i_fsample_t *samps = mymalloc((r-l) * im->channels * sizeof(i_fsample_t));
    i_fsample_t *psamps = samps;
    i_img_dim i = l;
    int ch;

    count = i_gsampf(im, l, r, y, samps, NULL, im->channels);

    while (i < r) {
      for (ch = 0; ch < im->channels; ++ch) {
        vals->channel[ch] = *psamps++;
      }
      ++vals;
      ++i;
    }

    myfree(samps);
  }
  else {
    count = i_gsampf(im, l, r, y, (i_fsample_t *)vals, NULL, im->channels);
  }

  return count > 0 ? (count / im->channels) : count;
}

static int
test_magic(unsigned char *buffer, size_t length, struct file_magic_entry const *magic) {
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
im_test_format_probe(im_context_t ctx, io_glue *data, int length) {
  static const struct file_magic_entry formats[] = {
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

    /* WEBP
       http://code.google.com/speed/webp/docs/riff_container.html */
    FORMAT_ENTRY2("RIFF    WEBP", "webp", "xxxx    xxxx"),

    /* JPEG 2000 
       This might match a little loosely */
    FORMAT_ENTRY("\x00\x00\x00\x0CjP  \x0D\x0A\x87\x0A", "jp2"),

    /* FLIF - Free Lossless Image Format - https://flif.info/spec.html */
    FORMAT_ENTRY("FLIF", "flif"),

    /* JPEG XL */
    FORMAT_ENTRY("\xFF\x0A", "jxl"), /* simple */
    FORMAT_ENTRY("\x00\x00\x00\x0C\x4A\x58\x4C\x20\x0D\x0A\x87\x0A", "jxl"), /* complex */

    /* Quite OK Image Format */
    FORMAT_ENTRY("qoif", "qoi"),

    /* HEIF/HEIC see https://github.com/strukturag/libheif/issues/83 */
    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftypheic", "heif", "    xxxxxxxx"),
    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftypheix", "heif", "    xxxxxxxx"),
    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftyphevc", "heif", "    xxxxxxxx"),
    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftypheim", "heif", "    xxxxxxxx"),
    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftypheis", "heif", "    xxxxxxxx"),
    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftyphevm", "heif", "    xxxxxxxx"),
    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftyphevs", "heif", "    xxxxxxxx"),
    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftypmif1", "heif", "    xxxxxxxx"),
    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftypmsf1", "heif", "    xxxxxxxx"),

    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftypavif", "avif", "    xxxxxxxx"),
    /* AV1 image sequence */
    FORMAT_ENTRY2("\x00\x00\x00\x00" "ftypavis", "avif", "    xxxxxxxx")
  };
  static const struct file_magic_entry more_formats[] = {
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

  rc = i_io_peekn(data, head, 18);
  if (rc == -1) return NULL;
#if 0
  {
    int i;
    fprintf(stderr, "%d bytes -", (int)rc);
    for (i = 0; i < rc; ++i)
      fprintf(stderr, " %02x", head[i]);
    fprintf(stderr, "\n");
  }
#endif

  {
    im_file_magic *p = ctx->file_magic;
    while (p) {
      if (test_magic(head, rc, &p->m)) {
	return p->m.name;
      }
      p = p->next;
    }
  }

  for(i=0; i<sizeof(formats)/sizeof(formats[0]); i++) { 
    struct file_magic_entry const *entry = formats + i;

    if (test_magic(head, rc, entry)) 
      return entry->name;
  }

  if ((rc == 18) &&
      tga_header_verify(head))
    return "tga";

  for(i=0; i<sizeof(more_formats)/sizeof(more_formats[0]); i++) { 
    struct file_magic_entry const *entry = more_formats + i;

    if (test_magic(head, rc, entry)) 
      return entry->name;
  }

  return NULL;
}

/*
=item i_img_is_monochrome(img, &zero_is_white)

=category Image Information

Tests an image to check it meets our monochrome tests.

The idea is that a file writer can use this to test where it should
write the image in whatever bi-level format it uses, eg. C<pbm> for
C<pnm>.

For performance of encoders we require monochrome images:

=over

=item *

be paletted

=item *

have a palette of two colors, containing only C<(0,0,0)> and
C<(255,255,255)> in either order.

=back

C<zero_is_white> is set to non-zero if the first palette entry is white.

=cut
*/

int
i_img_is_monochrome(i_img *im, int *zero_is_white) {
  if (im->type == i_palette_type
      && i_colorcount(im) == 2) {
    i_color colors[2];
    if (!i_getcolors(im, 0, colors, 2))
      return 0;
    if (im->channels == 3) {
      if (colors[0].rgb.r == 255 && 
          colors[0].rgb.g == 255 &&
          colors[0].rgb.b == 255 &&
          colors[1].rgb.r == 0 &&
          colors[1].rgb.g == 0 &&
          colors[1].rgb.b == 0) {
        *zero_is_white = 1;
        return 1;
      }
      else if (colors[0].rgb.r == 0 && 
               colors[0].rgb.g == 0 &&
               colors[0].rgb.b == 0 &&
               colors[1].rgb.r == 255 &&
               colors[1].rgb.g == 255 &&
               colors[1].rgb.b == 255) {
        *zero_is_white = 0;
        return 1;
      }
    }
    else if (im->channels == 1) {
      if (colors[0].channel[0] == 255 &&
          colors[1].channel[0] == 0) {
        *zero_is_white = 1;
        return 1;
      }
      else if (colors[0].channel[0] == 0 &&
               colors[1].channel[0] == 255) {
        *zero_is_white = 0;
        return 1;         
      }
    }
  }

  *zero_is_white = 0;
  return 0;
}

/*
=item i_get_file_background(im, &bg)

=category Files

Retrieve the file write background color tag from the image.

If not present, C<bg> is set to black.

Returns 1 if the C<i_background> tag was found and valid.

=cut
*/

int
i_get_file_background(i_img *im, i_color *bg) {
  int result = i_tags_get_color(&im->tags, "i_background", 0, bg);
  if (!result) {
    /* black default */
    bg->channel[0] = bg->channel[1] = bg->channel[2] = 0;
  }
  /* always full alpha */
  bg->channel[3] = 255;

  return result;
}

/*
=item i_get_file_backgroundf(im, &bg)

=category Files

Retrieve the file write background color tag from the image as a
floating point color.

Implemented in terms of i_get_file_background().

If not present, C<bg> is set to black.

Returns 1 if the C<i_background> tag was found and valid.

=cut
*/

int
i_get_file_backgroundf(i_img *im, i_fcolor *fbg) {
  i_color bg;
  int result = i_get_file_background(im, &bg);
  fbg->rgba.r = Sample8ToF(bg.rgba.r);
  fbg->rgba.g = Sample8ToF(bg.rgba.g);
  fbg->rgba.b = Sample8ToF(bg.rgba.b);
  fbg->rgba.a = 1.0;

  return result;
}

/*
=item i_model_curves(model, &count)

Return a pointer to an array of curves for the given color model.

Returns the number of color channels in the image in count.

Returns NULL and a count of zero for the icm_unknown model.

=cut
*/

const imcms_curve_t *
im_model_curves(pIMCTX, i_color_model_t model, int *color_chan) {
  switch (model) {
  case icm_unknown:
    *color_chan = 0;
    return NULL;
  case icm_gray:
  case icm_gray_alpha:
    *color_chan = 1;
    return &aIMCTX->gray_curve;

  case icm_rgb:
  case icm_rgb_alpha:
    *color_chan = 3;
    return aIMCTX->rgb_curves;
  }
}


/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>

Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

L<Imager>, L<gif.c>

=cut
*/
