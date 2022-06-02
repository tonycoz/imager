/*
=head1 NAME

img16.c - implements 16-bit images

=head1 SYNOPSIS

  i_img *im = i_img_16_new(i_img_dim x, i_img_dim y, int channels);
  # use like a normal image

=head1 DESCRIPTION

Implements 16-bit/sample images.

This basic implementation is required so that we have some larger 
sample image type to work with.

=over

=cut
*/

#define IMAGER_NO_CONTEXT

#include "imager.h"
#include "imageri.h"
#include "imapiver.h"
#ifdef IMAGER_USE_INTTYPES_H
#include <inttypes.h>
#endif

static int i_ppix_d16(i_img *im, i_img_dim x, i_img_dim y, const i_color *val);
static int i_gpix_d16(i_img *im, i_img_dim x, i_img_dim y, i_color *val);
static i_img_dim i_glin_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_color *vals);
static i_img_dim i_plin_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_color *vals);
static int i_ppixf_d16(i_img *im, i_img_dim x, i_img_dim y, const i_fcolor *val);
static int i_gpixf_d16(i_img *im, i_img_dim x, i_img_dim y, i_fcolor *val);
static i_img_dim i_glinf_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fcolor *vals);
static i_img_dim i_plinf_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fcolor *vals);
static i_img_dim i_gsamp_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, 
                       int const *chans, int chan_count);
static i_img_dim i_gsampf_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, 
                        int const *chans, int chan_count);
static i_img_dim i_gsamp_bits_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, unsigned *samps, 
			    int const *chans, int chan_count, int bits);
static i_img_dim i_psamp_bits_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, unsigned const *samps, 
			    int const *chans, int chan_count, int bits);
static i_img_dim i_psamp_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample_t *samps, const int *chans, int chan_count);
static i_img_dim i_psampf_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps, const int *chans, int chan_count);
static i_image_data_alloc_t *
i_data_16(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
          void **p, size_t *size, int *extra);

/*
=item vtable_16bit

Vtable for 16-bit direct images.

Internal.

=cut
*/
static const i_img_vtable
vtable_16bit = {
  IMAGER_API_LEVEL,
  
  i_ppix_d16, /* i_f_ppix */
  i_ppixf_d16, /* i_f_ppixf */
  i_plin_d16, /* i_f_plin */
  i_plinf_d16, /* i_f_plinf */
  i_gpix_d16, /* i_f_gpix */
  i_gpixf_d16, /* i_f_gpixf */
  i_glin_d16, /* i_f_glin */
  i_glinf_d16, /* i_f_glinf */
  i_gsamp_d16, /* i_f_gsamp */
  i_gsampf_d16, /* i_f_gsampf */

  NULL, /* i_f_gpal */
  NULL, /* i_f_ppal */
  NULL, /* i_f_addcolors */
  NULL, /* i_f_getcolors */
  NULL, /* i_f_colorcount */
  NULL, /* i_f_maxcolors */
  NULL, /* i_f_findcolor */
  NULL, /* i_f_setcolors */

  NULL, /* i_f_destroy */

  i_gsamp_bits_d16,
  i_psamp_bits_d16,

  i_psamp_d16,
  i_psampf_d16,

  i_data_16
};

/* we have a real 16-bit unsigned integer */
#define STORE16(bytes, offset, word) \
   (((i_sample16_t *)(bytes))[offset] = (word))
#define STORE8as16(bytes, offset, byte) \
   (((i_sample16_t *)(bytes))[offset] = (byte) * 256 + (byte))
#define GET16(bytes, offset) \
     (((i_sample16_t *)(bytes))[offset])

#define GET16as8(bytes, offset) \
     ((((i_sample16_t *)(bytes))[offset]+127) / 257)

/*
=item im_img_16_new(ctx, x, y, ch)
X<im_img_16_new API>X<i_img_16_new API>
=category Image creation/destruction
=synopsis i_img *img = im_img_16_new(aIMCTX, width, height, channels);
=synopsis i_img *img = i_img_16_new(width, height, channels);

Create a new 16-bit/sample image.

Returns the image on success, or NULL on failure.

Also callable as C<i_img_16_new(x, y, ch)>

=cut
*/

i_img *
im_img_16_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_img_16_new_extra(aIMCTX, x, y, ch, 0);
}

/*
=item im_img_16_new_extra(ctx, x, y, ch)
X<im_img_16_new_extra API>X<i_img_16_new_extra API>
=category Image creation/destruction
=synopsis i_img *img = im_img_16_new_extra(aIMCTX, width, height, channels);
=synopsis i_img *img = i_img_16_new_extra(width, height, channels);

Create a new 16-bit/sample image.

Returns the image on success, or NULL on failure.

Also callable as C<i_img_16_new_extra(x, y, ch)>

=cut
*/

