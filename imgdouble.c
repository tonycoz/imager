/*
=head1 NAME

imgdouble.c - implements double per sample images

=head1 SYNOPSIS

  i_img *im = i_img_double_new(width, height, channels);
  # use like a normal image

=head1 DESCRIPTION

Implements double/sample images.

This basic implementation is required so that we have some larger 
sample image type to work with.

=over

=cut
*/

#define IMAGER_NO_CONTEXT
#include "imager.h"
#include "imageri.h"
#include "imapiver.h"

static i_img_dim i_gsamp_ddoub(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, 
                       int const *chans, int chan_count);
static i_img_dim i_gsampf_ddoub(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, 
                        int const *chans, int chan_count);
static i_img_dim 
i_psamp_ddoub(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample_t *samps, const int *chans, int chan_count);
static i_img_dim 
i_psampf_ddoub(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps, const int *chans, int chan_count);
static i_image_data_alloc_t *
i_data_double(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
              void **pdata, size_t *psize, int *pextra);
static i_img_dim i_gslindbl_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample16_t *samps, const int *chans, int chan_count);
static i_img_dim i_gslindblf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, const int *chans, int chan_count);
static i_img_dim i_pslindbl_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample16_t *samps, const int *chans, int chan_count);
static i_img_dim i_pslindblf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps, const int *chans, int chan_count);


/*
=item vtable_double

Virtual table for double images.

Internal.

=cut
*/
static const i_img_vtable
vtable_double = {
  IMAGER_API_LEVEL,

  i_gsamp_ddoub, /* i_f_gsamp */
  i_gsampf_ddoub, /* i_f_gsampf */

  i_psamp_ddoub, /* i_f_psamp */
  i_psampf_ddoub, /* i_f_psampf */

  NULL, /* i_f_destroy */

  i_gsamp_bits_fb,
  i_psamp_bits_fb, /* i_f_psamp_bits */

  i_gslindbl_d,
  i_gslindblf_d,
  i_pslindbl_d,
  i_pslindblf_d,

  i_data_double,

  NULL, /* i_f_imageop */

  NULL, /* i_f_gpal */
  NULL, /* i_f_ppal */
  NULL, /* i_f_addcolors */
  NULL, /* i_f_getcolors */
  NULL, /* i_f_colorcount */
  NULL, /* i_f_maxcolors */
  NULL, /* i_f_findcolor */
};

/*
=item im_img_double_new(ctx, x, y, ch)
X<im_img_double_new API>X<i_img_double_new API>
=category Image creation/destruction
=synopsis i_img *img = im_img_double_new(aIMCTX, width, height, channels);
=synopsis i_img *img = i_img_double_new(width, height, channels);

Creates a new double per sample image.

Also callable as C<i_img_double_new(width, height, channels)>.

=cut
*/

i_img *
im_img_double_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_img_double_new_extra(aIMCTX, x, y, ch, 0);
}

/*
=item im_img_double_new_extra(ctx, x, y, ch, extra)
X<im_img_double_new_extra API>X<i_img_double_new_extra API>
=category Image creation/destruction
=synopsis i_img *img = im_img_double_new_extra(aIMCTX, width, height, channels, extra);
=synopsis i_img *img = i_img_double_new_extra(width, height, channels, extra);

Creates a new double per sample image, possibly with
L<Imager::ImageTypes/Extra channels>.

Also callable as C<i_img_double_new_extra(width, height, channels, extra)>.

=cut
*/

