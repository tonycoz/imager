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


#define IMG_SAMPLE_TYPE i_sample16_t
#define IMG_BITS i_16_bits
#define IMG_NEW_EXTRA_NAME im_img_16_new_extra
#define IMG_SUFFIX gam16
#define IMG_REP_TO_GAMMA_SAMPLE(x) (((x) + 127) / 257)
#define IMG_REP_TO_GAMMA_SAMPLEF(x) Sample16ToF(x)
#define IMG_REP_TO_LIN_SAMPLE(x, ch) SampleFTo16(imcms_to_linearf(curves[ch], Sample16ToF(x)))
#define IMG_REP_TO_LIN_SAMPLEF(x, ch) imcms_to_linearf(curves[ch], Sample16ToF(x))
#define IMG_REP_TO_LIN_SAMPLE_RAW(x) (x)
#define IMG_REP_TO_LIN_SAMPLEF_RAW(x) Sample16ToF(x)
#define IMG_GAMMA_TO_REP(x) Sample8To16(x)
#define IMG_GAMMAF_TO_REP(x) SampleFTo16(x)
#define IMG_LIN_SAMPLE_TO_REP(x, ch) SampleFTo16(imcms_from_linearf(curves[ch], Sample16ToF(x)))
#define IMG_LIN_SAMPLEF_TO_REP(x, ch) SampleFTo16(imcms_from_linearf(curves[ch], (x)))
#define IMG_LIN_SAMPLE_RAW_TO_REP(x) (x)
#define IMG_LIN_SAMPLEF_RAW_TO_REP(x) SampleFTo16(x)

#include "imgamimg.h"


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


/*
=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
*/
