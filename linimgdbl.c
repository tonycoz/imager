/*
=head1 NAME

linimgdbl.c - implements linear double/sample images

=head1 SYNOPSIS

  i_img *im = i_lin_img_double_new(i_img_dim x, i_img_dim y, int channels);
  # use like a normal image

=head1 DESCRIPTION

Implements double/linear sample images.

=over

=cut
*/

#define LINIMG_SAMPLE_TYPE i_fsample_t
#define LINIMG_BITS i_double_bits
#define LINIMG_SUFFIX lindbl

#define LINIMG_NEW_NAME im_lin_img_double_new
#define LINIMG_NEW_EXTRA_NAME im_lin_img_double_new_extra

#define LINIMG_REP_TO_LIN_SAMPLE(x) SampleFTo16(x)
#define LINIMG_REP_TO_LIN_SAMPLEF(x) (x)

#define LINIMG_REP_TO_GAMMA_SAMPLE(x, chx) \
  SampleFTo8(imcms_from_linearf(curves[chx], (x)))
#define LINIMG_REP_TO_GAMMA_SAMPLEF(x, chx) \
  imcms_from_linearf(curves[ch], (x))

#define LINIMG_REP_TO_GAMMA_RAW(x) SampleFTo8(x)
#define LINIMG_REP_TO_GAMMAF_RAW(x) (x)

#define LINIMG_LIN_SAMPLE_TO_REP(x) Sample16ToF(x)
#define LINIMG_LIN_SAMPLEF_TO_REP(x) (x)

#define LINIMG_GAMMA_TO_REP(x, chx) \
  imcms_to_linearf(curves[chx], Sample8ToF(x))
#define LINIMG_GAMMAF_TO_REP(x, chx) \
  imcms_to_linearf(curves[chx], (x))

#define LINIMG_GAMMA_RAW_TO_REP(x) Sample8ToF(x)
#define LINIMG_GAMMAF_RAW_TO_REP(x) (x)

#include "imlinimg.h"

/*
=item i_img_to_linrgbdbl(im)

=category Image creation

Returns a double/linear sample version of the supplied image.

Returns the image on success, or NULL on failure.

=cut
*/

i_img *
i_img_to_linrgbdbl(i_img *im) {
  i_img *targ;
  i_fsample_t *line;
  i_img_dim y;
  dIMCTXim(im);
  const int totalch = i_img_totalchannels(im);

  targ = im_lin_img_double_new_extra(aIMCTX, im->xsize, im->ysize, im->channels, im->extrachannels);
  if (!targ)
    return NULL;
  
  line = mymalloc(sizeof(i_fsample_t) * (size_t)totalch * im->xsize);
  for (y = 0; y < im->ysize; ++y) {
    i_gslinf(im, 0, im->xsize, y, line, NULL, totalch);
    i_pslinf(targ, 0, im->xsize, y, line, NULL, totalch);
  }

  myfree(line);

  return targ;
}

