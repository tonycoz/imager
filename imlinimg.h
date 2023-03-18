/*

  Common implementation of linear sample images.
  
  To use this you need to define the following macros, then #include "imlinimg.h":

  Defines the image data types:

  LINIMG_SAMPLE_TYPE
    The sample type, eg. i_sample16_t, double, float

  LINIMG_BITS
    The i_img_bits_t enum identifier for the type.  Used to set bits
    in the image, and for the data callback.

  Defines the visible names of the image type creation functions:

  LINIMG_NEW_NAME

    The name of the base image creation function.

  LINIMG_NEW_EXTRA_NAME

    The name of the extras image creation function.

  Used in defining the names of static functions and data structures,
  to make it easier to break on those for debugging.

  LINIMG_SUFFIX
    Suffix used for static callbacks, virtual table structure.


  Conversions back and forth between LINIMG_SAMPLE_TYPE and gamma
  color and non-color samples, and linear color and non-color samples;

  LINIMG_REP_TO_LIN_SAMPLE(rep), LINIMG_REP_TO_LIN_SAMPLEF(rep)

    Convert a linear sample of LINIMG_SAMPLE_TYPE to i_sample16_t or
    i_fsample_t, taking one parameter.

  LINIMG_REP_TO_GAMMA_SAMPLE(x, channel), LINIMG_REP_TO_GAMMA_SAMPLE(x, channel)

    Convert a linear sample of LINIMG_SAMPLE_TYPE to a gamma
    i_sample_t or i_fsample_t, taking one parameter.  This can use
    "curves" to convert from linear samples to gamma samples.

  LINIMG_REP_TO_GAMMA_RAW(x, channel), LINIMG_REP_TO_GAMMAF_RAW(x, channel)

    Convert a LIMIMG_SAMPLE_TYPE value to an 8-bit or double sample.
    Used for non-color channels in the i_gsamp() and i_gsampf()
    callbacks.

  LINIMG_LIN_SAMPLE_TO_REP(x), LINIMG_LIN_SAMPLEF_TO_REP(x)

    Convert a i_sample16_t or i_fsample_t a LINIMG_SAMPLE_TYPE value.

  LINIMG_GAMMA_TO_REP(x, channel), LINIMG_GAMMAF_TO_REP(x, channel)

    Convert a i_sample_t or i_fsample_t to a LINIMG_SAMPLE_TYPE using
    the curves for C<channel>.

  LINIMG_GAMMA_RAW_TO_REP(x), LINIMG_GAMMAF_RAW_TO_REP(x)

    Convert a i_sample_t or i_fsample_t to a LINIMG_SAMPLE_TYPE
    without using curves.  Used for non-color channels in the
    i_psamp() and i_psampf() implementations.
*/

#define IMAGER_NO_CONTEXT

#include "imager.h"
#include "imageri.h"
#include "imapiver.h"
#ifdef IMAGER_USE_INTTYPES_H
#include <inttypes.h>
#endif

static i_img_dim
IM_CAT(i_gsamp_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, 
                             int const *chans, int chan_count);
static i_img_dim
IM_CAT(i_gsampf_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, 
                              int const *chans, int chan_count);
static i_img_dim
IM_CAT(i_psamp_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample_t *samps,
                              const int *chans, int chan_count);
static i_img_dim
IM_CAT(i_psampf_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps,
                              const int *chans, int chan_count);
static i_image_data_alloc_t *
IM_CAT(i_data_, LINIMG_SUFFIX)(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
                          void **p, size_t *size, int *extra);

static i_img_dim
IM_CAT(i_gslin_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample16_t *samps,
                             const int *chans, int chan_count);
static i_img_dim
IM_CAT(i_gslinf_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps,
                              const int *chans, int chan_count);
static i_img_dim
IM_CAT(i_pslin_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample16_t *samps,
                             const int *chans, int chan_count);
static i_img_dim
IM_CAT(i_pslinf_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps,
                              const int *chans, int chan_count);


/*
=item vtable_lin16bit

Vtable for linear 16-bit direct images.

Internal.

=cut
*/

