/*
=head1 NAME

maskimg.c - implements masked images/image subsets

=head1 SYNOPSIS

=head1 DESCRIPTION

=over
=cut
*/

#define IMAGER_NO_CONTEXT

#include "imager.h"
#include "imageri.h"
#include "imapiver.h"

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
  i_img_dim xbase, ybase;
  i_sample_t *samps; /* temp space */
} i_img_mask_ext;

#define MASKEXT(im) ((i_img_mask_ext *)((im)->ext_data))

static void i_destroy_masked(i_img *im);
static i_img_dim i_gsamp_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samp, 
                          int const *chans, int chan_count);
static i_img_dim i_gsampf_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samp, 
                           int const *chans, int chan_count);
static i_img_dim i_gpal_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_palidx *vals);
static i_img_dim i_ppal_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_palidx *vals);
static i_img_dim
psamp_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
	       const i_sample_t *samples, const int *chans, int chan_count);
static i_img_dim
psampf_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
	       const i_fsample_t *samples, const int *chans, int chan_count);
static i_image_data_alloc_t *
data_masked(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
	    void **pdata, size_t *psize, int *pextra);
/*
=item vtable_masked

Virtual table for masked images.

=cut
*/

static const i_img_vtable
vtable_mask = {
  IMAGER_API_LEVEL,
  
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

  NULL, /* i_f_gsamp_bits */
  NULL, /* i_f_psamp_bits */

  psamp_masked, /* i_f_psamp */
  psampf_masked, /* i_f_psampf */

  data_masked
};

/*
=item i_img_masked_new(i_img *targ, i_img *mask, i_img_dim xbase, i_img_dim ybase, i_img_dim w, i_img_dim h)

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

i_img *
i_img_masked_new(i_img *targ, i_img *mask, i_img_dim x, i_img_dim y, i_img_dim w, i_img_dim h) {
  i_img *im;
  i_img_mask_ext *ext;
  dIMCTXim(targ);

  im_clear_error(aIMCTX);
  if (x < 0 || x >= targ->xsize || y < 0 || y >= targ->ysize) {
    im_push_error(aIMCTX, 0, "subset outside of target image");
    return NULL;
  }
  if (mask) {
    if (mask->channels == 0) {
      im_push_error(aIMCTX, 0, "mask has no color channels");
      return NULL;
    }
    if (w > mask->xsize)
      w = mask->xsize;
    if (h > mask->ysize)
      h = mask->ysize;
  }
  if (x+w > targ->xsize)
    w = targ->xsize - x;
  if (y+h > targ->ysize)
    h = targ->ysize - y;

  if (w < 1 || h < 1) {
    im_push_error(aIMCTX, 0, "width and height must be greater than or equal to 1");
    return NULL;
  }

  im = im_img_new(aIMCTX, &vtable_mask, w, h, targ->channels,
                  targ->extrachannels, targ->bits);
  if (im == NULL) {
    return NULL;
  }

  im->type = targ->type;
  im->isvirtual = 1;

  ext = mymalloc(sizeof(*ext));
  ext->targ = targ;
  ext->mask = mask;
  ext->xbase = x;
  ext->ybase = y;
  ext->samps = mymalloc(sizeof(i_sample_t) * im->xsize);
  im->ext_data = ext;

  im_img_init(aIMCTX, im);

  i_img_refcnt_inc(targ);
  if (mask)
    i_img_refcnt_inc(mask);

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
  i_img_mask_ext *ext = MASKEXT(im);
  i_img_destroy(ext->targ);
  if (ext->mask)
    i_img_destroy(ext->mask);
  myfree(ext->samps);
  myfree(im->ext_data);
}

static i_img_dim i_gsamp_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samp, 
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

static i_img_dim i_gsampf_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samp, 
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

static i_img_dim i_gpal_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_palidx *vals) {
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

static i_img_dim i_ppal_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_palidx *vals) {
  i_img_mask_ext *ext = MASKEXT(im);
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    if (ext->mask) {
      i_img_dim i;
      i_sample_t *samps = ext->samps;
      i_img_dim w = r - l;
      i_img_dim start;
      
      i_gsamp(ext->mask, l, r, y, samps, NULL, 1);
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
=item psamp_masked()

i_psamp() implementation for masked images.

=cut
*/

