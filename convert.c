/*
=head1 NAME

  convert.c - image conversions

=head1 SYNOPSIS

  i_convert(outimage, srcimage, coeff, outchans, inchans)

=head1 DESCRIPTION

Converts images from one format to another, typically in this case for
converting from RGBA to greyscale and back.

=over

=cut
*/

#include "image.h"


/*
=item i_convert(im, src, coeff, outchan, inchan)

Converts the image src into another image.

coeff contains the co-efficients of an outchan x inchan matrix, for
each output pixel:

              coeff[0], coeff[1] ...
  im[x,y] = [ coeff[inchan], coeff[inchan+1]...        ] * [ src[x,y], 1]
              ...              coeff[inchan*outchan-1]

If im has the wrong number of channels or is the wrong size then
i_convert() will re-create it.

=cut
*/

int
i_convert(i_img *im, i_img *src, float *coeff, int outchan, int inchan)
{
  i_color *vals;
  int x, y;
  int i, j;
  int ilimit;
  double work[MAXCHANNELS];

  mm_log((1,"i_convert(im %p, src, %p, coeff %p,outchan %d, inchan %d)\n",im,src, coeff,outchan, inchan));
 
  i_clear_error();

  ilimit = inchan;
  if (ilimit > src->channels)
    ilimit = src->channels;
  if (outchan > MAXCHANNELS) {
    i_push_error(0, "cannot have outchan > MAXCHANNELS");
    return 0;
  }

  /* first check the output image */
  if (im->channels != outchan || im->xsize != src->xsize 
      || im->ysize != src->ysize) {
    i_img_empty_ch(im, src->xsize, src->ysize, outchan);
  }
  vals = mymalloc(sizeof(i_color) * src->xsize);
  for (y = 0; y < src->ysize; ++y) {
    i_glin(src, 0, src->xsize, y, vals);
    for (x = 0; x < src->xsize; ++x) {
      for (j = 0; j < outchan; ++j) {
	work[j] = 0;
	for (i = 0; i < ilimit; ++i) {
	  work[j] += coeff[i+inchan*j] * vals[x].channel[i];
	}
	if (i < inchan) {
	  work[j] += coeff[i+inchan*j] * 255.9;
	}
      }
      for (j = 0; j < outchan; ++j) {
	if (work[j] < 0)
	  vals[x].channel[j] = 0;
	else if (work[j] >= 256)
	  vals[x].channel[j] = 255;
	else
	  vals[x].channel[j] = work[j];
      }
    }
    i_plin(im, 0, src->xsize, y, vals);
  }
  myfree(vals);
  return 1;
}

/*
=back

=head1 SEE ALSO

Imager(3)

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=cut
*/
