/*
   Imager "functions" implemented as inline functions.
*/
#ifndef IMAGER_IMINLINE_H_
#define IMAGER_IMINLINE_H_

#ifdef __cplusplus
extern "C" {
#endif

IMAGER_STATIC_INLINE i_img_dim
i_get_linear_samples(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
        i_sample16_t *samp, const int *chan, int chan_count) {
  return (im->vtbl->i_f_get_linear_samples)(im, x, r, y, samp, chan, chan_count);
}

IMAGER_STATIC_INLINE void
i_get_linear_samples_assert(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
        i_sample16_t *samp, const int *chan, int chan_count) {
  i_img_dim result = i_get_linear_samples(im, l, r, y, samp, chan, chan_count);
  IM_UNUSED_VAR(result);
  assert(result == (r-l) * chan_count);
}

IMAGER_STATIC_INLINE i_img_dim
i_get_linear_fsamples(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
         i_fsample_t *samp, const int *chan, int chan_count) {
  return (im->vtbl->i_f_get_linear_fsamples)(im, x, r, y, samp, chan, chan_count);
}

IMAGER_STATIC_INLINE void
i_get_linear_fsamples_assert(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
        i_fsample_t *samp, const int *chan, int chan_count) {
  i_img_dim result = i_get_linear_fsamples(im, l, r, y, samp, chan, chan_count);
  IM_UNUSED_VAR(result);
  assert(result == (r-l) * chan_count);
}

IMAGER_STATIC_INLINE i_img_dim
i_put_linear_samples(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
        const i_sample16_t *samp, const int *chan, int chan_count) {
  return (im->vtbl->i_f_put_linear_samples)(im, x, r, y, samp, chan, chan_count);
}

IMAGER_STATIC_INLINE i_img_dim
i_put_linear_fsamples(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
                  const i_fsample_t *samp, const int *chan, int chan_count) {
  return (im->vtbl->i_f_put_linear_fsamples)(im, x, r, y, samp, chan, chan_count);
}

IMAGER_STATIC_INLINE i_img_dim
i_gsamp(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps,
        const int *chans, int count) {
  return im->vtbl->i_f_gsamp(im, l, r, y, samps, chans, count);
}

IMAGER_STATIC_INLINE i_img_dim
i_gsampf(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps,
         const int *chans, int count) {
  return im->vtbl->i_f_gsampf(im, l, r, y, samps, chans, count);
}

IMAGER_STATIC_INLINE void
i_gsamp_assert(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps,
               const int *chans, int chan_count) {
  i_img_dim result = i_gsamp(im, l, r, y, samps, chans, chan_count);
  IM_UNUSED_VAR(result);
  assert(result == (r-l)*chan_count);
}

IMAGER_STATIC_INLINE void
i_gsampf_assert(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samps,
                const int *chans, int chan_count) {
  i_img_dim result = i_gsampf(im, l, r, y, samps, chans, chan_count);
  IM_UNUSED_VAR(result);
  assert(result == (r-l)*chan_count);
}

/*
=item i_psamp(im, left, right, y, samples, channels, channel_count)
=category Drawing

Writes sample values from C<samples> to C<im> for the horizontal line
(left, y) to (right-1, y) inclusive for the channels specified by
C<channels>, an array of C<int> with C<channel_count> elements.

If C<channels> is C<NULL> then the first C<channels_count> channels
are written to for each pixel.

Returns the number of samples written, which should be (right - left)
* channel_count.  If a channel not in the image is in channels, left
is negative, left is outside the image or y is outside the image,
returns -1 and pushes an error.

=cut
*/

IMAGER_STATIC_INLINE i_img_dim
i_psamp(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_sample_t *samps,
        const int *chans, int count) {
  return im->vtbl->i_f_psamp(im, l, r, y, samps, chans, count);
}

/*
=item i_psampf(im, left, right, y, samples, channels, channel_count)
=category Drawing

Writes floating point sample values from C<samples> to C<im> for the
horizontal line (left, y) to (right-1, y) inclusive for the channels
specified by C<channels>, an array of C<int> with C<channel_count>
elements.

If C<channels> is C<NULL> then the first C<channels_count> channels
are written to for each pixel.

Returns the number of samples written, which should be (right - left)
* channel_count.  If a channel not in the image is in channels, left
is negative, left is outside the image or y is outside the image,
returns -1 and pushes an error.

=cut
*/

IMAGER_STATIC_INLINE i_img_dim
i_psampf(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fsample_t *samps,
        const int *chans, int count) {
  return im->vtbl->i_f_psampf(im, l, r, y, samps, chans, count);
}

/*
=item i_gpix(im, C<x>, C<y>, C<color>)
=category Drawing

Retrieves the C<color> of the pixel (x,y).

Returns 0 if the pixel was retrieved, or -1 if not.

=cut
*/

IMAGER_STATIC_INLINE int
i_gpix(i_img *im, i_img_dim x, i_img_dim y, i_color *val) {
  int count = i_gsamp(im, x, x+1, y, val->channel, NULL, im->channels);
  if (count > 0) {
    int ch;
    for (ch = im->channels; ch < MAXCHANNELS; ++ch) {
      val->channel[ch] = 0;
    }
    return 0;
  }
  else {
    int ch;
    for (ch = 0; ch < MAXCHANNELS; ++ch)
      val->channel[ch] = 0;
    return -1;
  }
}

/*
=item i_gpixf(im, C<x>, C<y>, C<color>)
=category Drawing

Retrieves the floating point C<color> of the pixel (x,y).

Returns 0 if the pixel was retrieved, or -1 if not.

=cut
*/

IMAGER_STATIC_INLINE int
i_gpixf(i_img *im, i_img_dim x, i_img_dim y, i_fcolor *val) {
  int count = i_gsampf(im, x, x+1, y, val->channel, NULL, im->channels);
  if (count > 0) {
    int ch;
    for (ch = im->channels; ch < MAXCHANNELS; ++ch) {
      val->channel[ch] = 0;
    }
    return 0;
  }
  else {
    int ch;
    for (ch = 0; ch < MAXCHANNELS; ++ch)
      val->channel[ch] = 0;
    return -1;
  }
}

/*
=item i_ppix(im, x, y, color)
=category Drawing

Sets the pixel at (x,y) to I<color>.

Returns 0 if the pixel was drawn, or -1 if not.

Does no alpha blending, just copies the channels from the supplied
color to the image.

=cut
*/

IMAGER_STATIC_INLINE int
i_ppix(i_img *im, i_img_dim x, i_img_dim y, const i_color *val) {
  return i_psamp(im, x, x+1, y, val->channel, NULL, im->channels) > 0 ? 0 : -1;
}

/*
=item i_ppixf(im, C<x>, C<y>, C<fcolor>)
=category Drawing

Sets the pixel at (C<x>,C<y>) to the floating point color C<fcolor>.

Returns 0 if the pixel was drawn, or -1 if not.

Does no alpha blending, just copies the channels from the supplied
color to the image.

=cut
*/
IMAGER_STATIC_INLINE int
i_ppixf(i_img *im, i_img_dim x, i_img_dim y, const i_fcolor *val) {
  return i_psampf(im, x, x+1, y, val->channel, NULL, im->channels) > 0 ? 0 : -1;
}

/*
=item i_img_linear()
=category Image Information
=synopsis if (i_img_linear(im)) { ... }

Returns whether the samples of the image are natively linear scale or
not.

=cut
*/

IMAGER_STATIC_INLINE int
i_img_linear(const i_img *im) {
  return im->islinear;
}

/*
=item i_img_refcnt()
=category Image Information

Return the reference count for the image.

=cut
*/

IMAGER_STATIC_INLINE unsigned
i_img_refcnt(const i_img *im) {
  return im->ref_count;
}

/*
=item i_img_virtual()
=category Image Information
=synopsis if (i_img_virtual(im)) { ... }

Returns whether the image is virtual or not.

A virtual image represents a view on another image, or contains pixels
generated by an algorithm, or some combination of the two.

=cut
*/
IMAGER_STATIC_INLINE int
i_img_virtual(const i_img *im) {
  return im->isvirtual;
}

/*
=item i_img_type()
=category Image Information
=synposis if (i_img_type(im) == i_direct_type) { ... }

Returns the type of the image, whether it stores direct color
C<i_direct_type> or palette entries and a paletted C<i_palette_type>.

=cut
*/

IMAGER_STATIC_INLINE i_img_type_t
i_img_type(const i_img *im) {
  return im->type;
}

/*
=item i_img_bits()
=category Image Information
=synopsis i_img_bits_t bits = i_img_bits(im);

Returns the enum entry for the number of bits per sample in the image.

The absolute value of this as an integer is the number of bits in the
representation of the sample.

=cut
*/

IMAGER_STATIC_INLINE i_img_bits_t
i_img_bits(const i_img *im) {
  return im->bits;
}

/*
=item i_img_channels()
=category Image Information
=synopsis int chans = i_img_channels(im);
=synopsis int chans = i_img_getchannels(im);

Returns the number of color and alpha channels in the image.  An RGBA
image returns 4.

=cut
*/

IMAGER_STATIC_INLINE int
i_img_channels(const i_img *im) {
  return im->channels;
}

IMAGER_STATIC_INLINE int
i_img_getchannels(const i_img *im) {
  return im->channels;
}

/*
=item i_img_extrachannels()
=category Image Information
=synopsis int extra = i_img_extrachannels(im);

Returns the number of extra channels, the channels in addition to the
color and alpha channels in the image.

=cut
*/

IMAGER_STATIC_INLINE int
i_img_extrachannels(const i_img * im) {
  return im->extrachannels;
}


/*
=item i_img_totalchannels()
=category Image Information
=synopsis int totalch = i_img_totalchannels(im);

Returns the total number of channels, including both the color, alpha
and extra channels.

=cut
*/

IMAGER_STATIC_INLINE int
i_img_totalchannels(const i_img *im) {
  return im->channels + im->extrachannels;
}

/*
=item i_img_get_width(C<im>)
=category Image Information
=synopsis i_img_dim width = i_img_get_width(im);

Returns the width in pixels of the image.

=cut
*/
IMAGER_STATIC_INLINE i_img_dim
i_img_get_width(const i_img *im) {
  return im->xsize;
}

/*
=item i_img_get_height(C<im>)
=category Image Information
=synopsis i_img_dim height = i_img_get_height(im);

Returns the height in pixels of the image.

=cut
*/
IMAGER_STATIC_INLINE i_img_dim
i_img_get_height(const i_img *im) {
  return im->ysize;
}

/*
=item i_img_color_model(im)
=category Image Information
=synopsis i_color_model_t cm = i_img_color_model(im);

Returns the color model for the image.

=cut
*/
IMAGER_STATIC_INLINE i_color_model_t
i_img_color_model(const i_img *im) {
  return (i_color_model_t)im->channels;
}

/*
=item i_img_alpha_channel(im, &channel)
=category Image Information
=synopsis int alpha_channel;
=synopsis int has_alpha = i_img_alpha_channel(im, &alpha_channel);

Work out the alpha channel for an image.

If the image has an alpha channel, sets C<*channel> to the alpha
channel index and returns non-zero.

If the image has no alpha channel, returns zero and C<*channel> is not
modified.

C<channel> may be C<NULL>.

=cut
*/

IMAGER_STATIC_INLINE int
i_img_alpha_channel(const i_img *im, int *channel) {
  i_color_model_t model = i_img_color_model(im);
  switch (model) {
  case icm_gray_alpha:
  case icm_rgb_alpha:
    if (channel) *channel = (int)model - 1;
    return 1;

  default:
    return 0;
  }
}

/*
=item i_img_has_alpha(C<im>)

=category Image Information

Return true if the image has an alpha channel.

=cut
*/

IMAGER_STATIC_INLINE int
i_img_has_alpha(i_img *im) {
  return i_img_alpha_channel(im, NULL);
}

/*
=item i_img_color_channels(im)
=category Image Information
=synopsis int color_channels = i_img_color_channels(im);

Returns the number of color channels in the image.  For now this is
always 1 (for grayscale) or 3 (for RGB) but may be 0 in some special
cases in a future release of Imager.

=cut
*/

IMAGER_STATIC_INLINE int
i_img_color_channels(const i_img *im) {
  i_color_model_t model = i_img_color_model(im);
  switch (model) {
  case icm_gray_alpha:
  case icm_rgb_alpha:
    return (int)model - 1;

  case icm_gray:
  case icm_rgb:
    return (int)model;

  default:
    return 0;
  }
}

/*
=item im_img_float_new(ctx, x, y, ch)
X<im_img_float_new API>X<i_img_float_new API>
=category Image creation/destruction
=synopsis i_img *img = im_img_float_new(aIMCTX, width, height, channels);
=synopsis i_img *img = i_img_float_new(width, height, channels);

Creates a new float per sample image.

Also callable as C<i_img_float_new(width, height, channels)>.

=cut
*/

IMAGER_STATIC_INLINE i_img *
im_img_float_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_img_float_new_extra(aIMCTX, x, y, ch, 0);
}

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

