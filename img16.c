/*
=head1 NAME

img16.c - implements 16-bit images

=head1 SYNOPSIS

  i_img *im = i_img_16_new(int x, int y, int channels);
  # use like a normal image

=head1 DESCRIPTION

Implements 16-bit/sample images.

This basic implementation is required so that we have some larger 
sample image type to work with.

=over

=cut
*/

#include "image.h"
#include "imagei.h"

static int i_ppix_d16(i_img *im, int x, int y, i_color *val);
static int i_gpix_d16(i_img *im, int x, int y, i_color *val);
static int i_glin_d16(i_img *im, int l, int r, int y, i_color *vals);
static int i_plin_d16(i_img *im, int l, int r, int y, i_color *vals);
static int i_ppixf_d16(i_img *im, int x, int y, i_fcolor *val);
static int i_gpixf_d16(i_img *im, int x, int y, i_fcolor *val);
static int i_glinf_d16(i_img *im, int l, int r, int y, i_fcolor *vals);
static int i_plinf_d16(i_img *im, int l, int r, int y, i_fcolor *vals);
static int i_gsamp_d16(i_img *im, int l, int r, int y, i_sample_t *samps, 
                       int *chans, int chan_count);
static int i_gsampf_d16(i_img *im, int l, int r, int y, i_fsample_t *samps, 
                        int *chans, int chan_count);

/*
=item IIM_base_16bit_direct

Base structure used to initialize a 16-bit/sample image.

Internal.

=cut
*/
static i_img IIM_base_16bit_direct =
{
  0, /* channels set */
  0, 0, 0, /* xsize, ysize, bytes */
  ~0U, /* ch_mask */
  i_16_bits, /* bits */
  i_direct_type, /* type */
  0, /* virtual */
  NULL, /* idata */
  { 0, 0, NULL }, /* tags */
  NULL, /* ext_data */

  i_ppix_d16, /* i_f_ppix */
  i_ppixf_d16, /* i_f_ppixf */
  i_plin_d16, /* i_f_plin */
  i_plinf_d16, /* i_f_plinf */
  i_gpix_d16, /* i_f_gpix */
  i_gpixf_d16, /* i_f_gpixf */
  i_glin_d16, /* i_f_glin */
  i_glinf_d16, /* i_f_glinf */
  i_gsamp_d16, /* i_f_gsamp */
  i_gsampf_d16, /* i_f_gsampf */

  NULL, /* i_f_gpal */
  NULL, /* i_f_ppal */
  NULL, /* i_f_addcolor */
  NULL, /* i_f_getcolor */
  NULL, /* i_f_colorcount */
  NULL, /* i_f_findcolor */

  NULL, /* i_f_destroy */
};

/* it's possible some platforms won't have a 16-bit integer type,
   so we check for one otherwise we work by bytes directly

   We do assume 8-bit char
*/
#if __STDC_VERSION__ >= 199901L
/* C99 should define something useful */
#include <stdint.h>
#ifdef UINT16_MAX
typedef uint16_t i_sample16_t;
#define GOT16
#endif
#endif

/* check out unsigned short */
#ifndef GOT16
#include <limits.h>
#if USHRT_MAX == 65535
typedef unsigned short i_sample16_t;
#define GOT16
#endif
#endif

#ifdef GOT16

/* we have a real 16-bit unsigned integer */
#define STORE16(bytes, offset, word) \
   (((i_sample16_t *)(bytes))[offset] = (word))
#define STORE8as16(bytes, offset, byte) \
   (((i_sample16_t *)(bytes))[offset] = (byte) * 256)
#define GET16(bytes, offset) \
     (((i_sample16_t *)(bytes))[offset])
#define GET16as8(bytes, offset) \
     (((i_sample16_t *)(bytes))[offset] / 256)

#else

/* we have to do this the hard way */
#define STORE16(bytes, offset, word) \
   ((((unsigned char *)(bytes))[(offset)*2] = (word) >> 8), \
    (((unsigned char *)(bytes))[(offset)*2+1] = (word) & 0xFF))
