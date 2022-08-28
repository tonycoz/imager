/*
   Imager "functions" implemented as inline functions.
*/
#ifndef IMAGER_IMINLINE_H_
#define IMAGER_IMINLINE_H_

#ifdef __cplusplus
extern "C" {
#endif

IMAGER_STATIC_INLINE i_img_dim
i_gslin(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
        i_sample16_t *samp, const int *chan, int chan_count) {
  return (im->vtbl->i_f_gslin)(im, x, r, y, samp, chan, chan_count);
}

IMAGER_STATIC_INLINE i_img_dim
i_gslinf(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
         i_fsample_t *samp, const int *chan, int chan_count) {
  return (im->vtbl->i_f_gslinf)(im, x, r, y, samp, chan, chan_count);
}

IMAGER_STATIC_INLINE i_img_dim
i_pslin(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
        const i_sample16_t *samp, const int *chan, int chan_count) {
  return (im->vtbl->i_f_pslin)(im, x, r, y, samp, chan, chan_count);
}

IMAGER_STATIC_INLINE i_img_dim
i_pslinf(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
                  const i_fsample_t *samp, const int *chan, int chan_count) {
  return (im->vtbl->i_f_pslinf)(im, x, r, y, samp, chan, chan_count);
}

/*
=item i_img_linear()

Returns whether the samples of the image are natively linear scale or
not.

=cut
*/

IMAGER_STATIC_INLINE int
i_img_linear(const i_img *im) {
  return im->islinear;
}

/*
=item im_img_float_new(ctx, x, y, ch)
X<im_img_float_new API>X<i_img_float_new API>
=category Image creation/destruction
=synopsis i_img *img = im_img_float_new(aIMCTX, width, height, channels);
=synopsis i_img *img = i_img_float_new(width, height, channels);

Creates a new double per sample image.

Also callable as C<i_img_float_new(width, height, channels)>.

=cut
*/

IMAGER_STATIC_INLINE i_img *
im_img_float_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_img_float_new_extra(aIMCTX, x, y, ch, 0);
}

/*
=item im_lin_img_16_new(ctx, x, y, ch)
X<im_lin_img_16_new API>X<i_lin_img_16_new API>
=category Image creation/destruction
=synopsis i_img *img = im_lin_img_16_new(aIMCTX, width, height, channels);
=synopsis i_img *img = i_lin_img_16_new(width, height, channels);

Create a new 16-bit/sample image.

Returns the image on success, or NULL on failure.

Also callable as C<i_img_16_new(x, y, ch)>

=cut
*/

IMAGER_STATIC_INLINE i_img *
im_lin_img_16_new(pIMCTX, i_img_dim x, i_img_dim y, int ch) {
  return im_lin_img_16_new_extra(aIMCTX, x, y, ch, 0);
}


#ifdef __cplusplus
}
#endif

#endif