IMAGER_STATIC_INLINE i_img *
im_img_double_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_img_double_new_extra(aIMCTX, x, y, ch, 0);
}

/*
=item im_img_16_new(ctx, x, y, ch)
X<im_img_16_new API>X<i_img_16_new API>
=category Image creation/destruction
=synopsis i_img *img = im_img_16_new(aIMCTX, width, height, channels);
=synopsis i_img *img = i_img_16_new(width, height, channels);

Creates a new 16-bit per sample image.

Also callable as C<i_img_16_new(width, height, channels)>.

=cut
*/

IMAGER_STATIC_INLINE i_img *
im_img_16_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_img_16_new_extra(aIMCTX, x, y, ch, 0);
}

/*
=item im_img_8_new(ctx, x, y, ch)
X<im_img_8_new API>X<i_img_8_new API>
=category Image creation/destruction
=synopsis i_img *img = im_img_8_new(aIMCTX, width, height, channels);
=synopsis i_img *img = i_img_8_new(width, height, channels);

Creates a new 8-bit per sample image.

Also callable as C<i_img_8_new(width, height, channels)>.

=cut
*/

IMAGER_STATIC_INLINE i_img *
im_img_8_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_img_8_new_extra(aIMCTX, x, y, ch, 0);
}

/*
=item im_lin_img_16_new(ctx, x, y, ch)
X<im_lin_img_16_new API>X<i_lin_img_16_new API>
=category Image creation/destruction
=synopsis i_img *img = im_lin_img_16_new(aIMCTX, width, height, channels);
=synopsis i_img *img = i_lin_img_16_new(width, height, channels);

Create a new 16-bit/linear sample image.

Returns the image on success, or NULL on failure.

Also callable as C<i_img_16_new(x, y, ch)>

=cut
*/

