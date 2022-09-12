#define IMG_SAMPLE_TYPE i_sample_t
#define IMG_BITS i_8_bits
#define IMG_NEW_EXTRA_NAME im_img_8_new_extra
#define IMG_SUFFIX gam8
#define IMG_REP_TO_GAMMA_SAMPLE(x) (x)
#define IMG_REP_TO_GAMMA_SAMPLEF(x) Sample8ToF(x)
#define IMG_REP_TO_LIN_SAMPLE(x, ch) (imcms_to_linear(curves[ch], x))
#define IMG_REP_TO_LIN_SAMPLEF(x, ch) imcms_to_linearf(curves[ch], Sample8ToF(x))
#define IMG_REP_TO_LIN_SAMPLE_RAW(x) Sample8To16(x)
#define IMG_REP_TO_LIN_SAMPLEF_RAW(x) Sample8ToF(x)
#define IMG_GAMMA_TO_REP(x) (x)
#define IMG_GAMMAF_TO_REP(x) SampleFTo8(x)
#define IMG_LIN_SAMPLE_TO_REP(x, ch) (imcms_from_linear(curves[ch], (x)))
#define IMG_LIN_SAMPLEF_TO_REP(x, ch) SampleFTo8(imcms_from_linearf(curves[ch], (x)))
#define IMG_LIN_SAMPLE_RAW_TO_REP(x) Sample16To8(x)
#define IMG_LIN_SAMPLEF_RAW_TO_REP(x) SampleFTo8(x)

#include "imgamimg.h"

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

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

L<Imager>

=cut
*/
