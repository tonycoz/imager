/*

  Common implementation of gamma sample images.
  
  To use this you need to define the following macros, then #include "imgamimg.h":

  Defines the image data types:

  IMG_SAMPLE_TYPE
    The sample type, eg. i_sample16_t, double, float

  IMG_BITS
    The i_img_bits_t enum identifier for the type.  Used to set bits
    in the image, and for the data callback.

  Defines the visible names of the image type creation functions:

  IMG_NEW_EXTRA_NAME

    The name of the extras image creation function.

  Used in defining the names of static functions and data structures,
  to make it easier to break on those for debugging.

  IMG_SUFFIX
    Suffix used for static callbacks, virtual table structure.

  Converstions back and forth between IMG_SAMPLE_TYPE and gamma
  color and non-color samples, and linear color and non-color samples;

  IMG_REP_TO_GAMMA_SAMPLE(x), IMG_REP_TO_GAMMA_SAMPLEF(x)

    Convert a gamma sample of IMG_SAMPLE_TYPE to a gamma
    i_sample_t or i_fsample_t, taking one parameter.

  IMG_REP_TO_LIN_SAMPLE(rep, channel), IMG_REP_TO_LIN_SAMPLEF(rep, channel)

    Convert a gamma sample of IMG_SAMPLE_TYPE to i_sample16_t or
    i_fsample_t, taking two parameters. This can use "curves" to
    convert from linear samples to gamma samples.

  IMG_REP_TO_LIN_SAMPLE_RAW(x), IMG_REP_TO_LIN_SAMPLEF_RAW(x)

    Convert a IMG_SAMPLE_TYPE value to an 16-bit or double linear sample.
    Used for non-color channels in the i_gslin() and i_gslinf()
    callbacks.

  IMG_GAMMA_TO_REP(x), IMG_GAMMAF_TO_REP(x)

    Convert a i_sample_t or i_fsample_t to a IMG_SAMPLE_TYPE using
    the curves for C<channel>.

  IMG_LIN_SAMPLE_TO_REP(x, channel), IMG_LIN_SAMPLEF_TO_REP(x, channel)

    Convert a i_sample16_t or i_fsample_t a LINIMG_SAMPLE_TYPE
    value. This can use "curves" to convert from linear samples to
    gamma samples.

  IMG_LIN_SAMPLE_RAW_TO_REP(x), IMG_LIN_SAMPLEF_RAW_TO_REP(x)

    Convert a i_sample16_t or i_fsample_t a IMG_SAMPLE_TYPE value.
    This is used for non-color channels and should not try to use
    curves.

*/

#define IMAGER_NO_CONTEXT
#include "imager.h"
#include "imageri.h"
#include "imapiver.h"

#define ISUFFIX(x) IM_CAT(x, IMG_SUFFIX)

static i_img_dim
ISUFFIX(i_gsamp_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, 
                 int const *chans, int chan_count);
static i_img_dim
ISUFFIX(i_gsampf_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, 
                  int const *chans, int chan_count);
static i_img_dim 
ISUFFIX(i_psamp_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample_t *samps,
                  const int *chans, int chan_count);
static i_img_dim 
ISUFFIX(i_psampf_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps,
                   const int *chans, int chan_count);
static i_image_data_alloc_t *
ISUFFIX(i_data_)(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
                 void **pdata, size_t *psize, int *pextra);
static i_img_dim
ISUFFIX(i_gslin_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample16_t *samps,
                  const int *chans, int chan_count);
static i_img_dim
ISUFFIX(i_gslinf_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps,
                              const int *chans, int chan_count);
static i_img_dim
ISUFFIX(i_pslin_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample16_t *samps,
                             const int *chans, int chan_count);
static i_img_dim
ISUFFIX(i_pslinf_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps, const int *chans, int chan_count);


