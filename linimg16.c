/*
=head1 NAME

linimg16.c - implements linear 16-bit images

=head1 SYNOPSIS

  i_img *im = i_lin_img_16_new(i_img_dim x, i_img_dim y, int channels);
  # use like a normal image

=head1 DESCRIPTION

Implements 16-bit/linear sample images.

=over

=cut
*/

#define LINIMG_SAMPLE_TYPE i_sample16_t
#define LINIMG_BITS i_16_bits
#define LINIMG_SUFFIX lin16

#define LINIMG_NEW_NAME im_lin_img_16_new
#define LINIMG_NEW_EXTRA_NAME im_lin_img_16_new_extra

#define LINIMG_REP_TO_LIN_SAMPLE(x) (x)
#define LINIMG_REP_TO_LIN_SAMPLEF(x) Sample16ToF(x)

#define LINIMG_REP_TO_GAMMA_SAMPLE(x, chx) \
  imcms_from_linear(curves[chx], (x))
#define LINIMG_REP_TO_GAMMA_SAMPLEF(x, chx)                \
  imcms_from_linearf(curves[ch], Sample16ToF(x))

#define LINIMG_REP_TO_GAMMA_RAW(x) Sample16To8(x)
#define LINIMG_REP_TO_GAMMAF_RAW(x) Sample16ToF(x)

#define LINIMG_LIN_SAMPLE_TO_REP(x) (x)
#define LINIMG_LIN_SAMPLEF_TO_REP(x) SampleFTo16(x)

#define LINIMG_GAMMA_TO_REP(x, chx) \
  imcms_to_linear(curves[chx], (x))
#define LINIMG_GAMMAF_TO_REP(x, chx) \
  SampleFTo16(imcms_to_linearf(curves[chx], (x)))

#define LINIMG_GAMMA_RAW_TO_REP(x) Sample8To16(x)
#define LINIMG_GAMMAF_RAW_TO_REP(x) SampleFTo16(x)

#include "imlinimg.h"

/*
=item i_img_to_linrgb16(im)

=category Image creation

Returns a 16-bit/linear sample version of the supplied image.

Returns the image on success, or NULL on failure.

=cut
*/

i_img *
i_img_to_linrgb16(i_img *im) {
  i_img *targ;
  i_sample16_t *line;
  i_img_dim y;
  dIMCTXim(im);
  int totalch = i_img_total_channels(im);

  targ = im_lin_img_16_new_extra(aIMCTX, im->xsize, im->ysize, im->channels, im->extrachannels);
  if (!targ)
    return NULL;
  
  line = mymalloc(sizeof(i_sample16_t) * (size_t)totalch * im->xsize);
  for (y = 0; y < im->ysize; ++y) {
    i_get_linear_samples(im, 0, im->xsize, y, line, NULL, totalch);
    i_put_linear_samples(targ, 0, im->xsize, y, line, NULL, totalch);
  }

  myfree(line);

  return targ;
}

