/*
=head1 NAME

maskimg.c - implements masked images/image subsets

=head1 SYNOPSIS

=head1 DESCRIPTION

=over
=cut
*/

#include "imager.h"
#include "imageri.h"

#include <stdio.h>
/*
=item i_img_mask_ext

A pointer to this type of object is kept in the ext_data of a masked 
image.

=cut
*/

typedef struct {
  i_img *targ;
  i_img *mask;
  int xbase, ybase;
  i_sample_t *samps; /* temp space */
} i_img_mask_ext;

#define MASKEXT(im) ((i_img_mask_ext *)((im)->ext_data))

static void i_destroy_masked(i_img *im);
static int i_ppix_masked(i_img *im, int x, int y, const i_color *pix);
static int i_ppixf_masked(i_img *im, int x, int y, const i_fcolor *pix);
static int i_plin_masked(i_img *im, int l, int r, int y, const i_color *vals);
static int i_plinf_masked(i_img *im, int l, int r, int y, const i_fcolor *vals);
static int i_gpix_masked(i_img *im, int x, int y, i_color *pix);
static int i_gpixf_masked(i_img *im, int x, int y, i_fcolor *pix);
static int i_glin_masked(i_img *im, int l, int r, int y, i_color *vals);
static int i_glinf_masked(i_img *im, int l, int r, int y, i_fcolor *vals);
static int i_gsamp_masked(i_img *im, int l, int r, int y, i_sample_t *samp, 
                          int const *chans, int chan_count);
static int i_gsampf_masked(i_img *im, int l, int r, int y, i_fsample_t *samp, 
                           int const *chans, int chan_count);
static int i_gpal_masked(i_img *im, int l, int r, int y, i_palidx *vals);
static int i_ppal_masked(i_img *im, int l, int r, int y, const i_palidx *vals);

/*
=item IIM_base_masked

The basic data we copy into a masked image.

=cut
*/
static i_img IIM_base_masked =
{
  0, /* channels set */
  0, 0, 0, /* xsize, ysize, bytes */
  ~0U, /* ch_mask */
  i_8_bits, /* bits */
  i_palette_type, /* type */
  1, /* virtual */
  NULL, /* idata */
  { 0, 0, NULL }, /* tags */
  NULL, /* ext_data */

  i_ppix_masked, /* i_f_ppix */
  i_ppixf_masked, /* i_f_ppixf */
  i_plin_masked, /* i_f_plin */
  i_plinf_masked, /* i_f_plinf */
  i_gpix_masked, /* i_f_gpix */
  i_gpixf_masked, /* i_f_gpixf */
  i_glin_masked, /* i_f_glin */
  i_glinf_masked, /* i_f_glinf */
  i_gsamp_masked, /* i_f_gsamp */
  i_gsampf_masked, /* i_f_gsampf */

  i_gpal_masked, /* i_f_gpal */
  i_ppal_masked, /* i_f_ppal */
  i_addcolors_forward, /* i_f_addcolors */
  i_getcolors_forward, /* i_f_getcolors */
  i_colorcount_forward, /* i_f_colorcount */
  i_maxcolors_forward, /* i_f_maxcolors */
  i_findcolor_forward, /* i_f_findcolor */
  i_setcolors_forward, /* i_f_setcolors */

  i_destroy_masked, /* i_f_destroy */
};

/*
=item i_img_masked_new(i_img *targ, i_img *mask, int xbase, int ybase, int w, int h)

Create a new masked image.

The image mask is optional, in which case the image is just a view of
a rectangular portion of the image.

The mask only has an effect of writing to the image, the entire view
of the underlying image is readable.

pixel access to mimg(x,y) is translated to targ(x+xbase, y+ybase), as long 
as (0 <= x < w) and (0 <= y < h).

For a pixel to be writable, the pixel mask(x,y) must have non-zero in
it's first channel.  No scaling of the pixel is done, the channel 
sample is treated as boolean.

=cut
*/

i_img *i_img_masked_new(i_img *targ, i_img *mask, int x, int y, int w, int h) {
  i_img *im;
  i_img_mask_ext *ext;

  i_clear_error();
  if (x >= targ->xsize || y >= targ->ysize) {
    i_push_error(0, "subset outside of target image");
    return NULL;
  }
  if (mask) {
    if (w > mask->xsize)
      w = mask->xsize;
    if (h > mask->ysize)
      h = mask->ysize;
  }
  if (x+w > targ->xsize)
    w = targ->xsize - x;
  if (y+h > targ->ysize)
    h = targ->ysize - y;

  im = mymalloc(sizeof(i_img));
  memcpy(im, &IIM_base_masked, sizeof(i_img));
  im->xsize = w;
  im->ysize = h;
  im->channels = targ->channels;
  im->bits = targ->bits;
  im->type = targ->type;
  ext = mymalloc(sizeof(*ext));
  ext->targ = targ;
  ext->mask = mask;
  ext->xbase = x;
  ext->ybase = y;
  ext->samps = mymalloc(sizeof(i_sample_t) * im->xsize);
  im->ext_data = ext;

  return im;
}

