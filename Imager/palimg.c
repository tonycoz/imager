/*
=head1 NAME

  palimg.c - implements paletted images for Imager.

=head1 SYNOPSIS

=head1 DESCRIPTION

Implements paletted images using the new image interface.

=over

=item IIM_base_8bit_pal

Basic 8-bit/sample paletted image

=cut
*/

#include "image.h"
#include "imagei.h"

#define PALEXT(im) ((i_img_pal_ext*)((im)->ext_data))
static int i_ppix_p(i_img *im, int x, int y, i_color *val);
static int i_gpix_p(i_img *im, int x, int y, i_color *val);
static int i_glin_p(i_img *im, int l, int r, int y, i_color *vals);
static int i_plin_p(i_img *im, int l, int r, int y, i_color *vals);
static int i_gsamp_p(i_img *im, int l, int r, int y, i_sample_t *samps, int const *chans, int chan_count);
static int i_gpal_p(i_img *pm, int l, int r, int y, i_palidx *vals);
static int i_ppal_p(i_img *pm, int l, int r, int y, i_palidx *vals);
static int i_addcolors_p(i_img *im, i_color *color, int count);
static int i_getcolors_p(i_img *im, int i, i_color *color, int count);
static int i_colorcount_p(i_img *im);
static int i_maxcolors_p(i_img *im);
static int i_findcolor_p(i_img *im, i_color *color, i_palidx *entry);
static int i_setcolors_p(i_img *im, int index, i_color *color, int count);

static void i_destroy_p(i_img *im);

static i_img IIM_base_8bit_pal =
{
  0, /* channels set */
  0, 0, 0, /* xsize, ysize, bytes */
  ~0U, /* ch_mask */
  i_8_bits, /* bits */
  i_palette_type, /* type */
  0, /* virtual */
  NULL, /* idata */
  { 0, 0, NULL }, /* tags */
  NULL, /* ext_data */

  i_ppix_p, /* i_f_ppix */
  i_ppixf_fp, /* i_f_ppixf */
  i_plin_p, /* i_f_plin */
  i_plinf_fp, /* i_f_plinf */
  i_gpix_p, /* i_f_gpix */
  i_gpixf_fp, /* i_f_gpixf */
  i_glin_p, /* i_f_glin */
  i_glinf_fp, /* i_f_glinf */
  i_gsamp_p, /* i_f_gsamp */
  i_gsampf_fp, /* i_f_gsampf */

  i_gpal_p, /* i_f_gpal */
  i_ppal_p, /* i_f_ppal */
  i_addcolors_p, /* i_f_addcolors */
  i_getcolors_p, /* i_f_getcolors */
  i_colorcount_p, /* i_f_colorcount */
  i_maxcolors_p, /* i_f_maxcolors */
  i_findcolor_p, /* i_f_findcolor */
  i_setcolors_p, /* i_f_setcolors */

  i_destroy_p, /* i_f_destroy */
};

/*
=item i_img_pal_new_low(i_img *im, int x, int y, int channels, int maxpal)

Creates a new paletted image.

Currently 0 < maxpal <= 256

=cut
*/
i_img *i_img_pal_new_low(i_img *im, int x, int y, int channels, int maxpal) {
  i_img_pal_ext *palext;
  int bytes;

  i_clear_error();
  if (maxpal < 0 || maxpal > 256) {
    i_push_error(0, "Maximum of 256 palette entries");
    return NULL;
  }
  if (x < 1 || y < 1) {
    i_push_error(0, "Image sizes must be positive");
    return NULL;
  }
  if (channels < 1 || channels > MAXCHANNELS) {
    i_push_errorf(0, "Channels must be positive and <= %d", MAXCHANNELS);
    return NULL;
  }
  bytes = sizeof(i_palidx) * x * y;
  if (bytes / y / sizeof(i_palidx) != x) {
    i_push_errorf(0, "integer overflow calculating image allocation");
    return NULL;
  }

  memcpy(im, &IIM_base_8bit_pal, sizeof(i_img));
  palext = mymalloc(sizeof(i_img_pal_ext));
  palext->pal = mymalloc(sizeof(i_color) * maxpal);
  palext->count = 0;
  palext->alloc = maxpal;
  palext->last_found = -1;
  im->ext_data = palext;
  i_tags_new(&im->tags);
  im->bytes = bytes;
  im->idata = mymalloc(im->bytes);
  im->channels = channels;
  memset(im->idata, 0, im->bytes);
  im->xsize = x;
  im->ysize = y;
  
  return im;
}

i_img *i_img_pal_new(int x, int y, int channels, int maxpal) {
  i_img *im;
  mm_log((1, "i_img_pal_new(x %d, y %d, channels %d, maxpal %d)\n", x, y, channels, maxpal));
  im = mymalloc(sizeof(i_img));
  if (!i_img_pal_new_low(im, x, y, channels, maxpal)) {
    myfree(im);
    im = NULL;
  }

  return im;
}

