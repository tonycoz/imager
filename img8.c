#define IMAGER_NO_CONTEXT

#include "imager.h"
#include "imageri.h"
#include "imapiver.h"

static i_img_dim i_glinf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fcolor *vals);
static i_img_dim i_gsamp_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, const int *chans, int chan_count);
static i_img_dim i_gsampf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, const int *chans, int chan_count);
static i_img_dim i_psamp_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample_t *samps, const int *chans, int chan_count);
static i_img_dim i_psampf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps, const int *chans, int chan_count);
static i_image_data_alloc_t *
i_data_8(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
         void **p, size_t *size, int *extra);

static const i_img_vtable
vtable_8bit = {
  IMAGER_API_LEVEL,
  
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

  i_gsamp_bits_fb,
  NULL, /* i_f_psamp_bits */

  i_psamp_d,
  i_psampf_d,

  i_data_8
};

/*
=item im_img_8_new(ctx, x, y, ch)
X<im_img_8_new API>X<i_img_8_new API>
=category Image creation/destruction
=synopsis i_img *img = im_img_8_new(aIMCTX, width, height, channels);
=synopsis i_img *img = i_img_8_new(width, height, channels);

Creates a new image object I<x> pixels wide, and I<y> pixels high with
I<ch> channels.

=cut
*/

i_img *
im_img_8_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_img_8_new_extra(aIMCTX, x, y, ch, 0);
}

/*
=item im_img_8_new_extra(ctx, x, y, ch extra)
X<im_img_8_new_extra API>X<i_img_8_new_extra API>
=category Image creation/destruction
=synopsis i_img *img = im_img_8_new_extra(aIMCTX, width, height, channels, extra);
=synopsis i_img *img = i_img_8_new_extra(width, height, channels, extra);

Creates a new image object I<x> pixels wide, and I<y> pixels high with
I<ch> color/alpha channels and C<extra> extra channels.

=cut
*/

i_img *
im_img_8_new_extra(pIMCTX, i_img_dim x, i_img_dim y, int ch, int extra) {
  i_img *im;
  size_t bytes;
  int totalch;

  im_clear_error(aIMCTX);

  im_log((aIMCTX, 1,"im_img_8_new_extra(x %" i_DF ", y %" i_DF ", ch %d, extra %d)\n",
	  i_DFc(x), i_DFc(y), ch, extra));

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
  /* check this multiplication doesn't overflow */
  totalch = ch + extra;
  bytes = (i_img_dim_u)x * (i_img_dim_u)y * (i_img_dim_u)totalch;
  if (bytes / (i_img_dim_u)y / (i_img_dim_u)totalch != (i_img_dim_u)x) {
    im_push_errorf(aIMCTX, 0, "integer overflow calculating image allocation");
    return NULL;
  }

  im = im_img_new(aIMCTX, &vtable_8bit, x, y, ch, extra, i_8_bits);
  if (im == NULL) {
    return NULL;
  }

  im->bytes = bytes;
  im->idata = myzmalloc(im->bytes);
  
  im_img_init(aIMCTX, im);
  
  im_log((aIMCTX, 1,"(%p) <- i_img_8_new\n",im));
  return im;
}

/*
=head2 8-bit per sample image internal functions

These are the functions installed in an 8-bit per sample image.

=over

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
i_img_dim
i_glinf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fcolor *vals) {
  int ch;
  unsigned totalch = im->channels + im->extrachannels;
  i_img_dim count, i;
  unsigned char *data;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * totalch;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch)
	vals[i].channel[ch] = Sample8ToF(data[ch]);
      data += totalch;
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_gsamp_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, int *chans, int chan_count)

Reads sample values from im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples read (which should be (r-l) * bits_set(chan_mask)

=cut
*/
static
i_img_dim
i_gsamp_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, 
              const int *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;
  unsigned char *data;
  unsigned totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * totalch;
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
          *samps++ = data[chans[ch]];
          ++count;
        }
        data += totalch;
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
          *samps++ = data[ch];
          ++count;
        }
        data += totalch;
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_gsampf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, int *chans, int chan_count)

Reads sample values from im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chan_mask, where bit 0 is the first
channel.

Returns the number of samples read (which should be (r-l) * bits_set(chan_mask)

=cut
*/
static
i_img_dim
i_gsampf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps, 
           const int *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;
  unsigned char *data;
  unsigned totalch = im->channels + im->extrachannels;  

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * totalch;
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
          *samps++ = Sample8ToF(data[chans[ch]]);
          ++count;
        }
        data += totalch;
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
          *samps++ = Sample8ToF(data[ch]);
          ++count;
        }
        data += totalch;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_psamp_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps, int *chans, int chan_count)

Writes sample values to im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples written (which should be (r-l) *
bits_set(chan_mask)

=cut
*/

static
i_img_dim
i_psamp_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
	  const i_sample_t *samps, const int *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;
  unsigned char *data;
  unsigned totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * totalch;
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
	    data[chans[ch]] = *samps++;
	    ++count;
	  }
	  data += totalch;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
	  for (ch = 0; ch < chan_count; ++ch) {
	    if (im->ch_mask & (1 << (chans[ch])))
	      data[chans[ch]] = *samps;
	    ++samps;
	    ++count;
	  }
	  data += totalch;
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
	    data[ch] = *samps;
	  ++samps;
          ++count;
	  mask <<= 1;
        }
        data += totalch;
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
=item i_psampf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps, int *chans, int chan_count)

Writes sample values to im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples written (which should be (r-l) *
bits_set(chan_mask)

=cut
*/

static
i_img_dim
i_psampf_d(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
	  const i_fsample_t *samps, const int *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;
  unsigned char *data;
  unsigned totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * totalch;
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
	    data[chans[ch]] = SampleFTo8(*samps);
	    ++samps;
	    ++count;
	  }
	  data += totalch;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
	  for (ch = 0; ch < chan_count; ++ch) {
	    if (im->ch_mask & (1 << (chans[ch])))
	      data[chans[ch]] = SampleFTo8(*samps);
	    ++samps;
	    ++count;
	  }
	  data += totalch;
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
	    data[ch] = SampleFTo8(*samps);
	  ++samps;
          ++count;
	  mask <<= 1;
        }
        data += totalch;
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
i_data_8(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
         void **pdata, size_t *psize, int *pextra) {
  if ((im->extrachannels && !(flags & idf_extras))
      || (flags & idf_otherendian)
      || bits != i_8_bits
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

Arnar M. Hrafnkelsson <addi@umich.edu>

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

L<Imager>

=cut
*/