/*
=item i_destroy_masked(i_img *im)

The destruction handler for masked images.

Releases the ext_data.

Internal function.

=cut
*/

static void i_destroy_masked(i_img *im) {
  myfree(MASKEXT(im)->samps);
  myfree(im->ext_data);
}

/*
=item i_ppix_masked(i_img *im, int x, int y, const i_color *pix)

Write a pixel to a masked image.

Internal function.

=cut
*/
static int i_ppix_masked(i_img *im, int x, int y, const i_color *pix) {
  i_img_mask_ext *ext = MASKEXT(im);
  int result;

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize)
    return -1;
  if (ext->mask) {
    i_sample_t samp;
    
    if (i_gsamp(ext->mask, x, x+1, y, &samp, NULL, 1) && !samp)
      return 0; /* pretend it was good */
  }
  result = i_ppix(ext->targ, x + ext->xbase, y + ext->ybase, pix);
  im->type = ext->targ->type;
  return result;
}

/*
=item i_ppixf_masked(i_img *im, int x, int y, const i_fcolor *pix)

Write a pixel to a masked image.

Internal function.

=cut
*/
static int i_ppixf_masked(i_img *im, int x, int y, const i_fcolor *pix) {
  i_img_mask_ext *ext = MASKEXT(im);
  int result;

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize)
    return -1;
  if (ext->mask) {
    i_sample_t samp;
    
    if (i_gsamp(ext->mask, x, x+1, y, &samp, NULL, 1) && !samp)
      return 0; /* pretend it was good */
  }
  result = i_ppixf(ext->targ, x + ext->xbase, y + ext->ybase, pix);
  im->type = ext->targ->type;
  return result;
}

/*
=item i_plin_masked(i_img *im, int l, int r, int y, const i_color *vals)

Write a row of data to a masked image.

Internal function.

=cut
*/
static int i_plin_masked(i_img *im, int l, int r, int y, const i_color *vals) {
  i_img_mask_ext *ext = MASKEXT(im);

  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    if (ext->mask) {
      int i;
      int simple = 0;
      i_sample_t *samps = ext->samps;
      int w = r - l;

      i_gsamp(ext->mask, l, r, y, samps, NULL, 1);
      if (w < 10)
        simple = 0;
      else {
        /* the idea is to make a fast scan to see how often the state
           changes */
        int changes = 0;
        for (i = 0; i < w-1; ++i)
          if (!samps[i] != !samps[i+1])
            ++changes;
        if (changes > w/3) /* just rough */
          simple = 1;
      }
      if (simple) {
        /* we'd be calling a usually more complicated i_plin function
           almost as often as the usually simple i_ppix(), so just
           do a simple scan
        */
        for (i = 0; i < w; ++i) {
          if (samps[i])
            i_ppix(ext->targ, l + i + ext->xbase, y + ext->ybase, vals + i);
        }
        im->type = ext->targ->type;
        return r-l;
      }
      else {
        /* the scan above indicates there should be some contiguous 
           regions, look for them and render
        */
        int start;
        i = 0;
        while (i < w) {
          while (i < w && !samps[i])
            ++i;
          start = i;
          while (i < w && samps[i])
            ++i;
          if (i != start)
            i_plin(ext->targ, l + start + ext->xbase, l + i + ext->xbase, 
                   y + ext->ybase, vals + start);
        }
        im->type = ext->targ->type;
        return w;
      }
    }
    else {
      int result = i_plin(ext->targ, l + ext->xbase, r + ext->xbase, 
                          y + ext->ybase, vals);
      im->type = ext->targ->type;
      return result;
    }
  }
  else {
    return 0;
  }
}

