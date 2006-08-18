#include "imager.h"

/*
 * i_scale_mixing() is based on code contained in pnmscale.c, part of
 * the netpbm distribution.  No code was copied from pnmscale but
 * the algorthm was and for this I thank the netpbm crew.
 *
 * Tony
 */

/* pnmscale.c - read a portable anymap and scale it
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
*/


static void
zero_row(i_fcolor *row, int width, int channels);
static void
accum_output_row(i_fcolor *accum, double fraction, i_fcolor const *in,
		 int width, int channels);
static void
horizontal_scale(i_fcolor *out, int out_width, 
		 i_fcolor const *in, int in_width,
		 int channels);

/*
=item i_scale_mixing

Returns a new image scaled to the given size.

Unlike i_scale_axis() this does a simple coverage of pixels from
source to target and doesn't resample.

Adapted from pnmscale.

=cut
*/
i_img *
i_scale_mixing(i_img *src, int x_out, int y_out) {
  i_img *result;
  i_fcolor *in_row = NULL;
  i_fcolor *xscale_row = NULL;
  i_fcolor *accum_row = NULL;
  int y;
  int in_row_bytes, out_row_bytes;
  double rowsleft, fracrowtofill;
  int rowsread;
  double y_scale;

  mm_log((1, "i_scale_mixing(src %p, x_out %d, y_out %d)\n", 
	  src, x_out, y_out));

  i_clear_error();

  if (x_out <= 0) {
    i_push_errorf(0, "output width %d invalid", x_out);
    return NULL;
  }
  if (y_out <= 0) {
    i_push_errorf(0, "output height %d invalid", y_out);
    return NULL;
  }

  in_row_bytes = sizeof(i_fcolor) * src->xsize;
  if (in_row_bytes / sizeof(i_fcolor) != src->xsize) {
    i_push_error(0, "integer overflow allocating input row buffer");
    return NULL;
  }
  out_row_bytes = sizeof(i_fcolor) * x_out;
  if (out_row_bytes / sizeof(i_fcolor) != x_out) {
    i_push_error(0, "integer overflow allocating output row buffer");
    return NULL;
  }

  if (x_out == src->xsize && y_out == src->ysize) {
    return i_copy(src);
  }

  y_scale = y_out / (double)src->ysize;

  result = i_sametype_chans(src, x_out, y_out, src->channels);
  if (!result)
    return NULL;

  in_row     = mymalloc(in_row_bytes);
  accum_row  = mymalloc(in_row_bytes);
  xscale_row = mymalloc(out_row_bytes);

  rowsread = 0;
  rowsleft = 0.0;
  for (y = 0; y < y_out; ++y) {
    if (y_out == src->ysize) {
      i_glinf(src, 0, src->xsize, y, accum_row);
    }
    else {
      fracrowtofill = 1.0;
      zero_row(accum_row, src->xsize, src->channels);
      while (fracrowtofill > 0) {
	if (rowsleft <= 0) {
	  if (rowsread < src->ysize) {
	    i_glinf(src, 0, src->xsize, rowsread, in_row);
	    ++rowsread;
	  }
	  /* else just use the last row read */

	  rowsleft = y_scale;
	}
	if (rowsleft < fracrowtofill) {
	  accum_output_row(accum_row, rowsleft, in_row, src->xsize, 
			   src->channels);
	  fracrowtofill -= rowsleft;
	  rowsleft = 0;
	}
	else {
	  accum_output_row(accum_row, fracrowtofill, in_row, src->xsize, 
			   src->channels);
	  rowsleft -= fracrowtofill;
	  fracrowtofill = 0;
	}
      }
      /* we've accumulated a vertically scaled row */
      if (x_out == src->xsize) {
	/* no need to scale */
	i_plinf(result, 0, x_out, y, accum_row);
      }
      else {
	horizontal_scale(xscale_row, x_out, accum_row, src->xsize, 
			 src->channels);
	i_plinf(result, 0, x_out, y, xscale_row);
      }
    }
  }

  myfree(in_row);
  myfree(accum_row);
  myfree(xscale_row);

  return result;
}

static void
zero_row(i_fcolor *row, int width, int channels) {
  int x;
  int ch;

  /* with IEEE floats we could just use memset() but that's not
     safe in general under ANSI C */
  for (x = 0; x < width; ++x) {
    for (ch = 0; ch < channels; ++ch)
      row[x].channel[ch] = 0.0;
  }
}

static void
accum_output_row(i_fcolor *accum, double fraction, i_fcolor const *in,
		 int width, int channels) {
  int x, ch;

  for (x = 0; x < width; ++x) {
    for (ch = 0; ch < channels; ++ch) {
      accum[x].channel[ch] += in[x].channel[ch] * fraction;
    }
  }
}

static void
horizontal_scale(i_fcolor *out, int out_width, 
		 i_fcolor const *in, int in_width,
		 int channels) {
  double frac_col_to_fill, frac_col_left;
  int in_x;
  int out_x;
  double x_scale = (double)out_width / in_width;
  int ch;
  double accum[MAXCHANNELS] = { 0 };
  
  frac_col_to_fill = 1.0;
  out_x = 0;
  for (in_x = 0; in_x < in_width; ++in_x) {
    frac_col_left = x_scale;
    while (frac_col_left >= frac_col_to_fill) {
      for (ch = 0; ch < channels; ++ch)
	accum[ch] += frac_col_to_fill * in[in_x].channel[ch];

      for (ch = 0; ch < channels; ++ch) {
	out[out_x].channel[ch] = accum[ch];
	accum[ch] = 0;
      }
      frac_col_left -= frac_col_to_fill;
      frac_col_to_fill = 1.0;
      ++out_x;
    }

    if (frac_col_left > 0) {
      for (ch = 0; ch < channels; ++ch) {
	accum[ch] += frac_col_left * in[in_x].channel[ch];
      }
      frac_col_to_fill -= frac_col_left;
    }
  }

  if (out_x < out_width-1 || out_x > out_width) {
    i_fatal(3, "Internal error: out_x %d out of range (width %d)", out_x, out_width);
  }
  
  if (out_x < out_width) {
    for (ch = 0; ch < channels; ++ch) {
      accum[ch] += frac_col_to_fill * in[in_width-1].channel[ch];
      out[out_x].channel[ch] = accum[ch];
    }
  }
}