IMAGER_STATIC_INLINE i_img *
im_lin_img_16_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_lin_img_16_new_extra(aIMCTX, x, y, ch, 0);
}

/*
=item im_lin_img_double_new(ctx, x, y, ch)
X<im_lin_img_double_new API>X<i_lin_img_double_new API>
=category Image creation/destruction
=synopsis i_img *img = im_lin_img_double_new(aIMCTX, width, height, channels);
=synopsis i_img *img = i_lin_img_double_new(width, height, channels);

Create a new double/linear sample image.

Returns the image on success, or NULL on failure.

Also callable as C<i_img_double_new(x, y, ch)>

=cut
*/

IMAGER_STATIC_INLINE i_img *
im_lin_img_double_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_lin_img_double_new_extra(aIMCTX, x, y, ch, 0);
}

/*
=item i_img_all_channel_mask()
=category Image Information
=synopsis if (i_img_all_channel_mask(im)) { ... }

Returns true if the images channel mask covers all channels in the
image, including any extra channels.

=cut
*/

IMAGER_STATIC_INLINE int
i_img_all_channel_mask(const i_img *img) {
  const int totalch = i_img_totalchannels(img);
  const unsigned mask = (1 << totalch) - 1;

  return (img->ch_mask & mask) == mask;
}

/*
=item i_img_valid_channel_indexes()
=category Image Information
=synopsis if (!i_img_valid_channel_indexes(im, chans, chan_count)) { return -1; }

Return true if all of the channels specified by C<chans>/C<chan_count>
are present in the image.

Used by image implementations to validate passed in channel lists.

=cut
*/