i_img *
im_img_16_new_extra(pIMCTX, i_img_dim x, i_img_dim y, int ch, int extra) {
  i_img *im;
  size_t bytes, line_bytes;
  int totalch;

  im_log((aIMCTX, 1,"i_img_16_new_extra(x %" i_DF ", y %" i_DF ", ch %d)\n",
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
  bytes =  (i_img_dim_u)x * (i_img_dim_u)y * (i_img_dim_u)totalch * 2U;
  if (bytes / (i_img_dim_u)y / (i_img_dim_u)totalch / 2U != (i_img_dim_u)x) {
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

  im = im_img_new(aIMCTX, &vtable_16bit, x, y, ch, extra, i_16_bits);
  if (im == NULL)
    return NULL;

  im->bytes = bytes;
  im->idata = mymalloc(im->bytes);
  memset(im->idata, 0, im->bytes);

  im_img_init(aIMCTX, im);

  return im;
}

/*
=item i_img_to_rgb16(im)

=category Image creation

Returns a 16-bit/sample version of the supplied image.

Returns the image on success, or NULL on failure.

=cut
*/

i_img *
i_img_to_rgb16(i_img *im) {
  i_img *targ;
  i_fcolor *line;
  i_img_dim y;
  dIMCTXim(im);

  targ = im_img_16_new(aIMCTX, im->xsize, im->ysize, im->channels);
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

static int i_ppix_d16(i_img *im, i_img_dim x, i_img_dim y, const i_color *val) {
  i_img_dim off;
  int ch;
  unsigned totalch = im->channels + im->extrachannels;

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize) 
    return -1;

  off = (x + y * im->xsize) * totalch;
  if (I_ALL_CHANNELS_WRITABLE(im)) {
    for (ch = 0; ch < im->channels; ++ch)
      STORE8as16(im->idata, off+ch, val->channel[ch]);
  }
  else {
    for (ch = 0; ch < im->channels; ++ch)
      if (im->ch_mask & (1 << ch))
	STORE8as16(im->idata, off+ch, val->channel[ch]);
  }

  return 0;
}

static int i_gpix_d16(i_img *im, i_img_dim x, i_img_dim y, i_color *val) {
  i_img_dim off;
  int ch;
  unsigned totalch = im->channels + im->extrachannels;

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize) 
    return -1;

  off = (x + y * im->xsize) * totalch;
  for (ch = 0; ch < im->channels; ++ch)
    val->channel[ch] = GET16as8(im->idata, off+ch);

  return 0;
}

static int
i_ppixf_d16(i_img *im, i_img_dim x, i_img_dim y, const i_fcolor *val) {
  i_img_dim off;
  int ch;
  unsigned totalch = im->channels + im->extrachannels;

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize) 
    return -1;

  off = (x + y * im->xsize) * totalch;
  if (I_ALL_CHANNELS_WRITABLE(im)) {
    for (ch = 0; ch < im->channels; ++ch)
      STORE16(im->idata, off+ch, SampleFTo16(val->channel[ch]));
  }
  else {
    for (ch = 0; ch < im->channels; ++ch)
      if (im->ch_mask & (1 << ch))
	STORE16(im->idata, off+ch, SampleFTo16(val->channel[ch]));
  }

  return 0;
}

static int
i_gpixf_d16(i_img *im, i_img_dim x, i_img_dim y, i_fcolor *val) {
  i_img_dim off;
  int ch;
  unsigned totalch = im->channels + im->extrachannels;

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize) 
    return -1;

  off = (x + y * im->xsize) * totalch;
  for (ch = 0; ch < im->channels; ++ch)
    val->channel[ch] = Sample16ToF(GET16(im->idata, off+ch));

  return 0;
}

static i_img_dim
i_glin_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_color *vals) {
  int ch;
  i_img_dim count, i;
  i_img_dim off;
  unsigned totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
	vals[i].channel[ch] = GET16as8(im->idata, off+ch);
      }
      off += totalch;
    }
    return count;
  }
  else {
    return 0;
  }
}

static i_img_dim
i_plin_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_color *vals) {
  int ch;
  i_img_dim count, i;
  i_img_dim off;
  unsigned totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    count = r - l;
    if (I_ALL_CHANNELS_WRITABLE(im)) {
      for (i = 0; i < count; ++i) {
	for (ch = 0; ch < im->channels; ++ch) {
	  STORE8as16(im->idata, off+ch, vals[i].channel[ch]);
	}
        off += totalch;
      }
    }
    else {
      for (i = 0; i < count; ++i) {
	for (ch = 0; ch < im->channels; ++ch) {
	  if (im->ch_mask & (1 << ch))
	    STORE8as16(im->idata, off+ch, vals[i].channel[ch]);
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
i_glinf_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fcolor *vals) {
  int ch;
  i_img_dim count, i;
  i_img_dim off;
  unsigned totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
	vals[i].channel[ch] = Sample16ToF(GET16(im->idata, off+ch));
      }
      off += totalch;
    }
    return count;
  }
  else {
    return 0;
  }
}

