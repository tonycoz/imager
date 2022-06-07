#define IMAGER_NO_CONTEXT
#define IMAGER_WANT_FLOAT16
#include "imager.h"
#include "imageri.h"

/* used to output both gray and gray alpha from a gray alpha image */
static const int
gray_alpha_chans[] = { 0, 1 };

/* used for gray and gray alpha from a gray only image */
static const int
gray_chans[] = { 0, 0 };

static const int
rgb_rgba_chans[] = { 0, 1, 2, 3 };

static const int
rgb_rgb_chans[] = { 0, 1, 2, 0 };

static const int
abgr_rgba_chans[] = { 3, 2, 1, 0 };

static const int
abgr_rgb_chans[] = { 0, 2, 1, 0 };

static const int
rgb_gray_alpha_chans[] = { 0, 0, 0, 1 };

static const int
rgb_gray_chans[] = { 0, 0, 0, 0 };

static const int
abgr_gray_alpha_chans[] = { 1, 0, 0, 0 };

static const int
abgr_gray_chans[] = { 0, 0, 0, 0 };

static const int *
layout_chans(i_img *im, i_data_layout_t layout, int *count, int *need_alpha, int *need_zero) {
  *need_alpha = -1;
  *need_zero = -1;

  switch (layout) {
  case idl_gray:
    if (im->channels > 2)
      return NULL;
    *count = 1;
    return gray_chans;

  case idl_gray_alpha:
    if (im->channels > 2)
      return NULL;
    *count = 2;
    if (im->channels == 1) {
      *need_alpha = 1;
      return gray_chans;
    }
    else {
      return gray_alpha_chans;
    }

  case idl_rgb:
    *count = 3;
    if (im->channels > 2) {
      return rgb_rgb_chans;
    }
    else {
      return rgb_gray_chans;
    }

  case idl_rgb_alpha:
    *count = 4;
    if (im->channels == 1 || im->channels == 3)
      *need_alpha = 3;
    switch (im->channels) {
    case 1:
      return rgb_gray_chans;
    case 2:
      return rgb_gray_alpha_chans;
    case 3:
      return rgb_rgb_chans;
    case 4:
      return rgb_rgba_chans;
    }
    /* NOTREACHED */

  case idl_rgbx:
    *count = 4;
    *need_zero = 3;
    if (im->channels > 2) {
      return rgb_rgb_chans;
    }
    else {
      return rgb_gray_chans;
    }

  case idl_bgr:
    *count = 3;
    if (im->channels > 2) {
      return abgr_rgb_chans + 1;
    }
    else {
      return abgr_gray_chans + 1;
    }

  case idl_abgr:
    *count = 4;
    if (im->channels == 1 || im->channels == 3)
      *need_alpha = 0;
    switch(im->channels) {
    case 1:
      return abgr_gray_chans;
    case 2:
      return abgr_gray_alpha_chans;
    case 3:
      return abgr_rgb_chans;
    case 4:
      return abgr_rgba_chans;
    }
    /* NOTREACHED */

  case idl_palette: /* only the paletted image handler needs to handle this */
  default:
    {
      dIMCTXim(im);
      im_push_errorf(aIMCTX, 3, "Unknown data layout %d", (int)layout);
      return NULL;
    }
  }
}