/*
=item vtable_*

Virtual table.

Internal.

=cut
*/
static const i_img_vtable
ISUFFIX(vtable_) = {
  IMAGER_API_LEVEL,

  ISUFFIX(i_gsamp_), /* i_f_gsamp */
  ISUFFIX(i_gsampf_), /* i_f_gsampf */

  ISUFFIX(i_psamp_), /* i_f_psamp */
  ISUFFIX(i_psampf_), /* i_f_psampf */

  NULL, /* i_f_destroy */

#ifdef IMG_GSAMP_BITS
  IMG_GSAMP_BITS,
#else
  i_gsamp_bits_fb,
#endif
#ifdef IMG_PSAMP_BITS
  IMG_PSAMP_BITS,
#else
  i_psamp_bits_fb, /* i_f_psamp_bits */
#endif

  ISUFFIX(i_gslin_),
  ISUFFIX(i_gslinf_),
  ISUFFIX(i_pslin_),
  ISUFFIX(i_pslinf_),

  ISUFFIX(i_data_),

  NULL, /* i_f_imageop */

  NULL, /* i_f_gpal */
  NULL, /* i_f_ppal */
  NULL, /* i_f_addcolors */
  NULL, /* i_f_getcolors */
  NULL, /* i_f_colorcount */
  NULL, /* i_f_maxcolors */
  NULL, /* i_f_findcolor */
};