/*
=item i_plinf_masked(i_img *im, int l, int r, int y, const i_fcolor *vals)

Write a row of data to a masked image.

Internal function.

=cut
*/
static int i_plinf_masked(i_img *im, int l, int r, int y, const i_fcolor *vals) {
  i_img_mask_ext *ext = MASKEXT(im);
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    if (ext->mask) {
      int i;
      int simple = 0;
      i_sample_t *samps = ext->samps;
      int w = r - l;

      i_gsamp(ext->mask, l, r, y, samps, NULL, 1);
      if (w < 10)
        simple = 0;
      else {
        /* the idea is to make a fast scan to see how often the state
           changes */
        int changes = 0;
        for (i = 0; i < w-1; ++i)
          if (!samps[i] != !samps[i+1])
            ++changes;
        if (changes > w/3) /* just rough */
          simple = 1;
      }
      if (simple) {
        /* we'd be calling a usually more complicated i_plin function
           almost as often as the usually simple i_ppix(), so just
           do a simple scan
        */
        for (i = 0; i < w; ++i) {
          if (samps[i])
            i_ppixf(ext->targ, l + i + ext->xbase, y + ext->ybase, vals+i);
        }
        im->type = ext->targ->type;
        return r-l;
      }
      else {
        /* the scan above indicates there should be some contiguous 
           regions, look for them and render
        */
        int start;
        i = 0;
        while (i < w) {
          while (i < w && !samps[i])
            ++i;
          start = i;
          while (i < w && samps[i])
            ++i;
          if (i != start)
            i_plinf(ext->targ, l + start + ext->xbase, l + i + ext->xbase, 
                    y + ext->ybase, vals + start);
        }
        im->type = ext->targ->type;
        return w;
      }
    }
    else {
      int result = i_plinf(ext->targ, l + ext->xbase, r + ext->xbase, 
                           y + ext->ybase, vals);
      im->type = ext->targ->type;
      return result;
    }
  }
  else {
    return 0;
  }
}

/*
=item i_gpix_masked(i_img *im, int x, int y, i_color *pix)

Read a pixel from a masked image.

Internal.

=cut
*/
static int i_gpix_masked(i_img *im, int x, int y, i_color *pix) {
  i_img_mask_ext *ext = MASKEXT(im);

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize)
    return -1;

  return i_gpix(ext->targ, x + ext->xbase, y + ext->ybase, pix);
}

/*
=item i_gpixf_masked(i_img *im, int x, int y, i_fcolor *pix)

Read a pixel from a masked image.

Internal.

=cut
*/
static int i_gpixf_masked(i_img *im, int x, int y, i_fcolor *pix) {
  i_img_mask_ext *ext = MASKEXT(im);

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize)
    return -1;

  return i_gpixf(ext->targ, x + ext->xbase, y + ext->ybase, pix);
}

static int i_glin_masked(i_img *im, int l, int r, int y, i_color *vals) {
  i_img_mask_ext *ext = MASKEXT(im);
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    return i_glin(ext->targ, l + ext->xbase, r + ext->xbase, 
                  y + ext->ybase, vals);
  }
  else {
    return 0;
  }
}

static int i_glinf_masked(i_img *im, int l, int r, int y, i_fcolor *vals) {
  i_img_mask_ext *ext = MASKEXT(im);
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    return i_glinf(ext->targ, l + ext->xbase, r + ext->xbase, 
                  y + ext->ybase, vals);
  }
  else {
    return 0;
  }
}

static int i_gsamp_masked(i_img *im, int l, int r, int y, i_sample_t *samp, 
                          int const *chans, int chan_count) {
  i_img_mask_ext *ext = MASKEXT(im);
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    return i_gsamp(ext->targ, l + ext->xbase, r + ext->xbase, 
                  y + ext->ybase, samp, chans, chan_count);
  }
  else {
    return 0;
  }
}

static int i_gsampf_masked(i_img *im, int l, int r, int y, i_fsample_t *samp, 
                          int const *chans, int chan_count) {
  i_img_mask_ext *ext = MASKEXT(im);
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    return i_gsampf(ext->targ, l + ext->xbase, r + ext->xbase, 
                    y + ext->ybase, samp, chans, chan_count);
  }
  else {
    return 0;
  }
}

static int i_gpal_masked(i_img *im, int l, int r, int y, i_palidx *vals) {
  i_img_mask_ext *ext = MASKEXT(im);
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    return i_gpal(ext->targ, l + ext->xbase, r + ext->xbase, 
                  y + ext->ybase, vals);
  }
  else {
    return 0;
  }
}

static int i_ppal_masked(i_img *im, int l, int r, int y, const i_palidx *vals) {
  i_img_mask_ext *ext = MASKEXT(im);
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    if (ext->mask) {
      int i;
      i_sample_t *samps = ext->samps;
      int w = r - l;
      int start;
      
      i = 0;
      while (i < w) {
        while (i < w && !samps[i])
          ++i;
        start = i;
        while (i < w && samps[i])
          ++i;
        if (i != start)
          i_ppal(ext->targ, l+start+ext->xbase, l+i+ext->xbase, 
                 y+ext->ybase, vals+start);
      }
      return w;
    }
    else {
      return i_ppal(ext->targ, l + ext->xbase, r + ext->xbase, 
                    y + ext->ybase, vals);
    }
  }
  else {
    return 0;
  }
}


/*
=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
*/