static i_img_dim
i_plinf_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fcolor *vals) {
  int ch;
  i_img_dim count, i;
  i_img_dim off;
  unsigned totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * totalch;
    count = r - l;
    if (I_ALL_CHANNELS_WRITABLE(im)) {
      for (i = 0; i < count; ++i) {
	for (ch = 0; ch < im->channels; ++ch) {
	  STORE16(im->idata, off+ch, SampleFTo16(vals[i].channel[ch]));
	}
        off += totalch;
      }
    }
    else {
      for (i = 0; i < count; ++i) {
	for (ch = 0; ch < im->channels; ++ch) {
	  if (im->ch_mask & (1 << ch))
	    STORE16(im->idata, off+ch, SampleFTo16(vals[i].channel[ch]));
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
i_gsamp_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, 
            int const *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;
  i_img_dim off;
  unsigned totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
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
          *samps++ = GET16as8(im->idata, off+chans[ch]);
          ++count;
        }
        off += totalch;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	dIMCTXim(im);
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return 0;
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = GET16as8(im->idata, off+ch);
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

static i_img_dim i_gsampf_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, 
                        int const *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;
  i_img_dim off;
  int totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
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
          *samps++ = Sample16ToF(GET16(im->idata, off+chans[ch]));
          ++count;
        }
        off += totalch;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	dIMCTXim(im);
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return 0;
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = Sample16ToF(GET16(im->idata, off+ch));
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
i_gsamp_bits_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, unsigned *samps, 
			    int const *chans, int chan_count, int bits) {
  int ch;
  i_img_dim count, i, w;
  i_img_dim off;

  if (bits != 16) {
    return i_gsamp_bits_fb(im, l, r, y, samps, chans, chan_count, bits);
  }

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    unsigned totalch = im->channels + im->extrachannels;

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
          return -1;
        }
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = GET16(im->idata, off+chans[ch]);
          ++count;
        }
        off += totalch;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	dIMCTXim(im);
	i_push_error(0, "Invalid channel count");
	return -1;
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = GET16(im->idata, off+ch);
          ++count;
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
i_psamp_bits_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, unsigned const *samps, 
			    int const *chans, int chan_count, int bits) {
  int ch;
  i_img_dim count, i, w;
  i_img_dim off;

  if (bits != 16) {
    dIMCTXim(im);
    i_push_error(0, "Invalid bits for 16-bit image");
    return -1;
  }

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
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
          return -1;
        }
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & (1 << ch))
	    STORE16(im->idata, off+chans[ch], *samps);
	  ++samps;
	  ++count;
        }
        off += totalch;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	dIMCTXim(im);
	i_push_error(0, "Invalid channel count");
	return -1;
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & (1 << ch)) 
	    STORE16(im->idata, off+ch, *samps);
	  ++samps;
          ++count;
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
=item i_psamp_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample_t *samps, int *chans, int chan_count)

Writes sample values to im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples written (which should be (r-l) *
bits_set(chan_mask)

=cut
*/

static
i_img_dim
i_psamp_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
	  const i_sample_t *samps, const int *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    unsigned totalch = im->channels + im->extrachannels;
    i_img_dim offset;

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
	    STORE8as16(im->idata, offset + chans[ch], *samps);
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
	    STORE8as16(im->idata, offset + chans[ch], *samps);
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
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return -1;
      }
      for (i = 0; i < w; ++i) {
	unsigned mask = 1;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask)
	    STORE8as16(im->idata, offset + ch, *samps);
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

static
i_img_dim
i_psampf_d16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
	  const i_fsample_t *samps, const int *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_img_dim offset;
    int totalch = im->channels + im->extrachannels;

    if (r > im->xsize)
      r = im->xsize;
    offset = (l + y * im->xsize) * totalch;
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
	    unsigned samp16 = SampleFTo16(*samps);
	    STORE16(im->idata, offset + chans[ch], samp16);
	    ++samps;
	    ++count;
	  }
	  offset += totalch;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
	  for (ch = 0; ch < chan_count; ++ch) {
	    if (im->ch_mask & (1 << (chans[ch]))) {
	      unsigned samp16 = SampleFTo16(*samps);
	      STORE16(im->idata, offset + chans[ch], samp16);
	    }
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
	im_push_errorf(aIMCTX, 0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return -1;
      }
      for (i = 0; i < w; ++i) {
	unsigned mask = 1;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask) {
	    unsigned samp16 = SampleFTo16(*samps);
	    STORE16(im->idata, offset + ch, samp16);
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

static i_image_data_alloc_t *
i_data_16(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
          void **pdata, size_t *psize, int *pextra) {
  if ((im->extrachannels && !(flags & idf_extras))
      || (flags & idf_otherendian)
      || bits != i_16_bits
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
=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
*/