static const i_img_vtable
IM_CAT(vtable_, LINIMG_SUFFIX) = {
  IMAGER_API_LEVEL,
  
  IM_CAT(i_gsamp_, LINIMG_SUFFIX),
  IM_CAT(i_gsampf_, LINIMG_SUFFIX),

  IM_CAT(i_psamp_, LINIMG_SUFFIX),
  IM_CAT(i_psampf_, LINIMG_SUFFIX),

  NULL, /* i_f_destroy */

  i_gsamp_bits_fb,
  i_psamp_bits_fb,

  IM_CAT(i_gslin_, LINIMG_SUFFIX),
  IM_CAT(i_gslinf_, LINIMG_SUFFIX),
  IM_CAT(i_pslin_, LINIMG_SUFFIX),
  IM_CAT(i_pslinf_, LINIMG_SUFFIX),
  
  IM_CAT(i_data_, LINIMG_SUFFIX),

  NULL, /* i_f_imageop */

  NULL, /* i_f_gpal */
  NULL, /* i_f_ppal */
  NULL, /* i_f_addcolors */
  NULL, /* i_f_getcolors */
  NULL, /* i_f_colorcount */
  NULL, /* i_f_maxcolors */
  NULL, /* i_f_findcolor */
  NULL /* i_f_setcolors */
};

/*
=item im_lin_img_16_new_extra(ctx, x, y, ch)
X<im_lin_img_16_new_extra API>X<i_img_16_new_extra API>
=category Image creation/destruction
=synopsis i_img *img = im_lin_img_16_new_extra(aIMCTX, width, height, channels);
=synopsis i_img *img = i_lin_img_16_new_extra(width, height, channels);

Create a new linear 16-bit/sample image.

Returns the image on success, or NULL on failure.

Also callable as C<i_lin_img_16_new_extra(x, y, ch)>

=cut
*/

i_img *
LINIMG_NEW_EXTRA_NAME(pIMCTX, i_img_dim x, i_img_dim y, int ch, int extra) {
  i_img *im;
  size_t bytes, line_bytes;
  int totalch;

  im_log((aIMCTX, 1, IM_QUOTE(LINIMG_NEW_EXTRA_NAME) "(x %" i_DF ", y %" i_DF ", ch %d)\n",
	  i_DFc(x), i_DFc(y), ch, extra));

  im_clear_error(aIMCTX);

  if (x < 1 || y < 1) {
    im_push_error(aIMCTX, 0, "Image sizes must be positive");
    return NULL;
  }
  if (ch < 0 || ch > MAXCHANNELS) {
    im_push_errorf(aIMCTX, 0, "channels must be between 0 and %d", MAXCHANNELS);
    return NULL;
  }
  if (ch == 0 && extra == 0) {
    im_push_error(aIMCTX, 0, "there must be extra channels if channels is zero");
    return NULL;
  }
  if (extra < 0 || extra > MAXEXTRACHANNELS) {
    im_push_errorf(aIMCTX, 0, "extrachannels must be between 0 and %d", MAXEXTRACHANNELS);
    return NULL;
  }
  if (ch + extra > MAXTOTALCHANNELS) {
    im_push_errorf(aIMCTX, 0, "channels + extra channels must be no more than %d", MAXTOTALCHANNELS);
    return NULL;
  }
  totalch = ch + extra;
  bytes =  (i_img_dim_u)x * (i_img_dim_u)y * (i_img_dim_u)totalch * sizeof(LINIMG_SAMPLE_TYPE);
  if (bytes / (i_img_dim_u)y / (i_img_dim_u)totalch / sizeof(LINIMG_SAMPLE_TYPE) != (i_img_dim_u)x) {
    im_push_errorf(aIMCTX, 0, "integer overflow calculating image allocation");
    return NULL;
  }
  
  /* basic assumption: we can always allocate a buffer representing a
     line from the image, otherwise we're going to have trouble
     working with the image */
  line_bytes = sizeof(i_fcolor) * (i_img_dim_u)x;
  if (line_bytes / x != sizeof(i_fcolor)) {
    im_push_error(aIMCTX, 0, "integer overflow calculating scanline allocation");
    return NULL;
  }

  im = im_img_new(aIMCTX, &IM_CAT(vtable_, LINIMG_SUFFIX), x, y, ch, extra, LINIMG_BITS);
  if (im == NULL)
    return NULL;

  im->bytes = bytes;
  im->idata = myzmalloc(im->bytes);
  im->islinear = 1;

  im_img_init(aIMCTX, im);

  return im;
}