/*
=item i_img_rgb_convert(i_img *targ, i_img *src)

Converts paletted data in src to RGB data in targ

Internal function.

src must be a paletted image and targ must be an RGB image with the
same width, height and channels.

=cut
*/
static void i_img_rgb_convert(i_img *targ, i_img *src) {
  i_color *row = mymalloc(sizeof(i_color) * targ->xsize);
  int y;
  for (y = 0; y < targ->ysize; ++y) {
    i_glin(src, 0, src->xsize, y, row);
    i_plin(targ, 0, src->xsize, y, row);
  }
  myfree(row);
}

/*
=item i_img_to_rgb_inplace(im)

Converts im from a paletted image to an RGB image.

The conversion is done in place.

The conversion cannot be done for virtual images.

=cut
*/
int i_img_to_rgb_inplace(i_img *im) {
  i_img temp;
  i_color *pal;
  int palsize;

  if (im->virtual)
    return 0;

  if (im->type == i_direct_type)
    return 1; /* trivial success */

  i_img_empty_ch(&temp, im->xsize, im->ysize, im->channels);
  i_img_rgb_convert(&temp, im);

  /* nasty hack */
  (im->i_f_destroy)(im);
  myfree(im->idata);
  *im = temp;

  return 1;
}

/*
=item i_img_to_pal(i_img *im, i_quantize *quant)

Converts an RGB image to a paletted image

=cut
*/
i_img *i_img_to_pal(i_img *src, i_quantize *quant) {
  i_palidx *result;
  i_img *im;

  i_clear_error();
  
  quant_makemap(quant, &src, 1);
  result = quant_translate(quant, src);

  if (result) {

    im = i_img_pal_new(src->xsize, src->ysize, src->channels, quant->mc_size);

    /* copy things over */
    memcpy(im->idata, result, im->bytes);
    PALEXT(im)->count = quant->mc_count;
    memcpy(PALEXT(im)->pal, quant->mc_colors, sizeof(i_color) * quant->mc_count);
    
    myfree(result);

    return im;
  }
  else {
    return NULL;
  }
}

/*
=item i_img_to_rgb(i_img *src)

=cut
*/
i_img *i_img_to_rgb(i_img *src) {
  i_img *im = i_img_empty_ch(NULL, src->xsize, src->ysize, src->channels);
  i_img_rgb_convert(im, src);

  return im;
}

/*
=item i_destroy_p(i_img *im)

Destroys data related to a paletted image.

=cut
*/
static void i_destroy_p(i_img *im) {
  if (im) {
    i_img_pal_ext *palext = im->ext_data;
    if (palext) {
      if (palext->pal)
        myfree(palext->pal);
      myfree(palext);
    }
  }
}

/*
=item i_ppix_p(i_img *im, int x, int y, i_color *val)

Write to a pixel in the image.

Warning: converts the image to a RGB image if the color isn't already
present in the image.

=cut
*/
static int i_ppix_p(i_img *im, int x, int y, i_color *val) {
  i_palidx which;
  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize)
    return -1;
  if (i_findcolor(im, val, &which)) {
    ((i_palidx *)im->idata)[x + y * im->xsize] = which;
    return 0;
  }
  else {
    if (i_img_to_rgb_inplace(im)) {
      return i_ppix(im, x, y, val);
    }
    else
      return -1;
  }
}

/*
=item i_gpix(i_img *im, int x, int y, i_color *val)

Retrieve a pixel, converting from a palette index to a color.

=cut
*/
static int i_gpix_p(i_img *im, int x, int y, i_color *val) {
  i_palidx which;
  if (x < 0 || x >= im->xsize || y < 0 || y >= im->ysize) {
    return -1;
  }
  which = ((i_palidx *)im->idata)[x + y * im->xsize];
  if (which > PALEXT(im)->count)
    return -1;
  *val = PALEXT(im)->pal[which];

  return 0;
}