IMAGER_STATIC_INLINE bool
i_img_valid_channel_indexes(i_img *im, int const *chans, int chan_count) {
  const int total_ch = i_img_totalchannels(im);

  if (chan_count < 1) {
#ifdef IMAGER_NO_CONTEXT
    dIMCTXim(im);
#endif
    im_push_errorf(aIMCTX, 0, "Channel count (%d) must be positive", chan_count);
    return false;
  }
  if (chans) {
    int chi;
    for (chi = 0; chi < chan_count; ++chi) {
      int ch = chans[chi];
      if (ch < 0 || ch >= total_ch) {
#ifdef IMAGER_NO_CONTEXT
        dIMCTXim(im);
#endif
        im_push_errorf(aIMCTX, 0, "Channel %d (index %d) not in this image", ch, chi);
        return false;
      }
    }
    return true;
  }
  else {
    if (chan_count > total_ch) {
#ifdef IMAGER_NO_CONTEXT
      dIMCTXim(im);
#endif
      im_push_errorf(aIMCTX, 0, "Channel count (%d) with NULL chans > total channels (%d)",
                     chan_count, total_ch);
      return false;
    }
    return true;
  }
}

/*
=item i_img_valid_chans_assert(im, chans, chan_count)

Typically just use C<i_assert_valid_channels(im, chans, chan_count);>

For use in an assertion, returns true if one of the following is true:

=over

=item *

C<chans> is NULL and C<chan_count> is positive and less than or equal
to the total number of channels in the image.

=item *

C<chans> is non-NULL and chan_count is positive and chans[0]
.. chans[chan_count-1] are all greater than zero and less than the
total number of channels in the image.

=back

For non-assertion code, use i_img_valid_channel_indexes().

=cut
*/