static i_image_data_alloc_t *
data_8(i_img *im, i_data_layout_t layout, void **pdata, size_t *psize) {
  dIMCTXim(im);
  const int *chans;
  int count;
  int need_alpha;
  int need_zero;
  size_t size;
  i_sample_t *data;
  i_sample_t *datap;
  i_sample_t *pixelp;
  i_img_dim x, y;
  size_t row_size;

  chans = layout_chans(im, layout, &count, &need_alpha, &need_zero);
  if (!chans) {
    return NULL;
  }
  size = (size_t)count * (size_t)im->xsize * (size_t)im->ysize;
  if (size / (size_t)count / (size_t)im->xsize != (size_t)im->ysize) {
    im_push_error(aIMCTX, 0, "integer overflow calculating image data size");
    return NULL;
  }

  /* cannot fail */
  data = mymalloc(size);
  row_size = count * im->xsize;
  for (y = 0, datap = data; y < im->ysize; ++y, datap += row_size) {
    i_gsamp(im, 0, im->xsize, y, datap, chans, count);
    if (need_alpha >= 0) {
      for (x = 0, pixelp = datap; x < im->xsize; ++x, pixelp += count) {
        pixelp[need_alpha] = 255;
      }
    }
    if (need_zero >= 0) {
      for (x = 0, pixelp = datap; x < im->xsize; ++x, pixelp += count) {
        pixelp[need_zero] = 0;
      }
    }
  }

  *pdata = data;
  *psize = size;

  return i_new_image_data_alloc_free(im, data);
}

static void
swap_bytes(void *data, size_t size, size_t item_size) {
  unsigned char *p = data;
  unsigned char *end = p + size;
  int iter_limit = item_size / 2;

  while (p < end) {
    int i, j;

    for (i = 0, j = item_size-1; i < iter_limit; ++i) {
      unsigned char temp = p[i];
      p[i] = p[item_size-i];
      p[item_size-i] = temp;
    }

    p += item_size;
  }
}

static i_image_data_alloc_t *
data_double(i_img *im, i_data_layout_t layout, unsigned flags,
            void **pdata, size_t *psize) {
  dIMCTXim(im);
  const int *chans;
  int count;
  int need_alpha;
  int need_zero;
  size_t size;
  i_fsample_t *data;
  i_fsample_t *datap;
  i_fsample_t *pixelp;
  i_img_dim x, y;
  size_t row_size;

  chans = layout_chans(im, layout, &count, &need_alpha, &need_zero);
  size = (size_t)count * (size_t)im->xsize * (size_t)im->ysize * sizeof(i_fsample_t);
  if (size / (size_t)count / (size_t)im->xsize / sizeof(i_fsample_t) != (size_t)im->ysize) {
    im_push_error(aIMCTX, 0, "integer overflow calculating image data size");
    return NULL;
  }

  /* cannot fail */
  data = mymalloc(size);
  row_size = count * im->xsize;
  for (y = 0, datap = data; y < im->ysize; ++y, datap += row_size) {
    i_gsampf(im, 0, im->xsize, y, datap, chans, count);
    if (need_alpha >= 0) {
      for (x = 0, pixelp = datap; x < im->xsize; ++x, pixelp += count) {
        pixelp[need_alpha] = 1.0;
      }
    }
    if (need_zero >= 0) {
      for (x = 0, pixelp = datap; x < im->xsize; ++x, pixelp += count) {
        pixelp[need_zero] = 0.0;
      }
    }
  }

  if (flags & idf_otherendian) {
    swap_bytes(data, size, sizeof(i_fsample_t));
  }

  *pdata = data;
  *psize = size;

  return i_new_image_data_alloc_free(im, data);
}

