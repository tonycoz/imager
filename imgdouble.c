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

#include "imager.h"
#include "imageri.h"

static int i_ppix_ddoub(i_img *im, int x, int y, const i_color *val);
static int i_gpix_ddoub(i_img *im, int x, int y, i_color *val);
static int i_glin_ddoub(i_img *im, int l, int r, int y, i_color *vals);
static int i_plin_ddoub(i_img *im, int l, int r, int y, const i_color *vals);
static int i_ppixf_ddoub(i_img *im, int x, int y, const i_fcolor *val);
static int i_gpixf_ddoub(i_img *im, int x, int y, i_fcolor *val);
static int i_glinf_ddoub(i_img *im, int l, int r, int y, i_fcolor *vals);
static int i_plinf_ddoub(i_img *im, int l, int r, int y, const i_fcolor *vals);
static int i_gsamp_ddoub(i_img *im, int l, int r, int y, i_sample_t *samps, 
                       int const *chans, int chan_count);
static int i_gsampf_ddoub(i_img *im, int l, int r, int y, i_fsample_t *samps, 
                        int const *chans, int chan_count);

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
  NULL, /* i_f_addcolors */
  NULL, /* i_f_getcolors */
  NULL, /* i_f_colorcount */
  NULL, /* i_f_maxcolors */
  NULL, /* i_f_findcolor */
  NULL, /* i_f_setcolors */

  NULL, /* i_f_destroy */

  i_gsamp_bits_fb,
  NULL, /* i_f_psamp_bits */
};

/*
=item i_img_double_new(int x, int y, int ch)
=category Image creation/destruction
=synopsis i_img *img = i_img_double_new(width, height, channels);

Creates a new double per sample image.

=cut
*/
i_img *i_img_double_new(int x, int y, int ch) {
  int bytes;
  i_img *im;

  mm_log((1,"i_img_double_new(x %d, y %d, ch %d)\n", x, y, ch));

  if (x < 1 || y < 1) {
    i_push_error(0, "Image sizes must be positive");
    return NULL;
  }
  if (ch < 1 || ch > MAXCHANNELS) {
    i_push_errorf(0, "channels must be between 1 and %d", MAXCHANNELS);
    return NULL;
  }
  bytes = x * y * ch * sizeof(double);
  if (bytes / y / ch / sizeof(double) != x) {
    i_push_errorf(0, "integer overflow calculating image allocation");
    return NULL;
  }
  
  im = i_img_alloc();
  *im = IIM_base_double_direct;
  i_tags_new(&im->tags);
  im->xsize = x;
  im->ysize = y;
  im->channels = ch;
  im->bytes = bytes;
  im->ext_data = NULL;
  im->idata = mymalloc(im->bytes);
  memset(im->idata, 0, im->bytes);
  i_img_init(im);
  
  return im;
}

static int i_ppix_ddoub(i_img *im, int x, int y, const i_color *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  if (I_ALL_CHANNELS_WRITABLE(im)) {
    for (ch = 0; ch < im->channels; ++ch)
      ((double*)im->idata)[off+ch] = Sample8ToF(val->channel[ch]);
  }
  else {
    for (ch = 0; ch < im->channels; ++ch)
      if (im->ch_mask & (1<<ch))
	((double*)im->idata)[off+ch] = Sample8ToF(val->channel[ch]);
  }

  return 0;
}

static int i_gpix_ddoub(i_img *im, int x, int y, i_color *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  for (ch = 0; ch < im->channels; ++ch)
    val->channel[ch] = SampleFTo8(((double *)im->idata)[off+ch]);

  return 0;
}

static int i_ppixf_ddoub(i_img *im, int x, int y, const i_fcolor *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  if (I_ALL_CHANNELS_WRITABLE(im)) {
    for (ch = 0; ch < im->channels; ++ch)
      ((double *)im->idata)[off+ch] = val->channel[ch];
  }
  else {
    for (ch = 0; ch < im->channels; ++ch)
      if (im->ch_mask & (1 << ch))
	((double *)im->idata)[off+ch] = val->channel[ch];
  }

  return 0;
}

static int i_gpixf_ddoub(i_img *im, int x, int y, i_fcolor *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize) 
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

static int i_plin_ddoub(i_img *im, int l, int r, int y, const i_color *vals) {
  int ch, count, i;
  int off;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    count = r - l;
    if (I_ALL_CHANNELS_WRITABLE(im)) {
      for (i = 0; i < count; ++i) {
	for (ch = 0; ch < im->channels; ++ch) {
	  ((double *)im->idata)[off] = Sample8ToF(vals[i].channel[ch]);
	  ++off;
	}
      }
    }
    else {
      for (i = 0; i < count; ++i) {
	for (ch = 0; ch < im->channels; ++ch) {
	  if (im->ch_mask & (1 << ch))
	    ((double *)im->idata)[off] = Sample8ToF(vals[i].channel[ch]);
	  ++off;
	}
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

static int i_plinf_ddoub(i_img *im, int l, int r, int y, const i_fcolor *vals) {
  int ch, count, i;
  int off;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    count = r - l;
    if (I_ALL_CHANNELS_WRITABLE(im)) {
      for (i = 0; i < count; ++i) {
	for (ch = 0; ch < im->channels; ++ch) {
	  ((double *)im->idata)[off] = vals[i].channel[ch];
	  ++off;
	}
      }
    }
    else {
      for (i = 0; i < count; ++i) {
	for (ch = 0; ch < im->channels; ++ch) {
	  if (im->ch_mask & (1 << ch))
	    ((double *)im->idata)[off] = vals[i].channel[ch];
	  ++off;
	}
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

static int i_gsamp_ddoub(i_img *im, int l, int r, int y, i_sample_t *samps, 
                       int const *chans, int chan_count) {
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
      if (chan_count <= 0 || chan_count > im->channels) {
	i_push_errorf(0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return 0;
      }
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
                        int const *chans, int chan_count) {
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
      if (chan_count <= 0 || chan_count > im->channels) {
	i_push_errorf(0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return 0;
      }
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
