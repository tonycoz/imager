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

#ifdef __cplusplus
}
#endif

#endif