i_img *
IMG_NEW_EXTRA_NAME(pIMCTX, i_img_dim x, i_img_dim y, int ch, int extra) {
  size_t bytes;
  size_t line_bytes;
  i_img *im;
  int totalch;

  im_log((aIMCTX, 1, IM_QUOTE(IMG_NEW_EXTRA_NAME)"(x %" i_DF ", y %" i_DF ", ch %d, extra %d)\n",
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
  bytes = (i_img_dim_u)x * (i_img_dim_u)y * (i_img_dim_u)totalch * sizeof(IMG_SAMPLE_TYPE);
  if (bytes / (i_img_dim_u)y / (i_img_dim_u)totalch / sizeof(IMG_SAMPLE_TYPE) != x) {
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
  
  im = im_img_new(aIMCTX, &ISUFFIX(vtable_), x, y, ch, extra, IMG_BITS);
  if (im == NULL)
    return NULL;

  im->bytes = bytes;
  im->idata = myzmalloc(im->bytes);
  im_img_init(aIMCTX, im);
  
  return im;
}

static i_img_dim
ISUFFIX(i_gsamp_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, 
                  int const *chans, int chan_count) {
  i_img_dim count, i, w;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_img_dim off;
    int totalch = im->channels + im->extrachannels;
    const IMG_SAMPLE_TYPE *data = (const IMG_SAMPLE_TYPE *)im->idata;
    
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      if (!i_img_valid_channel_indexes(im, chans, chan_count)) {
        return 0;
      }
      for (i = 0; i < w; ++i) {
        int chi;
        for (chi = 0; chi < chan_count; ++chi) {
          int ch = chans[chi];
          *samps++ = IMG_REP_TO_GAMMA_SAMPLE(data[off+ch]);
          ++count;
        }
        off += totalch;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	dIMCTXim(im);
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= total channels", 
		      chan_count);
	return 0;
      }
      for (i = 0; i < w; ++i) {
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = IMG_REP_TO_GAMMA_SAMPLE(data[off+ch]);
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
ISUFFIX(i_gsampf_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, 
                   int const *chans, int chan_count) {
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_img_dim count, i, w;
    i_img_dim off;
    int totalch = im->channels + im->extrachannels;
    const IMG_SAMPLE_TYPE *data = (const IMG_SAMPLE_TYPE *)im->idata;

    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      if (!i_img_valid_channel_indexes(im, chans, chan_count)) {
        return 0;
      }
      for (i = 0; i < w; ++i) {
        int chi;
        for (chi = 0; chi < chan_count; ++chi) {
          int ch = chans[chi];
          *samps++ = IMG_REP_TO_GAMMA_SAMPLEF(data[off+ch]);
          ++count;
        }
        off += totalch;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	dIMCTXim(im);
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= total channels", 
		      chan_count);
	return 0;
      }
      for (i = 0; i < w; ++i) {
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = IMG_REP_TO_GAMMA_SAMPLEF(data[off+ch]);
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
=item i_psamp_flt(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample_t *samps, int *chans, int chan_count)

Writes sample values to im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples written (which should be (r-l) *
bits_set(chan_mask)

=cut
*/

static i_img_dim
ISUFFIX(i_psamp_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
                  const i_sample_t *samps, const int *chans, int chan_count) {
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_img_dim count, i, w;
    i_img_dim offset;
    int totalch = im->channels + im->extrachannels;
    IMG_SAMPLE_TYPE *data = (IMG_SAMPLE_TYPE *)im->idata;

    if (r > im->xsize)
      r = im->xsize;
    offset = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      if (!i_img_valid_channel_indexes(im, chans, chan_count)) {
        return -1;
      }
      if (i_img_all_channel_mask(im)) {
	for (i = 0; i < w; ++i) {
          int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[chi];
	    data[offset + ch] = IMG_GAMMA_TO_REP(*samps);
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
          int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[chi];
	    if (im->ch_mask & (1 << ch))
	      data[offset + ch] = IMG_GAMMA_TO_REP(*samps);
	    
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	dIMCTXim(im);
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= total channels", 
		      chan_count);
	return -1;
      }
      for (i = 0; i < w; ++i) {
        int ch;
	unsigned mask = 1;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask)
	    data[offset + ch] = IMG_GAMMA_TO_REP(*samps);

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
=item i_psampf_dflt(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps, int *chans, int chan_count)

Writes sample values to im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples written (which should be (r-l) *
bits_set(chan_mask)

=cut
*/

static
i_img_dim
ISUFFIX(i_psampf_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
                   const i_fsample_t *samps, const int *chans, int chan_count) {
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_img_dim count, i, w;
    i_img_dim offset;
    int totalch = im->channels + im->extrachannels;
    IMG_SAMPLE_TYPE *data = (IMG_SAMPLE_TYPE *)im->idata;

    if (r > im->xsize)
      r = im->xsize;
    offset = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      if (!i_img_valid_channel_indexes(im, chans, chan_count)) {
        return -1;
      }
      if (i_img_all_channel_mask(im)) {
	for (i = 0; i < w; ++i) {
          int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[chi];
	    data[offset + ch] = IMG_GAMMAF_TO_REP(*samps);
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
          int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
            int ch = chans[chi];
	    if (im->ch_mask & (1 << ch))
	      data[offset + ch] = IMG_GAMMAF_TO_REP(*samps);
	    
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	dIMCTXim(im);
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= total channels", 
		      chan_count);
	return -1;
      }
      for (i = 0; i < w; ++i) {
	unsigned mask = 1;
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask)
	    data[offset + ch] = IMG_GAMMAF_TO_REP(*samps);

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

static i_image_data_alloc_t *
ISUFFIX(i_data_)(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
                      void **pdata, size_t *psize, int *pextra) {
  if ((im->extrachannels && !(flags & idf_extras))
      || (flags & (idf_otherendian|idf_linear_curve)) != 0
      || bits != IMG_BITS
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
ISUFFIX(i_gslin_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
                  i_sample16_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);
  i_img_dim count, i, w;
  i_img_dim off;
  int color_chans;
  const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);
  const IMG_SAMPLE_TYPE *data = (const IMG_SAMPLE_TYPE *)im->idata;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    int totalch = i_img_totalchannels(im);
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      if (!i_img_valid_channel_indexes(im, chans, chan_count)) {
        return -1;
      }
      for (i = 0; i < w; ++i) {
        int chi;
        for (chi = 0; chi < chan_count; ++chi) {
	  int ch = chans[chi];
	  if (ch < color_chans)
	    *samps++ = IMG_REP_TO_LIN_SAMPLE(data[off+ch], ch);
	  else
	    *samps++ = IMG_REP_TO_LIN_SAMPLE_RAW(data[off+ch]);
          ++count;
        }
        off += totalch;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return 0;
      }
      for (i = 0; i < w; ++i) {
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (ch < color_chans)
	    *samps++ = IMG_REP_TO_LIN_SAMPLE(data[off+ch], ch);
	  else
	    *samps++ = IMG_REP_TO_LIN_SAMPLE_RAW(data[off+ch]);
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
ISUFFIX(i_gslinf_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
                   i_fsample_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);
  i_img_dim count, i, w;
  int color_chans;
  const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);
  const IMG_SAMPLE_TYPE *data = (const IMG_SAMPLE_TYPE *)im->idata;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    int totalch = i_img_totalchannels(im);
    i_img_dim off;
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      if (!i_img_valid_channel_indexes(im, chans, chan_count)) {
        return -1;
      }
      for (i = 0; i < w; ++i) {
        int chi;
        for (chi = 0; chi < chan_count; ++chi) {
	  int ch = chans[chi];
	  if (ch < color_chans)
	    *samps++ = IMG_REP_TO_LIN_SAMPLEF(data[off+ch], ch);
	  else
	    *samps++ = IMG_REP_TO_LIN_SAMPLEF_RAW(data[off+ch]);
          ++count;
        }
        off += totalch;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return 0;
      }
      for (i = 0; i < w; ++i) {
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (ch < color_chans)
	    *samps++ = IMG_REP_TO_LIN_SAMPLEF(data[off+ch], ch);
	  else
	    *samps++ = IMG_REP_TO_LIN_SAMPLEF_RAW(data[off+ch]);;
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
ISUFFIX(i_pslin_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
                  const i_sample16_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);
  i_img_dim count, i, w;
  int color_chans;
  const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);
  IMG_SAMPLE_TYPE *data = (IMG_SAMPLE_TYPE *)im->idata;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    int totalch = i_img_totalchannels(im);
    i_img_dim off;
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      if (!i_img_valid_channel_indexes(im, chans, chan_count)) {
        return -1;
      }
      if (i_img_all_channel_mask(im)) {
	for (i = 0; i < w; ++i) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    int ch = chans[chi];
	    if (ch < color_chans)
	      data[off+ch] = IMG_LIN_SAMPLE_TO_REP(*samps, ch);
	    else
	      data[off+ch] = IMG_LIN_SAMPLE_RAW_TO_REP(*samps);
            ++samps;
	    ++count;
	  }
	  off += totalch;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    int ch = chans[chi];
	    if (im->ch_mask & (1 << ch)) {
	      if (ch < color_chans)
                data[off+ch] = IMG_LIN_SAMPLE_TO_REP(*samps, ch);
              else
                data[off+ch] = IMG_LIN_SAMPLE_RAW_TO_REP(*samps);
	    }
	    ++samps;
	    ++count;
	  }
	  off += totalch;
	}
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return -1;
      }
      for (i = 0; i < w; ++i) {
	unsigned mask = 1;
	int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask) {
	    if (ch < color_chans)
              data[off+ch] = IMG_LIN_SAMPLE_TO_REP(*samps, ch);
            else
              data[off+ch] = IMG_LIN_SAMPLE_RAW_TO_REP(*samps);
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
    i_push_error(0, "Image position outside of image");
    return -1;
  }
}

static
i_img_dim
ISUFFIX(i_pslinf_)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
                   const i_fsample_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);
  i_img_dim count, i, w;
  int color_chans;
  const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);
  IMG_SAMPLE_TYPE *data = (IMG_SAMPLE_TYPE *)im->idata;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    int totalch = i_img_totalchannels(im);
    i_img_dim off;
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      if (!i_img_valid_channel_indexes(im, chans, chan_count)) {
        return -1;
      }
      if (i_img_all_channel_mask(im)) {
	for (i = 0; i < w; ++i) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    int ch = chans[chi];
	    if (ch < color_chans)
	      data[off+ch] = IMG_LIN_SAMPLEF_TO_REP(*samps, ch);
	    else
	      data[off+ch] = IMG_LIN_SAMPLEF_RAW_TO_REP(*samps);
            ++samps;
	    ++count;
	  }
	  off += totalch;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    int ch = chans[chi];
	    if (im->ch_mask & (1 << ch)) {
	      if (ch < color_chans)
                data[off+ch] = IMG_LIN_SAMPLEF_TO_REP(*samps, ch);
              else
                data[off+ch] = IMG_LIN_SAMPLEF_RAW_TO_REP(*samps);
	    }
	    ++samps;
	    ++count;
	  }
	  off += totalch;
	}
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return -1;
      }
      for (i = 0; i < w; ++i) {
	unsigned mask = 1;
	int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask) {
	    if (ch < color_chans)
              data[off+ch] = IMG_LIN_SAMPLEF_TO_REP(*samps, ch);
            else
              data[off+ch] = IMG_LIN_SAMPLEF_RAW_TO_REP(*samps);
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
    i_push_error(0, "Image position outside of image");
    return -1;
  }
}

