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

#include "imager.h"


/*
=item i_convert(src, coeff, outchan, inchan)

Converts the image src into another image.

coeff contains the co-efficients of an outchan x inchan matrix, for
each output pixel:

              coeff[0], coeff[1] ...
  im[x,y] = [ coeff[inchan], coeff[inchan+1]...        ] * [ src[x,y], 1]
              ...              coeff[inchan*outchan-1]

If im has the wrong number of channels or is the wrong size then
i_convert() will re-create it.

Now handles images with more than 8-bits/sample.

=cut
*/

i_img *
i_convert(i_img *src, const float *coeff, int outchan, int inchan) {
  int x, y;
  int i, j;
  int ilimit;
  double work[MAXCHANNELS];
  i_img *im = NULL;

  mm_log((1,"i_convert(src %p, coeff %p,outchan %d, inchan %d)\n",im,src, coeff,outchan, inchan));
 
  i_clear_error();

  ilimit = inchan;
  if (ilimit > src->channels)
    ilimit = src->channels;
  if (outchan > MAXCHANNELS) {
    i_push_error(0, "cannot have outchan > MAXCHANNELS");
    return 0;
  }

  if (src->type == i_direct_type) {
    im = i_sametype_chans(src, src->xsize, src->ysize, outchan);
    if (src->bits == i_8_bits) {
      i_color *vals;

      /* we can always allocate a single scanline of i_color */
      vals = mymalloc(sizeof(i_color) * src->xsize); /* checked 04Jul05 tonyc */
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
    }
    else {
      i_fcolor *vals;

      /* we can always allocate a single scanline of i_fcolor 
         for a >8 image */
      vals = mymalloc(sizeof(i_fcolor) * src->xsize); /* checked 4Jul05 tonyc */
      for (y = 0; y < src->ysize; ++y) {
        i_glinf(src, 0, src->xsize, y, vals);
        for (x = 0; x < src->xsize; ++x) {
          for (j = 0; j < outchan; ++j) {
            work[j] = 0;
            for (i = 0; i < ilimit; ++i) {
              work[j] += coeff[i+inchan*j] * vals[x].channel[i];
            }
            if (i < inchan) {
              work[j] += coeff[i+inchan*j];
            }
          }
          for (j = 0; j < outchan; ++j) {
            if (work[j] < 0)
              vals[x].channel[j] = 0;
            else if (work[j] >= 1)
              vals[x].channel[j] = 1;
            else
              vals[x].channel[j] = work[j];
          }
        }
        i_plinf(im, 0, src->xsize, y, vals);
      }
      myfree(vals);
    }
  }
  else {
    int count;
    int outcount;
    int index;
    i_color *colors;
    i_palidx *vals;

    im = i_img_pal_new(src->xsize, src->ysize, outchan, 
		       i_maxcolors(src));

    /* just translate the color table */
    count = i_colorcount(src);
    outcount = i_colorcount(im);
    /* color table allocated for image, so it must fit */
    colors = mymalloc(count * sizeof(i_color)); /* check 04Jul05 tonyc */
    i_getcolors(src, 0, colors, count);
    for (index = 0; index < count; ++index) {
      for (j = 0; j < outchan; ++j) {
        work[j] = 0;
        for (i = 0; i < ilimit; ++i) {
          work[j] += coeff[i+inchan*j] * colors[index].channel[i];
        }
        if (i < inchan) {
          work[j] += coeff[i+inchan*j] * 255.9;
        }
      }
      for (j = 0; j < outchan; ++j) {
        if (work[j] < 0)
          colors[index].channel[j] = 0;
        else if (work[j] >= 255)
          colors[index].channel[j] = 255;
        else
          colors[index].channel[j] = work[j];
      }
    }
    if (count < outcount) {
      i_setcolors(im, 0, colors, count);
    }
    else {
      i_setcolors(im, 0, colors, outcount);
      i_addcolors(im, colors, count-outcount);
    }
    /* and copy the indicies */
    /* i_palidx is always unsigned char and will never be bigger than short
       and since a line of 4-byte i_colors can fit then a line of i_palidx
       will fit */
    vals = mymalloc(sizeof(i_palidx) * im->xsize); /* checked 4jul05 tonyc */
    for (y = 0; y < im->ysize; ++y) {
      i_gpal(src, 0, im->xsize, y, vals);
      i_ppal(im, 0, im->xsize, y, vals);
    }
    myfree(vals);
    myfree(colors);
  }

  return im;
}

/*
=back

=head1 SEE ALSO

Imager(3)

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=cut
*/