static i_image_data_alloc_t *
data_16(i_img *im, i_data_layout_t layout, unsigned flags,
        void **pdata, size_t *psize) {
  dIMCTXim(im);
  const int *chans;
  int count;
  int need_alpha;
  int need_zero;
  size_t size;
  i_sample16_t *data;
  i_sample16_t *datap;
  i_sample16_t *pixelp;
  i_fsample_t *frow, *frowp;
  i_img_dim x, y;
  size_t row_size_samps;
  size_t frow_size_bytes;
  i_img_dim i;

  chans = layout_chans(im, layout, &count, &need_alpha, &need_zero);
  size = (size_t)count * (size_t)im->xsize * (size_t)im->ysize * sizeof(i_sample16_t);
  if (size / (size_t)count / (size_t)im->xsize / sizeof(i_sample16_t) != (size_t)im->ysize) {
    im_push_error(aIMCTX, 0, "integer overflow calculating image data size");
    return NULL;
  }

  /* cannot fail */
  data = mymalloc(size); 
  row_size_samps = count * im->xsize;
  frow_size_bytes = row_size_samps * sizeof(i_fsample_t);
  frow = mymalloc(frow_size_bytes);
  for (y = 0, datap = data; y < im->ysize; ++y, datap += row_size_samps) {
    i_gsampf(im, 0, im->xsize, y, frow, chans, count);
    for (i = 0, frowp = frow, pixelp = datap; i < row_size_samps; ++i, ++pixelp, ++frowp) {
      *pixelp = SampleFTo16(*frowp);
    }
    if (need_alpha >= 0) {
      for (x = 0, pixelp = datap; x < im->xsize; ++x, pixelp += count) {
        pixelp[need_alpha] = 0xFFFF;
      }
    }
    if (need_zero >= 0) {
      for (x = 0, pixelp = datap; x < im->xsize; ++x, pixelp += count) {
        pixelp[need_zero] = 0x0;
      }
    }
  }

  myfree(frow);

  if (flags & idf_otherendian) {
    swap_bytes(data, size, sizeof(i_sample16_t));
  }

  *pdata = data;
  *psize = size;

  return i_new_image_data_alloc_free(im, data);
}

static i_image_data_alloc_t *
data_float(i_img *im, i_data_layout_t layout, unsigned flags,
        void **pdata, size_t *psize) {
  dIMCTXim(im);
  const int *chans;
  int count;
  int need_alpha;
  int need_zero;
  size_t size;
  float *data;
  float *datap;
  float *pixelp;
  i_fsample_t *frow, *frowp;
  i_img_dim x, y;
  size_t row_size_samps;
  size_t frow_size_bytes;
  i_img_dim i;

  chans = layout_chans(im, layout, &count, &need_alpha, &need_zero);
  size = (size_t)count * (size_t)im->xsize * (size_t)im->ysize * sizeof(float);
  if (size / (size_t)count / (size_t)im->xsize / sizeof(float) != (size_t)im->ysize) {
    im_push_error(aIMCTX, 0, "integer overflow calculating image data size");
    return NULL;
  }

  /* cannot fail */
  data = mymalloc(size); 
  row_size_samps = count * im->xsize;
  frow_size_bytes = row_size_samps * sizeof(i_fsample_t);
  frow = mymalloc(frow_size_bytes);
  for (y = 0, datap = data; y < im->ysize; ++y, datap += row_size_samps) {
    i_gsampf(im, 0, im->xsize, y, frow, chans, count);
    for (i = 0, frowp = frow, pixelp = datap; i < row_size_samps; ++i, ++pixelp, ++frowp) {
      *pixelp = *frowp;
    }
    if (need_alpha >= 0) {
      for (x = 0, pixelp = datap; x < im->xsize; ++x, pixelp += count) {
        pixelp[need_alpha] = 1.0F;
      }
    }
    if (need_zero >= 0) {
      for (x = 0, pixelp = datap; x < im->xsize; ++x, pixelp += count) {
        pixelp[need_zero] = 0.0F;
      }
    }
  }

  myfree(frow);

  if (flags & idf_otherendian) {
    swap_bytes(data, size, sizeof(float));
  }

  *pdata = data;
  *psize = size;

  return i_new_image_data_alloc_free(im, data);
}

static i_image_data_alloc_t *
data_palette(i_img *im, void **pdata, size_t *psize) {
  size_t size = (size_t)im->xsize * (size_t)im->ysize;
  unsigned char *data;
  unsigned char *datap;
  i_img_dim y;
    
  if (size / (size_t)im->xsize != im->ysize) {
    dIMCTXim(im);
    im_push_error(aIMCTX, 0, "integer overflow calculating image data size");
    return NULL;
  }

  data = mymalloc(size);
  for (y = 0, datap = data; y < im->ysize; ++y, datap += im->xsize) {
    i_gpal(im, 0, im->xsize, y, datap);
  }

  *pdata = data;
  *psize = size;

  return i_new_image_data_alloc_free(im, data);
}