static i_img_dim
IM_CAT(i_gsamp_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, 
                             int const *chans, int chan_count) {
  dIMCTXim(im); 

  i_assert_valid_channels(im, chans, chan_count);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    const LINIMG_SAMPLE_TYPE * const data = (const LINIMG_SAMPLE_TYPE *)im->idata;
    const int totalch = i_img_totalchannels(im);
    int color_chans;
    const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);
    i_img_dim x;
    i_img_dim count = 0;
    i_img_dim off = (l + y * im->xsize) * totalch;

    if (r > im->xsize)
      r = im->xsize;

    if (chans) {
      for (x = l; x < r; ++x) {
        int chi;
        for (chi = 0; chi < chan_count; ++chi) {
          int ch = chans[chi];
          if (ch < color_chans) {
            *samps++ = LINIMG_REP_TO_GAMMA_SAMPLE(data[off+ch], ch);
          }
          else {
            *samps++ = LINIMG_REP_TO_GAMMA_RAW(data[off+ch]);
          }
               
          ++count;
        }
        off += totalch;
      }
    }
    else {
      for (x = l; x < r; ++x) {
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
          if (ch < color_chans) {
            *samps++ = LINIMG_REP_TO_GAMMA_SAMPLE(data[off+ch], ch);
          }
          else {
            *samps++ = LINIMG_REP_TO_GAMMA_RAW(data[off+ch]);
          }
          ++count;
        }
        off += totalch;
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

static i_img_dim
IM_CAT(i_gsampf_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, 
             int const *chans, int chan_count) {
  dIMCTXim(im);

  i_assert_valid_channels(im, chans, chan_count);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    const LINIMG_SAMPLE_TYPE *const data = (const LINIMG_SAMPLE_TYPE *)im->idata;
    const int totalch = i_img_totalchannels(im);
    int color_chans;
    const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);
    i_img_dim x;
    i_img_dim count = 0;
    i_img_dim off = (l + y * im->xsize) * totalch;;

    if (r > im->xsize)
      r = im->xsize;

    if (chans) {
      for (x = l; x < r; ++x) {
        int chi;
        for (chi = 0; chi < chan_count; ++chi) {
          int ch = chans[chi];
          if (ch < color_chans)
            *samps++ = LINIMG_REP_TO_GAMMA_SAMPLEF(data[off+ch], ch);
          else
            *samps++ = LINIMG_REP_TO_GAMMAF_RAW(data[off+ch]);
          ++count;
        }
        off += totalch;
      }
    }
    else {
      for (x = l; x < r; ++x) {
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
          if (ch < color_chans)
            *samps++ = LINIMG_REP_TO_GAMMA_SAMPLEF(data[off+ch], ch);
          else
            *samps++ = LINIMG_REP_TO_GAMMAF_RAW(data[off+ch]);
          ++count;
        }
        off += totalch;
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_psamp_l16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample_t *samps, int *chans, int chan_count)

Writes sample values to im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples written (which should be (r-l) *
bits_set(chan_mask)

=cut
*/

static i_img_dim
IM_CAT(i_psamp_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
                             const i_sample_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);

  i_assert_valid_channels(im, chans, chan_count);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    const int totalch = i_img_totalchannels(im);
    LINIMG_SAMPLE_TYPE * const data = (LINIMG_SAMPLE_TYPE *)im->idata;    
    i_img_dim offset = (l + y * im->xsize) * totalch;
    i_img_dim count = 0;
    i_img_dim x;
    int color_chans;
    const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);

    if (r > im->xsize)
      r = im->xsize;

    if (chans) {
      if (i_img_all_channel_mask(im)) {
	for (x = l; x < r; ++x) {
          int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[chi];
            if (ch < color_chans) 
              data[offset + ch] = LINIMG_GAMMA_TO_REP(*samps, ch);
            else
              data[offset + ch] = LINIMG_GAMMA_RAW_TO_REP(*samps);
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
      else {
	for (x = l; x < r; ++x) {
          int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[chi];
	    if (im->ch_mask & (1 << ch)) {
              if (ch < color_chans)
                data[offset + ch] = LINIMG_GAMMA_TO_REP(*samps, ch);
              else
                data[offset + ch] = LINIMG_GAMMA_RAW_TO_REP(*samps);
            }
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
    }
    else {
      for (x = l; x < r; ++x) {
	unsigned mask = 1;
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask) {
            if (ch < color_chans)
              data[offset + ch] = LINIMG_GAMMA_TO_REP(*samps, ch);
            else
              data[offset + ch] = LINIMG_GAMMA_RAW_TO_REP(*samps);
          }
	  ++samps;
          ++count;
	  mask <<= 1;
        }
        offset += totalch;
      }
    }

    return count;
  }
  else {
    dIMCTXim(im);
    i_push_error(0, "Image position outside of image");
    return -1;
  }
}

/*
=item i_psampf_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps, int *chans, int chan_count)

Writes sample values to im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples written (which should be (r-l) *
bits_set(chan_mask)

=cut
*/

static i_img_dim
IM_CAT(i_psampf_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
                              const i_fsample_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);

  i_assert_valid_channels(im, chans, chan_count);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    int color_chans;
    const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);
    const int totalch = i_img_totalchannels(im);
    LINIMG_SAMPLE_TYPE *const data = (LINIMG_SAMPLE_TYPE *)im->idata;    
    i_img_dim count = 0;
    i_img_dim x;
    i_img_dim offset = (l + y * im->xsize) * totalch;

    if (r > im->xsize)
      r = im->xsize;

    if (chans) {
      if (i_img_all_channel_mask(im)) {
	for (x = l; x < r; ++x) {
          int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[chi];
            if (ch < color_chans)
              data[offset + ch] = LINIMG_GAMMAF_TO_REP(*samps, ch);
            else
              data[offset + ch] = LINIMG_GAMMAF_RAW_TO_REP(*samps);
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
      else {
	for (x = l; x < r; ++x) {
          int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[chi];
	    if (im->ch_mask & (1 << ch)) {
              if (ch < color_chans)
                data[offset + ch] = LINIMG_GAMMAF_TO_REP(*samps, ch);
              else
                data[offset + ch] = LINIMG_GAMMAF_RAW_TO_REP(*samps);
	    }
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
    }
    else {
      for (x = l; x < r; ++x) {
	unsigned mask = 1;
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask) {
            if (ch < color_chans)
              data[offset + ch] = LINIMG_GAMMAF_TO_REP(*samps, ch);
            else
              data[offset + ch] = LINIMG_GAMMAF_RAW_TO_REP(*samps);
	  }
	  ++samps;
          ++count;
	  mask <<= 1;
        }
        offset += totalch;
      }
    }

    return count;
  }
  else {
    im_push_error(aIMCTX, 0, "Image position outside of image");
    return -1;
  }
}