i_img *
im_img_double_new_extra(pIMCTX, i_img_dim x, i_img_dim y, int ch, int extra) {
  size_t bytes;
  i_img *im;
  int totalch;

  im_log((aIMCTX, 1,"i_img_double_new(x %" i_DF ", y %" i_DF ", ch %d, extra %d)\n",
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
  bytes = (i_img_dim_u)x * (i_img_dim_u)y * (i_img_dim_u)totalch * sizeof(double);
  if (bytes / y / totalch / sizeof(double) != x) {
    im_push_errorf(aIMCTX, 0, "integer overflow calculating image allocation");
    return NULL;
  }
  
  im = im_img_new(aIMCTX, &vtable_double, x, y, ch, extra, i_double_bits);
  if (im == NULL)
    return NULL;

  im->bytes = bytes;
  im->idata = myzmalloc(im->bytes);
  im_img_init(aIMCTX, im);
  
  return im;
}

static i_img_dim
i_gsamp_ddoub(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, 
                       int const *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_img_dim off;
    int totalch = im->channels + im->extrachannels;
    
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= totalch) {
	  dIMCTXim(im);
          im_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return 0;
        }
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = SampleFTo8(((double *)im->idata)[off+chans[ch]]);
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
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = SampleFTo8(((double *)im->idata)[off+ch]);
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
i_gsampf_ddoub(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, 
               int const *chans, int chan_count) {
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    int ch;
    i_img_dim count, i, w;
    i_img_dim off;
    int totalch = im->channels + im->extrachannels;

    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= totalch) {
	  dIMCTXim(im);
          im_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return 0;
        }
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = ((double *)im->idata)[off+chans[ch]];
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
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = ((double *)im->idata)[off+ch];
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
=item i_psamp_ddoub(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample_t *samps, int *chans, int chan_count)

Writes sample values to im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples written (which should be (r-l) *
bits_set(chan_mask)

=cut
*/

static
i_img_dim
i_psamp_ddoub(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
	  const i_sample_t *samps, const int *chans, int chan_count) {

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    int ch;
    i_img_dim count, i, w;
    i_img_dim offset;
    int totalch = im->channels + im->extrachannels;

    if (r > im->xsize)
      r = im->xsize;
    offset = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      /* and test if all channels specified are in the mask */
      int all_in_mask = 1;
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= totalch) {
	  dIMCTXim(im);
          im_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return -1;
        }
	if (!((1 << chans[ch]) & im->ch_mask))
	  all_in_mask = 0;
      }
      if (all_in_mask) {
	for (i = 0; i < w; ++i) {
	  for (ch = 0; ch < chan_count; ++ch) {
	    ((double*)im->idata)[offset + chans[ch]] = Sample8ToF(*samps);
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
	  for (ch = 0; ch < chan_count; ++ch) {
	    if (im->ch_mask & (1 << (chans[ch])))
	      ((double*)im->idata)[offset + chans[ch]] = Sample8ToF(*samps);
	    
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
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask)
	    ((double*)im->idata)[offset + ch] = Sample8ToF(*samps);

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
=item i_psampf_ddoub(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps, int *chans, int chan_count)

Writes sample values to im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples written (which should be (r-l) *
bits_set(chan_mask)

=cut
*/

static
i_img_dim
i_psampf_ddoub(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
	       const i_fsample_t *samps, const int *chans, int chan_count) {

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    int ch;
    i_img_dim count, i, w;
    i_img_dim offset;
    int totalch = im->channels + im->extrachannels;

    if (r > im->xsize)
      r = im->xsize;
    offset = (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      /* and test if all channels specified are in the mask */
      int all_in_mask = 1;
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= totalch) {
	  dIMCTXim(im);
          im_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return -1;
        }
	if (!((1 << chans[ch]) & im->ch_mask))
	  all_in_mask = 0;
      }
      if (all_in_mask) {
	for (i = 0; i < w; ++i) {
	  for (ch = 0; ch < chan_count; ++ch) {
	    ((double*)im->idata)[offset + chans[ch]] = *samps;
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
	  for (ch = 0; ch < chan_count; ++ch) {
	    if (im->ch_mask & (1 << (chans[ch])))
	      ((double*)im->idata)[offset + chans[ch]] = *samps;
	    
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
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask)
	    ((double*)im->idata)[offset + ch] = *samps;

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
i_data_double(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
          void **pdata, size_t *psize, int *pextra) {
  if ((im->extrachannels && !(flags & idf_extras))
      || (flags & idf_otherendian)
      || bits != i_double_bits
      || layout == idl_palette
      || layout != im->channels) {
    return i_img_data_fallback(im, layout, bits, flags, pdata, psize, pextra);
  }

  *pdata = im->idata;
  *psize = im->bytes;
  *pextra = im->extrachannels;

  return i_new_image_data_alloc_def(im);
}

/*
=item i_img_to_drgb(im)

=category Image creation

Returns a double/sample version of the supplied image.

Returns the image on success, or NULL on failure.

=cut
*/

i_img *
i_img_to_drgb(i_img *im) {
  i_img *targ;
  i_fcolor *line;
  i_img_dim y;
  dIMCTXim(im);

  targ = im_img_double_new(aIMCTX, im->xsize, im->ysize, im->channels);
  if (!targ)
    return NULL;
  line = mymalloc(sizeof(i_fcolor) * im->xsize);
  for (y = 0; y < im->ysize; ++y) {
    i_glinf(im, 0, im->xsize, y, line);
    i_plinf(targ, 0, im->xsize, y, line);
  }

  myfree(line);

  return targ;
}

static i_img_dim
i_gslindbl_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
	  i_sample16_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);
  int ch;
  int chi;
  i_img_dim count, i, w;
  i_img_dim off;
  int color_chans;
  const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
          im_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return 0;
        }
      }
      for (i = 0; i < w; ++i) {
        for (chi = 0; chi < chan_count; ++chi) {
	  int ch = chans[chi];
	  i_fsample_t raw = ((double *)im->idata)[off+chans[ch]];
	  if (ch < color_chans)
	    *samps++ = SampleFTo16(imcms_to_linearf(curves[ch], raw));
	  else
	    *samps++ = SampleFTo16(raw);
          ++count;
        }
        off += im->channels;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > im->channels) {
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return 0;
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
	  i_fsample_t raw = ((double *)im->idata)[off+ch];
	  if (ch < color_chans)
	    *samps++ = SampleFTo16(imcms_to_linearf(curves[ch], raw));
	  else
	    *samps++ = SampleFTo16(raw);
          ++count;
        }
        off += im->channels;
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

static i_img_dim
i_gslindblf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
	  i_fsample_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);
  int ch;
  int chi;
  i_img_dim count, i, w;
  int color_chans;
  const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_img_dim off;
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
          im_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return 0;
        }
      }
      for (i = 0; i < w; ++i) {
        for (chi = 0; chi < chan_count; ++chi) {
	  int ch = chans[chi];
	  i_fsample_t raw = ((double *)im->idata)[off+ch];
	  if (ch < color_chans)
	    *samps++ = imcms_to_linearf(curves[ch], raw);
	  else
	    *samps++ = raw;
          ++count;
        }
        off += im->channels;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > im->channels) {
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return 0;
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
	  i_fsample_t raw = ((double *)im->idata)[off+ch];
	  if (ch < color_chans)
	    *samps++ = imcms_to_linearf(curves[ch], raw);
	  else
	    *samps++ = raw;
          ++count;
        }
        off += im->channels;
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

static
i_img_dim
i_pslindbl_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
	  const i_sample16_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);
  i_img_dim count, i, w;
  int color_chans;
  const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_img_dim off;
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      /* and test if all channels specified are in the mask */
      int all_in_mask = 1;
      int ch;
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
          im_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return -1;
        }
	if (!((1 << chans[ch]) & im->ch_mask))
	  all_in_mask = 0;
      }
      if (all_in_mask) {
	for (i = 0; i < w; ++i) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    i_sample16_t samp = *samps++;
	    int ch = chans[chi];
	    i_fsample_t out;
	    if (ch < color_chans)
	      out = imcms_from_linearf(curves[ch], Sample16ToF(samp));
	    else
	      out = Sample16ToF(samp);
	    ((double *)im->idata)[off+ch] = out;
	    ++count;
	  }
	  off += im->channels;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    int ch = chans[chi];
	    i_fsample_t out;
	    if (im->ch_mask & (1 << ch)) {
	      if (ch < color_chans)
		out = imcms_from_linearf(curves[ch], Sample16ToF(*samps));
	      else
		out = Sample16ToF(*samps);
	      ((double *)im->idata)[off+ch] = out;
	    }
	    ++samps;
	    ++count;
	  }
	  off += im->channels;
	}
      }
    }
    else {
      if (chan_count <= 0 || chan_count > im->channels) {
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return -1;
      }
      for (i = 0; i < w; ++i) {
	unsigned mask = 1;
	int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask) {
	    i_fsample_t out;
	    if (ch < color_chans)
	      out = imcms_from_linearf(curves[ch], Sample16ToF(*samps));
	    else
	      out = Sample16ToF(*samps);
	    ((double *)im->idata)[off+ch] = out;
	  }
	  ++samps;
          ++count;
	  mask <<= 1;
        }
        off += im->channels;
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
i_pslindblf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
	  const i_fsample_t *samps, const int *chans, int chan_count) {
  dIMCTXim(im);
  i_img_dim count, i, w;
  int color_chans;
  const imcms_curve_t *curves = i_model_curves(i_img_color_model(im), &color_chans);

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_img_dim off;
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      /* and test if all channels specified are in the mask */
      int all_in_mask = 1;
      int ch;
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
	  dIMCTXim(im);
          im_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return -1;
        }
	if (!((1 << chans[ch]) & im->ch_mask))
	  all_in_mask = 0;
      }
      if (all_in_mask) {
	for (i = 0; i < w; ++i) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    i_fsample_t samp = *samps++;
	    int ch = chans[chi];
	    i_fsample_t out;
	    if (ch < color_chans)
	      out = imcms_from_linearf(curves[ch], samp);
	    else
	      out = samp;
	    ((double *)im->idata)[off+ch] = out;
	    ++count;
	  }
	  off += im->channels;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
	  int chi;
	  for (chi = 0; chi < chan_count; ++chi) {
	    int ch = chans[chi];
	    i_fsample_t out;
	    if (im->ch_mask & (1 << ch)) {
	      if (ch < color_chans)
		out = imcms_from_linearf(curves[ch], *samps);
	      else
		out = *samps;
	      ((double *)im->idata)[off+ch] = out;
	    }
	    ++samps;
	    ++count;
	  }
	  off += im->channels;
	}
      }
    }
    else {
      if (chan_count <= 0 || chan_count > im->channels) {
	dIMCTXim(im);
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return -1;
      }
      for (i = 0; i < w; ++i) {
	unsigned mask = 1;
	int ch;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask) {
	    i_fsample_t out;
	    if (ch < color_chans)
	      out = imcms_from_linearf(curves[ch], *samps);
	    else
	      out = *samps;
	    ((double *)im->idata)[off+ch] = out;
	  }
	  ++samps;
          ++count;
	  mask <<= 1;
        }
        off += im->channels;
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