#define STORE8as16(bytes, offset, byte) \
   ((((unsigned char *)(bytes))[(offset)*2] = (byte)), \
    (((unsigned char *)(bytes))[(offset)*2+1] = 0))
   
#define GET16(bytes, offset) \
   (((unsigned char *)(bytes))[(offset)*2] * 256 \
    + ((unsigned char *)(bytes))[(offset)*2+1])
#define GET16as8(bytes, offset) \
   (((unsigned char *)(bytes))[(offset)*2] << 8)

#endif

/*
=item i_img_16_new(int x, int y, int ch)

Creates a new 16-bit per sample image.

=cut
*/
i_img *i_img_16_new_low(i_img *im, int x, int y, int ch) {
  mm_log((1,"i_img_16_new(x %d, y %d, ch %d)\n", x, y, ch));
  
  *im = IIM_base_16bit_direct;
  i_tags_new(&im->tags);
  im->xsize = x;
  im->ysize = y;
  im->channels = ch;
  im->bytes = x * y * ch * 2;
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

i_img *i_img_16_new(int x, int y, int ch) {
  i_img *im;

  im = mymalloc(sizeof(i_img));
  if (im) {
    if (!i_img_16_new_low(im, x, y, ch)) {
      myfree(im);
      im = NULL;
    }
  }
  
  mm_log((1, "(%p) <- i_img_16_new\n", im));
  
  return im;
}

static int i_ppix_d16(i_img *im, int x, int y, i_color *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y > im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  for (ch = 0; ch < im->channels; ++ch)
    STORE8as16(im->idata, off+ch, val->channel[ch]);

  return 0;
}

static int i_gpix_d16(i_img *im, int x, int y, i_color *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y > im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  for (ch = 0; ch < im->channels; ++ch)
    val->channel[ch] = GET16as8(im->idata, off+ch);

  return 0;
}

static int i_ppixf_d16(i_img *im, int x, int y, i_fcolor *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y > im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  for (ch = 0; ch < im->channels; ++ch)
    STORE16(im->idata, off+ch, SampleFTo16(val->channel[ch]));

  return 0;
}

static int i_gpixf_d16(i_img *im, int x, int y, i_fcolor *val) {
  int off, ch;

  if (x < 0 || x >= im->xsize || y < 0 || y > im->ysize) 
    return -1;

  off = (x + y * im->xsize) * im->channels;
  for (ch = 0; ch < im->channels; ++ch)
    val->channel[ch] = Sample16ToF(GET16(im->idata, off+ch));

  return 0;
}

static int i_glin_d16(i_img *im, int l, int r, int y, i_color *vals) {
  int ch, count, i;
  int off;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
	vals[i].channel[ch] = GET16as8(im->idata, off);
        ++off;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

static int i_plin_d16(i_img *im, int l, int r, int y, i_color *vals) {
  int ch, count, i;
  int off;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
        STORE8as16(im->idata, off, vals[i].channel[ch]);
        ++off;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

static int i_glinf_d16(i_img *im, int l, int r, int y, i_fcolor *vals) {
  int ch, count, i;
  int off;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
	vals[i].channel[ch] = Sample16ToF(GET16(im->idata, off));
        ++off;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

static int i_plinf_d16(i_img *im, int l, int r, int y, i_fcolor *vals) {
  int ch, count, i;
  int off;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    off = (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
        STORE16(im->idata, off, SampleFTo16(vals[i].channel[ch]));
        ++off;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

static int i_gsamp_d16(i_img *im, int l, int r, int y, i_sample_t *samps, 
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
          *samps++ = GET16as8(im->idata, off+chans[ch]);
          ++count;
        }
        off += im->channels;
      }
    }
    else {
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = GET16as8(im->idata, off+ch);
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

static int i_gsampf_d16(i_img *im, int l, int r, int y, i_fsample_t *samps, 
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
          *samps++ = Sample16ToF(GET16(im->idata, off+chans[ch]));
          ++count;
        }
        off += im->channels;
      }
    }
    else {
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = Sample16ToF(GET16(im->idata, off+ch));
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