static i_image_data_alloc_t *
IM_CAT(i_data_, LINIMG_SUFFIX)(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
                            void **pdata, size_t *psize, int *pextra) {
  if ((im->extrachannels && !(flags & idf_extras))
      || (flags & ~(idf_extras|idf_synthesize|idf_writable)) != idf_linear_curve
      || bits != LINIMG_BITS
      || layout == idl_palette
      || layout != im->channels) {
    return i_img_data_fallback(im, layout, bits, flags, pdata, psize, pextra);
  }

  *pdata = im->idata;
  *psize = im->bytes;
  *pextra = im->extrachannels;

  return i_new_image_data_alloc_def(im);
}

static i_img_dim
IM_CAT(i_gslin_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
                             i_sample16_t *samps, const int *chans, int chan_count) {
  i_assert_valid_channels(im, chans, chan_count);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    const LINIMG_SAMPLE_TYPE * const data = (const LINIMG_SAMPLE_TYPE *)im->idata;
    const int totalch = i_img_totalchannels(im);
    i_img_dim x;
    i_img_dim count = 0;
    i_img_dim off = (l+y*im->xsize) * totalch;
    
    if (r > im->xsize)
      r = im->xsize;

    if (chans) {
      if (chan_count == 1) {
        int ch = chans[0];
        for (x = l; x < r; ++x) {
          *samps++ = LINIMG_REP_TO_LIN_SAMPLE(data[off+ch]);
          ++count;
          off += totalch;
        }
      }
      else {
        for (x = l; x < r; ++x) {
          int chi;
          for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[chi];
            *samps++ = LINIMG_REP_TO_LIN_SAMPLE(data[off+ch]);
            ++count;
          }
          off += totalch;
        }
      }
    }
    else {
      if (chan_count == 1) {
        for (x = l; x < r; ++x) {
          *samps++ = LINIMG_REP_TO_LIN_SAMPLE(data[off]); /* always channel 0 */
          ++count;
          off += totalch;
        }
      }
      else {
        for (x = l; x < r; ++x) {
          int ch;
          for (ch = 0; ch < chan_count; ++ch) {
            *samps++ = LINIMG_REP_TO_LIN_SAMPLE(data[off+ch]);
            ++count;
          }
          off += totalch;
        }
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

static i_img_dim
IM_CAT(i_gslinf_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
                              i_fsample_t *samps, const int *chans, int chan_count) {
  i_assert_valid_channels(im, chans, chan_count);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    const LINIMG_SAMPLE_TYPE * const data = (const LINIMG_SAMPLE_TYPE *)im->idata;
    int const totalch = i_img_totalchannels(im);
    i_img_dim off = (l+y*im->xsize) * totalch;;
    i_img_dim x;
    i_img_dim count = 0;

    if (r > im->xsize)
      r = im->xsize;

    if (chans) {
      if (chan_count == 1) {
        int ch = chans[0];
        for (x = l; x < r; ++x) {
          *samps++ = LINIMG_REP_TO_LIN_SAMPLEF(data[off+ch]);
          ++count;
          off += totalch;
        }
      }
      else {
        for (x = l; x < r; ++x) {
          int chi;
          for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[chi];
            *samps++ = LINIMG_REP_TO_LIN_SAMPLEF(data[off+ch]);
            ++count;
          }
          off += totalch;
        }
      }
    }
    else {
      if (chan_count == 1) {
        for (x = l; x < r; ++x) {
          *samps++ = LINIMG_REP_TO_LIN_SAMPLEF(data[off]); /* always channel 0 */
          ++count;
          off += totalch;
        }
      }
      else {
        for (x = l; x < r; ++x) {
          int ch;
          for (ch = 0; ch < chan_count; ++ch) {
            *samps++ = LINIMG_REP_TO_LIN_SAMPLEF(data[off+ch]);
            ++count;
          }
          off += totalch;
        }
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

static i_img_dim
IM_CAT(i_pslin_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
                             const i_sample16_t *samps, const int *chans, int chan_count) {
  i_assert_valid_channels(im, chans, chan_count);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    LINIMG_SAMPLE_TYPE * const data = (LINIMG_SAMPLE_TYPE *)im->idata;    
    const int totalch = i_img_totalchannels(im);
    i_img_dim off = (l + y * im->xsize) * totalch;
    i_img_dim count = 0;
    i_img_dim x;
    
    if (r > im->xsize)
      r = im->xsize;

    if (chans) {
      if (i_img_all_channel_mask(im)) {
	for (x = l; x < r; ++x) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    int ch = chans[chi];
	    data[off + ch] = LINIMG_LIN_SAMPLE_TO_REP(*samps);
            ++samps;
	    ++count;
	  }
	  off += totalch;
	}
      }
      else {
	for (x = l; x < r; ++x) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    int ch = chans[chi];
	    if (im->ch_mask & (1 << ch)) {
              data[off + ch] = LINIMG_LIN_SAMPLE_TO_REP(*samps);
	    }
	    ++samps;
	    ++count;
	  }
	  off += totalch;
	}
      }
    }
    else {
      for (x = l; x < r; ++x) {
	unsigned mask = 1;
	int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask) {
            data[off + ch] = LINIMG_LIN_SAMPLE_TO_REP(*samps);
	  }
	  ++samps;
          ++count;
	  mask <<= 1;
        }
        off += totalch;
      }
    }

    return count;
  }
  else {
    dIMCTXim(im);
    i_push_error(0, "Image position outside of image");
    return -1;
  }
}