static i_img_dim
psamp_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
	     const i_sample_t *samples, const int *chans, int chan_count) {
  i_img_mask_ext *ext = MASKEXT(im);

  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    unsigned old_ch_mask = ext->targ->ch_mask;
    i_img_dim result = 0;
    ext->targ->ch_mask = im->ch_mask;
    if (r > im->xsize)
      r = im->xsize;
    if (ext->mask) {
      i_img_dim w = r - l;
      i_img_dim i = 0;
      i_img_dim x = ext->xbase + l;
      i_img_dim work_y = y + ext->ybase;
      i_sample_t *mask_samps = ext->samps;
	
      i_gsamp(ext->mask, l, r, y, mask_samps, NULL, 1);
      /* not optimizing this yet */
      while (i < w) {
	if (mask_samps[i]) {
	  /* found a set mask value, try to do a run */
	  i_img_dim run_left = x;
	  const i_sample_t *run_samps = samples;
	  ++i;
	  ++x;
	  samples += chan_count;
	  
	  while (i < w && mask_samps[i]) {
	    ++i;
	    ++x;
	    samples += chan_count;
	  }
	  result += i_psamp(ext->targ, run_left, x, work_y, run_samps, chans, chan_count);
	}
	else {
	  ++i;
	  ++x;
	  samples += chan_count;
	  result += chan_count; /* pretend we wrote masked off pixels */
	}
      }
    }
    else {
      result = i_psamp(ext->targ, l + ext->xbase, r + ext->xbase, 
		       y + ext->ybase, samples, chans, chan_count);
    }
    im->type = ext->targ->type;
    ext->targ->ch_mask = old_ch_mask;
    return result;
  }
  else {
    dIMCTXim(im);
    i_push_error(0, "Image position outside of image");
    return -1;
  }
}

/*
=item psampf_masked()

i_psampf() implementation for masked images.

=cut
*/

static i_img_dim
psampf_masked(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
	     const i_fsample_t *samples, const int *chans, int chan_count) {
  i_img_mask_ext *ext = MASKEXT(im);

  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_img_dim result = 0;
    unsigned old_ch_mask = ext->targ->ch_mask;
    ext->targ->ch_mask = im->ch_mask;
    if (r > im->xsize)
      r = im->xsize;
    if (ext->mask) {
      i_img_dim w = r - l;
      i_img_dim i = 0;
      i_img_dim x = ext->xbase + l;
      i_img_dim work_y = y + ext->ybase;
      i_sample_t *mask_samps = ext->samps;
	
      i_gsamp(ext->mask, l, r, y, mask_samps, NULL, 1);
      /* not optimizing this yet */
      while (i < w) {
	if (mask_samps[i]) {
	  /* found a set mask value, try to do a run */
	  i_img_dim run_left = x;
	  const i_fsample_t *run_samps = samples;
	  ++i;
	  ++x;
	  samples += chan_count;
	  
	  while (i < w && mask_samps[i]) {
	    ++i;
	    ++x;
	    samples += chan_count;
	  }
	  result += i_psampf(ext->targ, run_left, x, work_y, run_samps, chans, chan_count);
	}
	else {
	  ++i;
	  ++x;
	  samples += chan_count;
	  result += chan_count; /* pretend we wrote masked off pixels */
	}
      }
    }
    else {
      result = i_psampf(ext->targ, l + ext->xbase, r + ext->xbase, 
			y + ext->ybase, samples,
				 chans, chan_count);
    }
    im->type = ext->targ->type;
    ext->targ->ch_mask = old_ch_mask;
    return result;
  }
  else {
    dIMCTXim(im);
    i_push_error(0, "Image position outside of image");
    return -1;
  }
}

static i_image_data_alloc_t *
data_masked(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
	    void **pdata, size_t *psize, int *pextra) {
  i_img_mask_ext *ext = MASKEXT(im);

  im->type = ext->targ->type;

  return i_img_data_fallback(im, layout, bits, flags, pdata, psize, pextra);
}

/*
=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
*/