/*
=item i_img_data_fallback()

Fallback for i_img_data().  This is used by image implementations of
i_img_data()/C<i_f_data> to produce synthesized data.

  i_img_data_fallback(im, layout, bits, flags, pptr, psize, pextrachannels)

The intent this is used something like:

 i_image_alloc_t *my_img_data(...) {
   if (layout, bits, extrachannels matches image layout) {
     ... populate *pptr, *size, *extrachannels
     ... increment image refcount
     ... return new i_image_alloc_t.
   }
   else  {
     return i_img_data_fallback(im, layout, bits, flags, pptr, psize, pextrachannels);
   }
 }

and my_img_data goes into the image's virtual table.

=cut
*/

i_image_data_alloc_t *
i_img_data_fallback(i_img *im, i_data_layout_t layout, i_img_bits_t bits, unsigned flags,
                    void **pdata, size_t *psize, int *extra) {
  dIMCTXim(im);

  im_clear_error(aIMCTX);

  if (!(flags & idf_synthesize)) {
    im_push_error(aIMCTX, 0, "image not in requested layout and synthesis not requested");
    return NULL;
  }

  if (im->channels == 0) {
    im_push_error(aIMCTX, 0, "cannot synthesize from 0 channel image");
    return NULL;
  }

  /* we never synthesize extra channels */
  *extra = 0;

  if (layout == idl_palette) {
    if (im->type == i_palette_type && bits == i_8_bits) {
      return data_palette(im, pdata, psize);
    }
    else {
      im_push_error(aIMCTX, 0, "paletted layout only valid for paletted images");
      return NULL;
    }
  }

  switch (bits) {
  case i_8_bits:
    return data_8(im, layout, pdata, psize);

  case i_double_bits:
    return data_double(im, layout, flags, pdata, psize);

  case i_16_bits:
    return data_16(im, layout, flags, pdata, psize);

  case i_float_bits:
    return data_float(im, layout, flags, pdata, psize);

  default:
    im_push_errorf(aIMCTX, 0, "image bits %d not implemented", (int)bits);
    return NULL;
  }
}

struct def_data_alloc {
  i_image_data_alloc_t head;
  i_img *im;
};

static void
release_def_alloc(i_image_data_alloc_t *alloc) {
  struct def_data_alloc *work = (struct def_data_alloc *)alloc;
  i_img_destroy(work->im);
  myfree(alloc);
}

i_image_data_alloc_t *
i_new_image_data_alloc_def(i_img *im) {
  struct def_data_alloc *result = mymalloc(sizeof(struct def_data_alloc));
  result->head.f_release = release_def_alloc;
  result->im = im;
  i_img_refcnt_inc(im);

  return &result->head;
}

struct myfree_data_alloc {
  i_image_data_alloc_t head;
  void *releaseme;
};

static void
release_myfree_alloc(i_image_data_alloc_t *alloc) {
  struct myfree_data_alloc *work = (struct myfree_data_alloc *)alloc;
  myfree(work->releaseme);
  myfree(alloc);
}

/*
=item i_new_image_data_alloc_free()

Create an image data allocation object that releases the memory block
supplied using myfree().

This is used to generate the L<i_image_data_alloc_t> object returned
by i_img_data() when data is synthesized.

=cut
*/

i_image_data_alloc_t *
i_new_image_data_alloc_free(i_img *im, void *releaseme) {
  struct myfree_data_alloc *result = mymalloc(sizeof(struct myfree_data_alloc));
  result->head.f_release = release_myfree_alloc;
  result->releaseme = releaseme;

  return &result->head;
}