static i_img_dim
IM_CAT(i_pslinf_, LINIMG_SUFFIX)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
                              const i_fsample_t *samps, const int *chans, int chan_count) {
  i_assert_valid_channels(im, chans, chan_count);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    LINIMG_SAMPLE_TYPE * const data = (LINIMG_SAMPLE_TYPE *)im->idata;    
    const int totalch = i_img_totalchannels(im);
    i_img_dim off = (l + y * im->xsize) * totalch;
    i_img_dim count = 0;
    i_img_dim x;

    if (r > im->xsize)
      r = im->xsize;

    if (chans) {
      if (i_img_all_channel_mask(im)) {
	for (x = l; x < r; ++x) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    int ch = chans[chi];
	    data[off + ch] = LINIMG_LIN_SAMPLEF_TO_REP(*samps);
            ++samps;
	    ++count;
	  }
	  off += totalch;
	}
      }
      else {
	for (x = l; x < r; ++x) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    int ch = chans[chi];
	    if (im->ch_mask & (1 << ch)) {
              data[off + ch] = LINIMG_LIN_SAMPLEF_TO_REP(*samps);
	    }
	    ++samps;
	    ++count;
	  }
	  off += totalch;
	}
      }
    }
    else {
      for (x = l; x < r; ++x) {
	unsigned mask = 1;
	int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask) {
            data[off + ch] = LINIMG_LIN_SAMPLEF_TO_REP(*samps);
	  }
	  ++samps;
          ++count;
	  mask <<= 1;
        }
        off += totalch;
      }
    }

    return count;
  }
  else {
    dIMCTXim(im);
    i_push_error(0, "Image position outside of image");
    return -1;
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