IMAGER_STATIC_INLINE bool
i_img_valid_chans_assert(i_img *im, const int *chans, int chan_count) {
  const int total_ch = i_img_totalchannels(im);
  if (chan_count <= 0)
    return false;
  if (chans) {
    int chi;
    for (chi = 0; chi < chan_count; ++chi) {
      if (chans[chi] < 0 || chans[chi] >= total_ch)
        return false;
    }
    return true;
  }
  else {
    return chan_count <= total_ch;
  }
}

#define i_assert_valid_channels(im, chans, chan_count) \
  assert(i_img_valid_chans_assert((im), (chans), (chan_count)))

/*
=item i_sametype_chans(C<im>, C<xsize>, C<ysize>, C<channels>)

=category Image creation/destruction
=synopsis i_img *img = i_sametype_chans(src, width, height, channels);

Returns an image of the same type (sample size) with the specified
number of color channels.

For paletted images the equivalent direct type is returned.

=cut
*/

IMAGER_STATIC_INLINE i_img *
i_sametype_chans(i_img *src, i_img_dim xsize, i_img_dim ysize, int channels) {
  return i_sametype_chans_extra(src, xsize, ysize, channels, 0);
}

/*
=item im_align_size(align, size)

Given an alignment and a size, return size rounded up to be a multiple
of the alignment.

C<align> must be a power of two.

=cut
*/

IMAGER_STATIC_INLINE size_t
im_align_size(size_t align, size_t size) {
  assert(("align must be a power of 2" && (align & -align) == align));
  if (size & (align - 1U)) {
    size_t new_size = (size + align) & ~(align - 1);
    assert(new_size > size);
    assert((new_size & (align-1)) == 0);
    return new_size;
  }
  else {
    return size;
  }
}

/*
=item im_aligned_alloc_fatal(align, size)

Allocate aligned memory and abort execution on failure.

This will fallback to mymalloc() if no aligned allocation function was
found during configuration (and return unaligned memory.)

C<align> must be a power of 2.

=cut
*/

IMAGER_STATIC_INLINE void *
im_aligned_alloc_fatal(size_t align, size_t size) {
  void *p = im_aligned_alloc_low(align, size);
  if (!p) {
#ifdef IMAGER_NO_CONTEXT
    dIMCTX;
#endif
    im_fatal(aIMCTX, 3, "Out of memory");
  }
  return p;
}

/*
=item im_aligned_alloc_simd_null(type_size, count)

Allocate (at least) count units of type_size, aligned for SIMD
processing.

Returns NULL on allocation failure or integer overflow.

=cut
*/

IMAGER_STATIC_INLINE void *
im_aligned_alloc_simd_null(size_t type_size, size_t count) {
  size_t size = type_size * count;
  if (size / type_size != count) {
    errno = EINVAL;
    return NULL;
  }

  return im_aligned_alloc_low(IMAGER_ALIGN_SIZE(type_size), size);
}

/*
=item im_aligned_alloc_simd_fatal(type_size, count)

Allocate (at least) count units of type_size, aligned for SIMD
processing.

Aborts the program on allocation failure or integer overflow.

=cut
*/

IMAGER_STATIC_INLINE void *
im_aligned_alloc_simd_fatal(size_t type_size, size_t count) {
  size_t size = type_size * count;
  if (size / type_size != count) {
#ifdef IMAGER_NO_CONTEXT
    dIMCTX;
#endif
    im_fatal(aIMCTX, 3, "Integer overflow calculating allocation");
  }

  void *p = im_aligned_alloc_low(IMAGER_ALIGN_SIZE(type_size), size);
  if (!p) {
#ifdef IMAGER_NO_CONTEXT
    dIMCTX;
#endif
    im_fatal(aIMCTX, 3, "Out of memory");
  }

  return p;
}

#ifdef __cplusplus
}
#endif

#endif