/*
=item i_glinp(i_img *im, int l, int r, int y, i_color *vals)

Retrieve a row of pixels.

=cut
*/
static int i_glin_p(i_img *im, int l, int r, int y, i_color *vals) {
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    int palsize = PALEXT(im)->count;
    i_color *pal = PALEXT(im)->pal;
    i_palidx *data;
    int count, i;
    if (r > im->xsize)
      r = im->xsize;
    data = ((i_palidx *)im->idata) + l + y * im->xsize;
    count = r - l;
    for (i = 0; i < count; ++i) {
      i_palidx which = *data++;
      if (which < palsize)
        vals[i] = pal[which];
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_plin_p(i_img *im, int l, int r, int y, i_color *vals)

Write a line of color data to the image.

If any color value is not in the image when the image is converted to 
RGB.

=cut
*/
static int i_plin_p(i_img *im, int l, int r, int y, i_color *vals) {
  int ch, count, i;
  i_palidx *data;
  i_palidx which;
  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = ((i_palidx *)im->idata) + l + y * im->xsize;
    count = r - l;
    for (i = 0; i < count; ++i) {
      if (i_findcolor(im, vals+i, &which)) {
        ((i_palidx *)data)[i] = which;
      }
      else {
        if (i_img_to_rgb_inplace(im)) {
          return i+i_plin(im, l+i, r, y, vals+i);
        }
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_gsamp_p(i_img *im, int l, int r, int y, i_sample_t *samps, int chans, int chan_count)

=cut
*/
static int i_gsamp_p(i_img *im, int l, int r, int y, i_sample_t *samps, 
              int const *chans, int chan_count) {
  int ch;
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    int palsize = PALEXT(im)->count;
    i_color *pal = PALEXT(im)->pal;
    i_palidx *data;
    int count, i, w;
    if (r > im->xsize)
      r = im->xsize;
    data = ((i_palidx *)im->idata) + l + y * im->xsize;
    count = 0;
    w = r - l;
    if (chans) {
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= im->channels) {
          i_push_errorf(0, "No channel %d in this image", chans[ch]);
        }
      }

      for (i = 0; i < w; ++i) {
        i_palidx which = *data++;
        if (which < palsize) {
          for (ch = 0; ch < chan_count; ++ch) {
            *samps++ = pal[which].channel[chans[ch]];
            ++count;
          }
        }
      }
    }
    else {
      for (i = 0; i < w; ++i) {
        i_palidx which = *data++;
        if (which < palsize) {
          for (ch = 0; ch < chan_count; ++ch) {
            *samps++ = pal[which].channel[ch];
            ++count;
          }
        }
      }
    }
    return count;
  }
  else {
    return 0;
  }
}

/*
=item i_gpal_p(i_img *im, int l, int r, int y, i_palidx *vals)

=cut
*/

static int i_gpal_p(i_img *im, int l, int r, int y, i_palidx *vals) {
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_palidx *data;
    int i, w;
    if (r > im->xsize)
      r = im->xsize;
    data = ((i_palidx *)im->idata) + l + y * im->xsize;
    w = r - l;
    for (i = 0; i < w; ++i) {
      *vals++ = *data++;
    }
    return i;
  }
  else {
    return 0;
  }
}

/*
=item i_ppal_p(i_img *im, int l, int r, int y, i_palidx *vals)

=cut
*/

static int i_ppal_p(i_img *im, int l, int r, int y, i_palidx *vals) {
  if (y >= 0 && y < im->ysize && l < im->xsize && l >= 0) {
    i_palidx *data;
    int i, w;
    if (r > im->xsize)
      r = im->xsize;
    data = ((i_palidx *)im->idata) + l + y * im->xsize;
    w = r - l;
    for (i = 0; i < w; ++i) {
      *data++ = *vals++;
    }
    return i;
  }
  else {
    return 0;
  }
}

/*
=item i_addcolors_p(i_img *im, i_color *color, int count)

=cut
*/
static int i_addcolors_p(i_img *im, i_color *color, int count) {
  if (PALEXT(im)->count + count <= PALEXT(im)->alloc) {
    int result = PALEXT(im)->count;
    int index = result;

    PALEXT(im)->count += count;
    while (count) {
      PALEXT(im)->pal[index++] = *color++;
      --count;
    }

    return result;
  }
  else
    return -1;
}

/*
=item i_getcolors_p(i_img *im, int i, i_color *color, int count)

=cut
*/
static int i_getcolors_p(i_img *im, int i, i_color *color, int count) {
  if (i >= 0 && i+count <= PALEXT(im)->count) {
    while (count) {
      *color++ = PALEXT(im)->pal[i++];
      --count;
    }
    return 1;
  }
  else
    return 0;
}

static int color_eq(i_img *im, i_color *c1, i_color *c2) {
  int ch;
  for (ch = 0; ch < im->channels; ++ch) {
    if (c1->channel[ch] != c2->channel[ch])
      return 0;
  }
  return 1;
}

/*
=item i_colorcount_p(i_img *im)

=cut
*/
static int i_colorcount_p(i_img *im) {
  return PALEXT(im)->count;
}

/*
=item i_maxcolors_p(i_img *im)

=cut
*/
static int i_maxcolors_p(i_img *im) {
  return PALEXT(im)->alloc;
}

/*
=item i_setcolors_p(i_img *im, int index, i_color *colors, int count)

=cut
*/
static int i_setcolors_p(i_img *im, int index, i_color *colors, int count) {
  if (index >= 0 && count >= 1 && index + count < PALEXT(im)->count) {
    while (count) {
      PALEXT(im)->pal[index++] = *colors++;
      --count;
    }
    return 1;
  }

  return 0;
}

/*
=item i_findcolor_p(i_img *im)

=cut
*/
static int i_findcolor_p(i_img *im, i_color *color, i_palidx *entry) {
  if (PALEXT(im)->count) {
    int i;
    /* often the same color comes up several times in a row */
    if (PALEXT(im)->last_found >= 0) {
      if (color_eq(im, color, PALEXT(im)->pal + PALEXT(im)->last_found)) {
        *entry = PALEXT(im)->last_found;
        return 1;
      }
    }
    for (i = 0; i < PALEXT(im)->count; ++i) {
      if (color_eq(im, color, PALEXT(im)->pal + i)) {
        PALEXT(im)->last_found = *entry = i;
        return 1;
      }
    }
  }
  return 0;
}

/*
=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
*/
