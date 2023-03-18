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

static i_img_dim
i_gsamp_bits_gam16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, unsigned *samps,
                   int const *chans, int chan_count, int bits);
static i_img_dim
i_psamp_bits_gam16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, unsigned const *samps,
                 int const *chans, int chan_count, int bits);

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
#define IMG_GSAMP_BITS i_gsamp_bits_gam16
#define IMG_PSAMP_BITS i_psamp_bits_gam16

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
  i_fsample_t *line;
  i_img_dim y;
  dIMCTXim(im);
  size_t num_samps;
  i_img_dim sampn;
  int totalch = i_img_totalchannels(im);

  targ = im_img_16_new_extra(aIMCTX, im->xsize, im->ysize, im->channels, im->extrachannels);
  if (!targ)
    return NULL;
  num_samps = im->xsize * totalch;
  line = mymalloc(sizeof(i_fsample_t) * num_samps);
  for (y = 0; y < im->ysize; ++y) {
    i_gsampf(im,   0, im->xsize, y, line, NULL, totalch);
    i_psampf(targ, 0, im->xsize, y, line, NULL, totalch);
  }

  myfree(line);

  return targ;
}

static i_img_dim 
i_gsamp_bits_gam16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
                   unsigned *samps, int const *chans, int chan_count, int bits) {
  i_assert_valid_channels(im, chans, chan_count);

  if (bits != 16) {
    return i_gsamp_bits_fb(im, l, r, y, samps, chans, chan_count, bits);
  }

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    int totalch = im->channels + im->extrachannels;
    const IMG_SAMPLE_TYPE *data = (const IMG_SAMPLE_TYPE *)im->idata;

    if (r > im->xsize)
      r = im->xsize;

    i_img_dim off = (l + y * im->xsize) * totalch;
    i_img_dim w = r - l;
    i_img_dim count = 0;

    if (chans) {
      i_img_dim i;
      for (i = 0; i < w; ++i) {
        int chi;
        for (chi = 0; chi < chan_count; ++chi) {
          int ch = chans[chi];
          *samps++ = data[off+ch];
          ++count;
        }
        off += totalch;
      }
    }
    else {
      i_img_dim i;
      for (i = 0; i < w; ++i) {
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = data[off+ch];
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
i_psamp_bits_gam16(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y,
                   unsigned const *samps, int const *chans, int chan_count, int bits) {
  i_assert_valid_channels(im, chans, chan_count);

  if (bits != 16) {
    return i_psamp_bits_fb(im, l, r, y, samps, chans, chan_count, bits);
  }

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    int totalch = im->channels + im->extrachannels;
    IMG_SAMPLE_TYPE *data = (IMG_SAMPLE_TYPE *)im->idata;

    if (r > im->xsize)
      r = im->xsize;
    i_img_dim off = (l + y * im->xsize) * totalch;
    i_img_dim w = r - l;
    i_img_dim count = 0;

    if (chans) {
      i_img_dim i;
      for (i = 0; i < w; ++i) {
        int chi;
        for (chi = 0; chi < chan_count; ++chi) {
          int ch = chans[chi];
          if (im->ch_mask & (1 << ch))
            data[off+ch] = *samps;
          ++samps;
          ++count;
        }
        off += totalch;
      }
    }
    else {
      i_img_dim i;
      for (i = 0; i < w; ++i) {
        int ch;
        for (ch = 0; ch < chan_count; ++ch) {
          if (im->ch_mask & (1 << ch)) 
            data[off+ch] = *samps;
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
