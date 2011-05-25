#include "imager.h"
#include "imageri.h"

static int i_ppix_d(i_img *im, int x, int y, const i_color *val);
static int i_gpix_d(i_img *im, int x, int y, i_color *val);
static int i_glin_d(i_img *im, int l, int r, int y, i_color *vals);
static int i_plin_d(i_img *im, int l, int r, int y, const i_color *vals);
static int i_ppixf_d(i_img *im, int x, int y, const i_fcolor *val);
static int i_gpixf_d(i_img *im, int x, int y, i_fcolor *val);
static int i_glinf_d(i_img *im, int l, int r, int y, i_fcolor *vals);
static int i_plinf_d(i_img *im, int l, int r, int y, const i_fcolor *vals);
static int i_gsamp_d(i_img *im, int l, int r, int y, i_sample_t *samps, const int *chans, int chan_count);
static int i_gsampf_d(i_img *im, int l, int r, int y, i_fsample_t *samps, const int *chans, int chan_count);

/*
=item IIM_base_8bit_direct (static)

A static i_img object used to initialize direct 8-bit per sample images.

=cut
*/
static i_img IIM_base_8bit_direct =
{
  0, /* channels set */
  0, 0, 0, /* xsize, ysize, bytes */
  ~0U, /* ch_mask */
  i_8_bits, /* bits */
  i_direct_type, /* type */
  0, /* virtual */
  NULL, /* idata */
  { 0, 0, NULL }, /* tags */
  NULL, /* ext_data */

  i_ppix_d, /* i_f_ppix */
  i_ppixf_d, /* i_f_ppixf */
  i_plin_d, /* i_f_plin */
  i_plinf_d, /* i_f_plinf */
  i_gpix_d, /* i_f_gpix */
  i_gpixf_d, /* i_f_gpixf */
  i_glin_d, /* i_f_glin */
  i_glinf_d, /* i_f_glinf */
  i_gsamp_d, /* i_f_gsamp */
  i_gsampf_d, /* i_f_gsampf */

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

/*static void set_8bit_direct(i_img *im) {
  im->i_f_ppix = i_ppix_d;
  im->i_f_ppixf = i_ppixf_d;
  im->i_f_plin = i_plin_d;
  im->i_f_plinf = i_plinf_d;
  im->i_f_gpix = i_gpix_d;
  im->i_f_gpixf = i_gpixf_d;
  im->i_f_glin = i_glin_d;
  im->i_f_glinf = i_glinf_d;
  im->i_f_gpal = NULL;
  im->i_f_ppal = NULL;
  im->i_f_addcolor = NULL;
  im->i_f_getcolor = NULL;
  im->i_f_colorcount = NULL;
  im->i_f_findcolor = NULL;
  }*/

/*
=item IIM_new(x, y, ch)

=item i_img_8_new(x, y, ch)

=category Image creation/destruction

=synopsis i_img *img = i_img_8_new(width, height, channels);

Creates a new image object I<x> pixels wide, and I<y> pixels high with
I<ch> channels.

=cut
*/


i_img *
IIM_new(int x,int y,int ch) {
  i_img *im;
  mm_log((1,"IIM_new(x %d,y %d,ch %d)\n",x,y,ch));

  im=i_img_empty_ch(NULL,x,y,ch);
  
  mm_log((1,"(%p) <- IIM_new\n",im));
  return im;
}


void
IIM_DESTROY(i_img *im) {
  mm_log((1,"IIM_DESTROY(im* %p)\n",im));
  i_img_destroy(im);
  /*   myfree(cl); */
}

/* 
=item i_img_new()

Create new image reference - notice that this isn't an object yet and
this should be fixed asap.

=cut
*/


i_img *
i_img_new() {
  i_img *im;
  
  mm_log((1,"i_img_struct()\n"));

  im = i_img_alloc();
  
  *im = IIM_base_8bit_direct;
  im->xsize=0;
  im->ysize=0;
  im->channels=3;
  im->ch_mask=MAXINT;
  im->bytes=0;
  im->idata=NULL;

  i_img_init(im);
  
  mm_log((1,"(%p) <- i_img_struct\n",im));
  return im;
}

/* 
=item i_img_empty(im, x, y)

Re-new image reference (assumes 3 channels)

   im - Image pointer
   x - xsize of destination image
   y - ysize of destination image

**FIXME** what happens if a live image is passed in here?

Should this just call i_img_empty_ch()?

=cut
*/

i_img *
i_img_empty(i_img *im,int x,int y) {
  mm_log((1,"i_img_empty(*im %p, x %d, y %d)\n",im, x, y));
  return i_img_empty_ch(im, x, y, 3);
}

/* 
=item i_img_empty_ch(im, x, y, ch)

Re-new image reference 

   im - Image pointer
   x  - xsize of destination image
   y  - ysize of destination image
   ch - number of channels

=cut
*/

i_img *
i_img_empty_ch(i_img *im,int x,int y,int ch) {
  int bytes;

  mm_log((1,"i_img_empty_ch(*im %p, x %d, y %d, ch %d)\n", im, x, y, ch));

  if (x < 1 || y < 1) {
    i_push_error(0, "Image sizes must be positive");
    return NULL;
  }
  if (ch < 1 || ch > MAXCHANNELS) {
    i_push_errorf(0, "channels must be between 1 and %d", MAXCHANNELS);
    return NULL;
  }
  /* check this multiplication doesn't overflow */
  bytes = x*y*ch;
  if (bytes / y / ch != x) {
    i_push_errorf(0, "integer overflow calculating image allocation");
    return NULL;
  }

  if (im == NULL)
    im = i_img_alloc();

  memcpy(im, &IIM_base_8bit_direct, sizeof(i_img));
  i_tags_new(&im->tags);
  im->xsize    = x;
  im->ysize    = y;
  im->channels = ch;
  im->ch_mask  = MAXINT;
  im->bytes=bytes;
  if ( (im->idata=mymalloc(im->bytes)) == NULL) 
    i_fatal(2,"malloc() error\n"); 
  memset(im->idata,0,(size_t)im->bytes);
  
  im->ext_data = NULL;

  i_img_init(im);
  
  mm_log((1,"(%p) <- i_img_empty_ch\n",im));
  return im;
}

/*
=head2 8-bit per sample image internal functions

These are the functions installed in an 8-bit per sample image.

=over

=item i_ppix_d(im, x, y, col)

Internal function.

This is the function kept in the i_f_ppix member of an i_img object.
It does a normal store of a pixel into the image with range checking.

Returns 0 if the pixel could be set, -1 otherwise.

=cut
*/
static
int
i_ppix_d(i_img *im, int x, int y, const i_color *val) {
  int ch;
  
  if ( x>-1 && x<im->xsize && y>-1 && y<im->ysize ) {
    for(ch=0;ch<im->channels;ch++)
      if (im->ch_mask&(1<<ch)) 
	im->idata[(x+y*im->xsize)*im->channels+ch]=val->channel[ch];
    return 0;
  }
  return -1; /* error was clipped */
}

/*
=item i_gpix_d(im, x, y, &col)

Internal function.

This is the function kept in the i_f_gpix member of an i_img object.
It does normal retrieval of a pixel from the image with range checking.

Returns 0 if the pixel could be set, -1 otherwise.

=cut
*/
static
int 
i_gpix_d(i_img *im, int x, int y, i_color *val) {
  int ch;
  if (x>-1 && x<im->xsize && y>-1 && y<im->ysize) {
    for(ch=0;ch<im->channels;ch++) 
      val->channel[ch]=im->idata[(x+y*im->xsize)*im->channels+ch];
    return 0;
  }
  for(ch=0;ch<im->channels;ch++) val->channel[ch] = 0;
  return -1; /* error was cliped */
}

/*
=item i_glin_d(im, l, r, y, vals)

Reads a line of data from the image, storing the pixels at vals.

The line runs from (l,y) inclusive to (r,y) non-inclusive

vals should point at space for (r-l) pixels.

l should never be less than zero (to avoid confusion about where to
put the pixels in vals).

Returns the number of pixels copied (eg. if r, l or y is out of range)

=cut
*/
static
int
i_glin_d(i_img *im, int l, int r, int y, i_color *vals) {
  int ch, count, i;
  unsigned char *data;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch)
	vals[i].channel[ch] = *data++;
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_plin_d(im, l, r, y, vals)

Writes a line of data into the image, using the pixels at vals.

The line runs from (l,y) inclusive to (r,y) non-inclusive

vals should point at (r-l) pixels.

l should never be less than zero (to avoid confusion about where to
get the pixels in vals).

Returns the number of pixels copied (eg. if r, l or y is out of range)

=cut
*/
static
int
i_plin_d(i_img *im, int l, int r, int y, const i_color *vals) {
  int ch, count, i;
  unsigned char *data;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
	if (im->ch_mask & (1 << ch)) 
	  *data = vals[i].channel[ch];
	++data;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_ppixf_d(im, x, y, val)

=cut
*/
static
int
i_ppixf_d(i_img *im, int x, int y, const i_fcolor *val) {
  int ch;
  
  if ( x>-1 && x<im->xsize && y>-1 && y<im->ysize ) {
    for(ch=0;ch<im->channels;ch++)
      if (im->ch_mask&(1<<ch)) {
	im->idata[(x+y*im->xsize)*im->channels+ch] = 
          SampleFTo8(val->channel[ch]);
      }
    return 0;
  }
  return -1; /* error was clipped */
}

/*
=item i_gpixf_d(im, x, y, val)

=cut
*/
static
int
i_gpixf_d(i_img *im, int x, int y, i_fcolor *val) {
  int ch;
  if (x>-1 && x<im->xsize && y>-1 && y<im->ysize) {
    for(ch=0;ch<im->channels;ch++) {
      val->channel[ch] = 
        Sample8ToF(im->idata[(x+y*im->xsize)*im->channels+ch]);
    }
    return 0;
  }
  return -1; /* error was cliped */
}

/*
=item i_glinf_d(im, l, r, y, vals)

Reads a line of data from the image, storing the pixels at vals.

The line runs from (l,y) inclusive to (r,y) non-inclusive

vals should point at space for (r-l) pixels.

l should never be less than zero (to avoid confusion about where to
put the pixels in vals).

Returns the number of pixels copied (eg. if r, l or y is out of range)

=cut
*/
static
int
i_glinf_d(i_img *im, int l, int r, int y, i_fcolor *vals) {
  int ch, count, i;
  unsigned char *data;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch)
	vals[i].channel[ch] = Sample8ToF(*data++);
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_plinf_d(im, l, r, y, vals)

Writes a line of data into the image, using the pixels at vals.

The line runs from (l,y) inclusive to (r,y) non-inclusive

vals should point at (r-l) pixels.

l should never be less than zero (to avoid confusion about where to
get the pixels in vals).

Returns the number of pixels copied (eg. if r, l or y is out of range)

=cut
*/
static
int
i_plinf_d(i_img *im, int l, int r, int y, const i_fcolor *vals) {
  int ch, count, i;
  unsigned char *data;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
    count = r - l;
    for (i = 0; i < count; ++i) {
      for (ch = 0; ch < im->channels; ++ch) {
	if (im->ch_mask & (1 << ch)) 
	  *data = SampleFTo8(vals[i].channel[ch]);
	++data;
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_gsamp_d(i_img *im, int l, int r, int y, i_sample_t *samps, int *chans, int chan_count)

Reads sample values from im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chans, an array of int with chan_count
elements.

Returns the number of samples read (which should be (r-l) * bits_set(chan_mask)

=cut
*/
static
int
i_gsamp_d(i_img *im, int l, int r, int y, i_sample_t *samps, 
              const int *chans, int chan_count) {
  int ch, count, i, w;
  unsigned char *data;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
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
          *samps++ = data[chans[ch]];
          ++count;
        }
        data += im->channels;
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
          *samps++ = data[ch];
          ++count;
        }
        data += im->channels;
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_gsampf_d(i_img *im, int l, int r, int y, i_fsample_t *samps, int *chans, int chan_count)

Reads sample values from im for the horizontal line (l, y) to (r-1,y)
for the channels specified by chan_mask, where bit 0 is the first
channel.

Returns the number of samples read (which should be (r-l) * bits_set(chan_mask)

=cut
*/
static
int
i_gsampf_d(i_img *im, int l, int r, int y, i_fsample_t *samps, 
           const int *chans, int chan_count) {
  int ch, count, i, w;
  unsigned char *data;
  for (ch = 0; ch < chan_count; ++ch) {
    if (chans[ch] < 0 || chans[ch] >= im->channels) {
      i_push_errorf(0, "No channel %d in this image", chans[ch]);
    }
  }
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * im->channels;
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
          *samps++ = Sample8ToF(data[chans[ch]]);
          ++count;
        }
        data += im->channels;
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
          *samps++ = Sample8ToF(data[ch]);
          ++count;
        }
        data += im->channels;
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

Arnar M. Hrafnkelsson <addi@umich.edu>

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

L<Imager>

=cut
*/
