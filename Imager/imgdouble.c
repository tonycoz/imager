/*
=head1 NAME

imgdouble.c - implements double per sample images

=head1 SYNOPSIS

  i_img *im = i_img_double_new(int x, int y, int channels);
  # use like a normal image

=head1 DESCRIPTION

Implements double/sample images.

This basic implementation is required so that we have some larger 
sample image type to work with.

=over

=cut
*/

#include "image.h"
#include "imagei.h"

static int i_ppix_ddoub(i_img *im, int x, int y, i_color *val);
static int i_gpix_ddoub(i_img *im, int x, int y, i_color *val);
static int i_glin_ddoub(i_img *im, int l, int r, int y, i_color *vals);
static int i_plin_ddoub(i_img *im, int l, int r, int y, i_color *vals);
static int i_ppixf_ddoub(i_img *im, int x, int y, i_fcolor *val);
static int i_gpixf_ddoub(i_img *im, int x, int y, i_fcolor *val);
static int i_glinf_ddoub(i_img *im, int l, int r, int y, i_fcolor *vals);
static int i_plinf_ddoub(i_img *im, int l, int r, int y, i_fcolor *vals);
static int i_gsamp_ddoub(i_img *im, int l, int r, int y, i_sample_t *samps, 
                       int *chans, int chan_count);
static int i_gsampf_ddoub(i_img *im, int l, int r, int y, i_fsample_t *samps, 
                        int *chans, int chan_count);

/*
=item IIM_base_16bit_direct

Base structure used to initialize a 16-bit/sample image.

Internal.

=cut
*/
static i_img IIM_base_double_direct =
{
  0, /* channels set */
  0, 0, 0, /* xsize, ysize, bytes */
  ~0U, /* ch_mask */
  i_double_bits, /* bits */
  i_direct_type, /* type */
  0, /* virtual */
  NULL, /* idata */
  { 0, 0, NULL }, /* tags */
  NULL, /* ext_data */

  i_ppix_ddoub, /* i_f_ppix */
  i_ppixf_ddoub, /* i_f_ppixf */
  i_plin_ddoub, /* i_f_plin */
  i_plinf_ddoub, /* i_f_plinf */
  i_gpix_ddoub, /* i_f_gpix */
  i_gpixf_ddoub, /* i_f_gpixf */
  i_glin_ddoub, /* i_f_glin */
  i_glinf_ddoub, /* i_f_glinf */
  i_gsamp_ddoub, /* i_f_gsamp */
  i_gsampf_ddoub, /* i_f_gsampf */

  NULL, /* i_f_gpal */
  NULL, /* i_f_ppal */
  NULL, /* i_f_addcolor */
  NULL, /* i_f_getcolor */
  NULL, /* i_f_colorcount */
  NULL, /* i_f_findcolor */

  NULL, /* i_f_destroy */
};

/*
=item i_img_double_new(int x, int y, int ch)

Creates a new double per sample image.

=cut
*/
i_img *i_img_double_new_low(i_img *im, int x, int y, int ch) {
  mm_log((1,"i_img_double_new(x %d, y %d, ch %d)\n", x, y, ch));
  
  *im = IIM_base_double_direct;
  i_tags_new(&im->tags);
  im->xsize = x;
  im->ysize = y;
  im->channels = ch;
  im->bytes = x * y * ch * sizeof(double);
  im->ext_data = NULL;
  im->idata = mymalloc(im->bytes);
  if (im->idata) {
    memset(im->idata, 0, im->bytes);
  }
  else {
    i_tags_destroy(&im->tags);
    im = NULL;
  }
  
  return im;
}

i_img *i_img_double_new(int x, int y, int ch) {
  i_img *im;

  im = mymalloc(sizeof(i_img));
  if (im) {
    if (!i_img_double_new_low(im, x, y, ch)) {
      myfree(im);
      im = NULL;
    }
  }
  
  mm_log((1, "(%p) <- i_img_double_new\n", im));
  
  return im;
}

static int i_ppix_ddoub(i_img *im, int x, int y, i_color *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y > im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  for (ch = 0; ch < im->channels; ++ch)
    ((double*)im->idata)[off+ch] = Sample8ToF(val->channel[ch]);

  return 0;
}

static int i_gpix_ddoub(i_img *im, int x, int y, i_color *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y > im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  for (ch = 0; ch < im->channels; ++ch)
    val->channel[ch] = SampleFTo8(((double *)im->idata)[off+ch]);

  return 0;
}

static int i_ppixf_ddoub(i_img *im, int x, int y, i_fcolor *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y > im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  for (ch = 0; ch < im->channels; ++ch)
    ((double *)im->idata)[off+ch] = val->channel[ch];;

  return 0;
}

static int i_gpixf_ddoub(i_img *im, int x, int y, i_fcolor *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y > im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  for (ch = 0; ch < im->channels; ++ch)
    val->channel[ch] = ((double *)im->idata)[off+ch];

  return 0;
}

static int i_glin_ddoub(i_img *im, int l, int r, int y, i_color *vals) {
  int ch, count, i;
  int off;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
	vals[i].channel[ch] = SampleFTo8(((double *)im->idata)[off]);
        ++off;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

static int i_plin_ddoub(i_img *im, int l, int r, int y, i_color *vals) {
  int ch, count, i;
  int off;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
        ((double *)im->idata)[off] = Sample8ToF(vals[i].channel[ch]);
        ++off;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

static int i_glinf_ddoub(i_img *im, int l, int r, int y, i_fcolor *vals) {
  int ch, count, i;
  int off;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
	vals[i].channel[ch] = ((double *)im->idata)[off];
        ++off;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

static int i_plinf_ddoub(i_img *im, int l, int r, int y, i_fcolor *vals) {
  int ch, count, i;
  int off;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
        ((double *)im->idata)[off] = vals[i].channel[ch];
        ++off;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

static int i_gsamp_ddoub(i_img *im, int l, int r, int y, i_sample_t *samps, 
                       int *chans, int chan_count) {
  int ch, count, i, w;
  int off;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
          i_push_errorf(0, "No channel %d in this image", chans[ch]);
          return 0;
        }
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = SampleFTo8(((double *)im->idata)[off+chans[ch]]);
          ++count;
        }
        off += im->channels;
      }
    }
    else {
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = SampleFTo8(((double *)im->idata)[off+ch]);
          ++count;
        }
        off += im->channels;
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

static int i_gsampf_ddoub(i_img *im, int l, int r, int y, i_fsample_t *samps, 
                        int *chans, int chan_count) {
  int ch, count, i, w;
  int off;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
          i_push_errorf(0, "No channel %d in this image", chans[ch]);
          return 0;
        }
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = ((double *)im->idata)[off+chans[ch]];
          ++count;
        }
        off += im->channels;
      }
    }
    else {
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = ((double *)im->idata)[off+ch];
          ++count;
        }
        off += im->channels;
      }
    }

    return count;
  }
  else {
    return 0;
  }
}


/*
=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
*/
