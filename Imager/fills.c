#include "image.h"
#include "imagei.h"

/*
=head1 NAME

fills.c - implements the basic general fills

=head1 SYNOPSIS

  i_fill_t *fill;
  i_color c1, c2;
  i_fcolor fc1, fc2;
  int combine;
  fill = i_new_fill_solidf(&fc1, combine);
  fill = i_new_fill_solid(&c1, combine);
  fill = i_new_fill_hatchf(&fc1, &fc2, combine, hatch, cust_hash, dx, dy);
  fill = i_new_fill_hatch(&c1, &c2, combine, hatch, cust_hash, dx, dy);
  fill = i_new_fill_image(im, matrix, xoff, yoff, combine);
  i_fill_destroy(fill);

=head1 DESCRIPTION

Implements the basic general fills, which can be used for filling some
shapes and for flood fills.

Each fill can implement up to 3 functions:

=over

=item fill_with_color

called for fills on 8-bit images.  This can be NULL in which case the
fill_with_colorf function is called.

=item fill_with_fcolor

called for fills on non-8-bit images or when fill_with_color is NULL.

=item destroy

called by i_fill_destroy() if non-NULL, to release any extra resources
that the fill may need.

=back

fill_with_color and fill_with_fcolor are basically the same function
except that the first works with lines of i_color and the second with
lines of i_fcolor.

If the combines member if non-zero the line data is populated from the
target image before calling fill_with_*color.

fill_with_color needs to fill the I<data> parameter with the fill
pixels.  If combines is non-zero it the fill pixels should be combined
with the existing data.

The current fills are:

=over

=item *

solid fill

=item *

hatched fill

=item *

fountain fill

=back

Fountain fill is implemented by L<filters.c>.

Other fills that could be implemented include:

=over

=item *

image - an image tiled over the fill area, with an offset either
horizontally or vertically.

=item *

checkerboard - combine 2 fills in a checkerboard

=item *

combine - combine the levels of 2 other fills based in the levels of
an image

=item *

regmach - use the register machine to generate colors

=back

=over

=cut
*/

static i_color fcolor_to_color(i_fcolor *c) {
  int ch;
  i_color out;

  for (ch = 0; ch < MAXCHANNELS; ++ch)
    out.channel[ch] = SampleFTo8(c->channel[ch]);

  return out;
}

static i_fcolor color_to_fcolor(i_color *c) {
  int ch;
  i_fcolor out;

  for (ch = 0; ch < MAXCHANNELS; ++ch)
    out.channel[ch] = Sample8ToF(c->channel[ch]);

  return out;
}

/* alpha combine in with out */
#define COMBINE(out, in, channels) \
  { \
    int ch; \
    for (ch = 0; ch < (channels); ++ch) { \
      (out).channel[ch] = ((out).channel[ch] * (255 - (in).channel[3]) \
        + (in).channel[ch] * (in).channel[3]) / 255; \
    } \
  }

/* alpha combine in with out, in this case in is a simple array of
   samples, potentially not integers - the mult combiner uses doubles
   for accuracy */
#define COMBINEA(out, in, channels) \
  { \
    int ch; \
    for (ch = 0; ch < (channels); ++ch) { \
      (out).channel[ch] = ((out).channel[ch] * (255 - (in)[3]) \
        + (in)[ch] * (in)[3]) / 255; \
    } \
  }

#define COMBINEF(out, in, channels) \
  { \
    int ch; \
    for (ch = 0; ch < (channels); ++ch) { \
      (out).channel[ch] = (out).channel[ch] * (1.0 - (in).channel[3]) \
        + (in).channel[ch] * (in).channel[3]; \
    } \
  }

typedef struct
{
  i_fill_t base;
  i_color c;
  i_fcolor fc;
} i_fill_solid_t;

static void fill_solid(i_fill_t *, int x, int y, int width, int channels, 
                       i_color *);
static void fill_solidf(i_fill_t *, int x, int y, int width, int channels, 
                        i_fcolor *);
static void fill_solid_comb(i_fill_t *, int x, int y, int width, int channels, 
                            i_color *);
static void fill_solidf_comb(i_fill_t *, int x, int y, int width, 
                             int channels, i_fcolor *);

static i_fill_solid_t base_solid_fill =
{
  {
    fill_solid,
    fill_solidf,
    NULL,
    NULL,
    NULL,
  },
};
static i_fill_solid_t base_solid_fill_comb =
{
  {
    fill_solid_comb,
    fill_solidf_comb,
    NULL,
    NULL,
    NULL,
  },
};

/*
=item i_fill_destroy(fill)

Call to destroy any fill object.

=cut
*/

void
i_fill_destroy(i_fill_t *fill) {
  if (fill->destroy)
    (fill->destroy)(fill);
  myfree(fill);
}

/*
=item i_new_fill_solidf(color, combine)

Create a solid fill based on a float color.

If combine is non-zero then alpha values will be combined.

=cut
*/

i_fill_t *
i_new_fill_solidf(i_fcolor *c, int combine) {
  int ch;
  i_fill_solid_t *fill = mymalloc(sizeof(i_fill_solid_t));
  
  if (combine) {
    *fill = base_solid_fill_comb;
    i_get_combine(combine, &fill->base.combine, &fill->base.combinef);
  }
  else
    *fill = base_solid_fill;
  fill->fc = *c;
  for (ch = 0; ch < MAXCHANNELS; ++ch) {
    fill->c.channel[ch] = SampleFTo8(c->channel[ch]);
  }
  
  return &fill->base;
}

/*
=item i_new_fill_solid(color, combine)

Create a solid fill based.

If combine is non-zero then alpha values will be combined.

=cut
*/

i_fill_t *
i_new_fill_solid(i_color *c, int combine) {
  int ch;
  i_fill_solid_t *fill = mymalloc(sizeof(i_fill_solid_t));

  if (combine) {
    *fill = base_solid_fill_comb;
    i_get_combine(combine, &fill->base.combine, &fill->base.combinef);
  }
  else
    *fill = base_solid_fill;
  fill->c = *c;
  for (ch = 0; ch < MAXCHANNELS; ++ch) {
    fill->fc.channel[ch] = Sample8ToF(c->channel[ch]);
  }
  
  return &fill->base;
}

static unsigned char
builtin_hatches[][8] =
{
  {
    /* 1x1 checkerboard */
    0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,
  },
  {
    /* 2x2 checkerboard */
    0xCC, 0xCC, 0x33, 0x33, 0xCC, 0xCC, 0x33, 0x33,
  },
  {
    /* 4 x 4 checkerboard */
    0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,
  },
  {
    /* single vertical lines */
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  },
  {
    /* double vertical lines */
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 
  },
  {
    /* quad vertical lines */
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
  },
  {
    /* single hlines */
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  },
  {
    /* double hlines */
    0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
  },
  {
    /* quad hlines */
    0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
  },
  {
    /* single / */
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
  },
  {
    /* single \ */
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
  },
  {
    /* double / */
    0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88,
  },
  {
    /* double \ */
    0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11,
  },
  {
    /* single grid */
    0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  },
  {
    /* double grid */
    0xFF, 0x88, 0x88, 0x88, 0xFF, 0x88, 0x88, 0x88,
  },
  {
    /* quad grid */
    0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA,
  },
  {
    /* single dots */
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  },
  {
    /* 4 dots */
    0x88, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00,
  },
  {
    /* 16 dots */
    0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
  },
  {
    /* simple stipple */
    0x48, 0x84, 0x00, 0x00, 0x84, 0x48, 0x00, 0x00,
  },
  {
    /* weave */
    0x55, 0xFD, 0x05, 0xFD, 0x55, 0xDF, 0x50, 0xDF,
  },
  {
    /* single cross hatch */
    0x82, 0x44, 0x28, 0x10, 0x28, 0x44, 0x82, 0x01,
  },
  {
    /* double cross hatch */
    0xAA, 0x44, 0xAA, 0x11, 0xAA, 0x44, 0xAA, 0x11,
  },
  {
    /* vertical lozenge */
    0x11, 0x11, 0x11, 0xAA, 0x44, 0x44, 0x44, 0xAA,
  },
  {
    /* horizontal lozenge */
    0x88, 0x70, 0x88, 0x07, 0x88, 0x70, 0x88, 0x07,
  },
  {
    /* scales overlapping downwards */
    0x80, 0x80, 0x41, 0x3E, 0x08, 0x08, 0x14, 0xE3,
  },
  {
    /* scales overlapping upwards */
    0xC7, 0x28, 0x10, 0x10, 0x7C, 0x82, 0x01, 0x01,
  },
  {
    /* scales overlapping leftwards */
    0x83, 0x84, 0x88, 0x48, 0x38, 0x48, 0x88, 0x84,
  },
  {
    /* scales overlapping rightwards */
    0x21, 0x11, 0x12, 0x1C, 0x12, 0x11, 0x21, 0xC1,
  },
  {
    /* denser stipple */
    0x44, 0x88, 0x22, 0x11, 0x44, 0x88, 0x22, 0x11,
  },
  {
    /* L-shaped tiles */
    0xFF, 0x84, 0x84, 0x9C, 0x94, 0x9C, 0x90, 0x90,
  },
  {
    /* wider stipple */
    0x80, 0x40, 0x20, 0x00, 0x02, 0x04, 0x08, 0x00,
  },
};

typedef struct
{
  i_fill_t base;
  i_color fg, bg;
  i_fcolor ffg, fbg;
  unsigned char hatch[8];
  int dx, dy;
} i_fill_hatch_t;

static void fill_hatch(i_fill_t *fill, int x, int y, int width, int channels, 
                       i_color *data);
static void fill_hatchf(i_fill_t *fill, int x, int y, int width, int channels, 
                        i_fcolor *data);
static
i_fill_t *
i_new_hatch_low(i_color *fg, i_color *bg, i_fcolor *ffg, i_fcolor *fbg, 
                int combine, int hatch, unsigned char *cust_hatch,
                int dx, int dy);

/*
=item i_new_fill_hatch(fg, bg, combine, hatch, cust_hatch, dx, dy)

Creates a new hatched fill with the fg color used for the 1 bits in
the hatch and bg for the 0 bits.  If combine is non-zero alpha values
will be combined.

If cust_hatch is non-NULL it should be a pointer to 8 bytes of the
hash definition, with the high-bits to the left.

If cust_hatch is NULL then one of the standard hatches is used.

(dx, dy) are an offset into the hatch which can be used to unalign adjoining areas, or to align the origin of a hatch with the the side of a filled area.

=cut
*/
i_fill_t *
i_new_fill_hatch(i_color *fg, i_color *bg, int combine, int hatch, 
            unsigned char *cust_hatch, int dx, int dy) {
  return i_new_hatch_low(fg, bg, NULL, NULL, combine, hatch, cust_hatch, 
                         dx, dy);
}

/*
=item i_new_fill_hatchf(fg, bg, combine, hatch, cust_hatch, dx, dy)

Creates a new hatched fill with the fg color used for the 1 bits in
the hatch and bg for the 0 bits.  If combine is non-zero alpha values
will be combined.

If cust_hatch is non-NULL it should be a pointer to 8 bytes of the
hash definition, with the high-bits to the left.

If cust_hatch is NULL then one of the standard hatches is used.

(dx, dy) are an offset into the hatch which can be used to unalign adjoining areas, or to align the origin of a hatch with the the side of a filled area.

=cut
*/
i_fill_t *
i_new_fill_hatchf(i_fcolor *fg, i_fcolor *bg, int combine, int hatch, 
            unsigned char *cust_hatch, int dx, int dy) {
  return i_new_hatch_low(NULL, NULL, fg, bg, combine, hatch, cust_hatch, 
                         dx, dy);
}

static void fill_image(i_fill_t *fill, int x, int y, int width, int channels,
                       i_color *data);
static void fill_imagef(i_fill_t *fill, int x, int y, int width, int channels,
                       i_fcolor *data);
struct i_fill_image_t {
  i_fill_t base;
  i_img *src;
  int xoff, yoff;
  int has_matrix;
  double matrix[9];
};

/*
=item i_new_fill_image(im, matrix, xoff, yoff, combine)

Create an image based fill.

=cut
*/
i_fill_t *
i_new_fill_image(i_img *im, double *matrix, int xoff, int yoff, int combine) {
  struct i_fill_image_t *fill = mymalloc(sizeof(*fill));

  fill->base.fill_with_color = fill_image;
  fill->base.fill_with_fcolor = fill_imagef;
  fill->base.destroy = NULL;

  if (combine) {
    i_get_combine(combine, &fill->base.combine, &fill->base.combinef);
  }
  else {
    fill->base.combine = NULL;
    fill->base.combinef = NULL;
  }
  fill->src = im;
  if (xoff < 0)
    xoff += im->xsize;
  fill->xoff = xoff;
  if (yoff < 0)
    yoff += im->ysize;
  fill->yoff = yoff;
  if (matrix) {
    fill->has_matrix = 1;
    memcpy(fill->matrix, matrix, sizeof(fill->matrix));
  }
  else
    fill->has_matrix = 0;

  return &fill->base;
}


#define T_SOLID_FILL(fill) ((i_fill_solid_t *)(fill))

/*
=back

=head1 INTERNAL FUNCTIONS

=over

=item fill_solid(fill, x, y, width, channels, data)

The 8-bit sample fill function for non-combining solid fills.

=cut
*/
static void
fill_solid(i_fill_t *fill, int x, int y, int width, int channels, 
           i_color *data) {
  while (width-- > 0) {
    *data++ = T_SOLID_FILL(fill)->c;
  }
}

/*
=item fill_solid(fill, x, y, width, channels, data)

The floating sample fill function for non-combining solid fills.

=cut
*/
static void
fill_solidf(i_fill_t *fill, int x, int y, int width, int channels, 
           i_fcolor *data) {
  while (width-- > 0) {
    *data++ = T_SOLID_FILL(fill)->fc;
  }
}

/*
=item fill_solid_comb(fill, x, y, width, channels, data)

The 8-bit sample fill function for combining solid fills.

=cut
*/
static void
fill_solid_comb(i_fill_t *fill, int x, int y, int width, int channels, 
                i_color *data) {
  i_color c = T_SOLID_FILL(fill)->c;

  while (width-- > 0) {
    *data++ = c;
  }
}

/*
=item fill_solidf_comb(fill, x, y, width, channels, data)

The floating sample fill function for combining solid fills.

=cut
*/
static void
fill_solidf_comb(i_fill_t *fill, int x, int y, int width, int channels, 
           i_fcolor *data) {
  i_fcolor c = T_SOLID_FILL(fill)->fc;

  while (width-- > 0) {
    *data++ = c;
  }
}

/*
=item i_new_hatch_low(fg, bg, ffg, fbg, combine, hatch, cust_hatch, dx, dy)

Implements creation of hatch fill objects.

=cut
*/
static
i_fill_t *
i_new_hatch_low(i_color *fg, i_color *bg, i_fcolor *ffg, i_fcolor *fbg, 
                int combine, int hatch, unsigned char *cust_hatch,
                int dx, int dy) {
  i_fill_hatch_t *fill = mymalloc(sizeof(i_fill_hatch_t));

  fill->base.fill_with_color = fill_hatch;
  fill->base.fill_with_fcolor = fill_hatchf;
  fill->base.destroy = NULL;
  fill->fg = fg ? *fg : fcolor_to_color(ffg);
  fill->bg = bg ? *bg : fcolor_to_color(fbg);
  fill->ffg = ffg ? *ffg : color_to_fcolor(fg);
  fill->fbg = fbg ? *fbg : color_to_fcolor(bg);
  if (combine) {
    i_get_combine(combine, &fill->base.combine, &fill->base.combinef);
  }
  else {
    fill->base.combine = NULL;
    fill->base.combinef = NULL;
  }
  if (cust_hatch) {
    memcpy(fill->hatch, cust_hatch, 8);
  }
  else {
    if (hatch > sizeof(builtin_hatches)/sizeof(*builtin_hatches)) 
      hatch = 0;
    memcpy(fill->hatch, builtin_hatches[hatch], 8);
  }
  fill->dx = dx & 7;
  fill->dy = dy & 7;

  return &fill->base;
}

/*
=item fill_hatch(fill, x, y, width, channels, data)

The 8-bit sample fill function for hatched fills.

=cut
*/
static void fill_hatch(i_fill_t *fill, int x, int y, int width, int channels, 
                       i_color *data) {
  i_fill_hatch_t *f = (i_fill_hatch_t *)fill;
  int byte = f->hatch[(y + f->dy) & 7];
  int xpos = (x + f->dx) & 7;
  int mask = 128 >> xpos;

  while (width-- > 0) {
    *data++ = (byte & mask) ? f->fg : f->bg;
    
    if ((mask >>= 1) == 0)
      mask = 128;
  }
}

/*
=item fill_hatchf(fill, x, y, width, channels, data)

The floating sample fill function for hatched fills.

=back
*/
static void fill_hatchf(i_fill_t *fill, int x, int y, int width, int channels, 
                        i_fcolor *data) {
  i_fill_hatch_t *f = (i_fill_hatch_t *)fill;
  int byte = f->hatch[(y + f->dy) & 7];
  int xpos = (x + f->dx) & 7;
  int mask = 128 >> xpos;
  
  while (width-- > 0) {
    *data++ = (byte & mask) ? f->ffg : f->fbg;
    
    if ((mask >>= 1) == 0)
      mask = 128;
  }
}

/* hopefully this will be inlined  (it is with -O3 with gcc 2.95.4) */
/* linear interpolation */
static i_color interp_i_color(i_color before, i_color after, double pos,
                              int channels) {
  i_color out;
  int ch;

  pos -= floor(pos);
  for (ch = 0; ch < channels; ++ch)
    out.channel[ch] = (1-pos) * before.channel[ch] + pos * after.channel[ch];
  if (out.channel[3])
    for (ch = 0; ch < channels; ++ch)
      if (ch != 3) {
        int temp = out.channel[ch] * 255 / out.channel[3];
        if (temp > 255)
          temp = 255;
        out.channel[ch] = temp;
      }

  return out;
}

/* hopefully this will be inlined  (it is with -O3 with gcc 2.95.4) */
/* linear interpolation */
static i_fcolor interp_i_fcolor(i_fcolor before, i_fcolor after, double pos,
                                int channels) {
  i_fcolor out;
  int ch;

  pos -= floor(pos);
  for (ch = 0; ch < channels; ++ch)
    out.channel[ch] = (1-pos) * before.channel[ch] + pos * after.channel[ch];
  if (out.channel[3])
    for (ch = 0; ch < channels; ++ch)
      if (ch != 3) {
        int temp = out.channel[ch] / out.channel[3];
        if (temp > 1.0)
          temp = 1.0;
        out.channel[ch] = temp;
      }

  return out;
}

/*
=item fill_image(fill, x, y, width, channels, data, work)

=cut
*/
static void fill_image(i_fill_t *fill, int x, int y, int width, int channels,
                       i_color *data) {
  struct i_fill_image_t *f = (struct i_fill_image_t *)fill;
  int i = 0;
  i_color c;
  
  if (f->has_matrix) {
    /* the hard way */
    while (i < width) {
      double rx = f->matrix[0] * (x+i) + f->matrix[1] * y + f->matrix[2];
      double ry = f->matrix[3] * (x+i) + f->matrix[4] * y + f->matrix[5];
      double ix = floor(rx / f->src->xsize);
      double iy = floor(ry / f->src->ysize);
      i_color c[2][2];
      i_color c2[2];
      int dy;

      if (f->xoff) {
        rx += iy * f->xoff;
        ix = floor(rx / f->src->xsize);
      }
      else if (f->yoff) {
        ry += ix * f->yoff;
        iy = floor(ry / f->src->ysize);
      }
      rx -= ix * f->src->xsize;
      ry -= iy * f->src->ysize;

      for (dy = 0; dy < 2; ++dy) {
        if ((int)rx == f->src->xsize-1) {
          i_gpix(f->src, f->src->xsize-1, ((int)ry+dy) % f->src->ysize, &c[dy][0]);
          i_gpix(f->src, 0, ((int)ry+dy) % f->src->xsize, &c[dy][1]);
        }
        else {
          i_glin(f->src, (int)rx, (int)rx+2, ((int)ry+dy) % f->src->ysize, 
                 c[dy]);
        }
        c2[dy] = interp_i_color(c[dy][0], c[dy][1], rx, f->src->channels);
      }
      *data++ = interp_i_color(c2[0], c2[1], ry, f->src->channels);
      ++i;
    }
  }
  else {
    /* the easy way */
    /* this should be possible to optimize to use i_glin() */
    while (i < width) {
      int rx = x+i;
      int ry = y;
      int ix = rx / f->src->xsize;
      int iy = ry / f->src->ysize;

      if (f->xoff) {
        rx += iy * f->xoff;
        ix = rx / f->src->xsize;
      }
      else if (f->yoff) {
        ry += ix * f->yoff;
        iy = ry / f->src->xsize;
      }
      rx -= ix * f->src->xsize;
      ry -= iy * f->src->ysize;
      i_gpix(f->src, rx, ry, data);
      ++data;
      ++i;
    }
  }
}

/*
=item fill_image(fill, x, y, width, channels, data, work)

=cut
*/
static void fill_imagef(i_fill_t *fill, int x, int y, int width, int channels,
                       i_fcolor *data) {
  struct i_fill_image_t *f = (struct i_fill_image_t *)fill;
  int i = 0;
  i_fcolor c;
  
  if (f->has_matrix) {
    /* the hard way */
    while (i < width) {
      double rx = f->matrix[0] * (x+i) + f->matrix[1] * y + f->matrix[2];
      double ry = f->matrix[3] * (x+i) + f->matrix[4] * y + f->matrix[5];
      double ix = floor(rx / f->src->xsize);
      double iy = floor(ry / f->src->ysize);
      i_fcolor c[2][2];
      i_fcolor c2[2];
      int dy;

      if (f->xoff) {
        rx += iy * f->xoff;
        ix = floor(rx / f->src->xsize);
      }
      else if (f->yoff) {
        ry += ix * f->yoff;
        iy = floor(ry / f->src->ysize);
      }
      rx -= ix * f->src->xsize;
      ry -= iy * f->src->ysize;

      for (dy = 0; dy < 2; ++dy) {
        if ((int)rx == f->src->xsize-1) {
          i_gpixf(f->src, f->src->xsize-1, ((int)ry+dy) % f->src->ysize, &c[dy][0]);
          i_gpixf(f->src, 0, ((int)ry+dy) % f->src->xsize, &c[dy][1]);
        }
        else {
          i_glinf(f->src, (int)rx, (int)rx+2, ((int)ry+dy) % f->src->ysize, 
                 c[dy]);
        }
        c2[dy] = interp_i_fcolor(c[dy][0], c[dy][1], rx, f->src->channels);
      }
      *data++ = interp_i_fcolor(c2[0], c2[1], ry, f->src->channels);
      ++i;
    }
  }
  else {
    /* the easy way */
    /* this should be possible to optimize to use i_glin() */
    while (i < width) {
      int rx = x+i;
      int ry = y;
      int ix = rx / f->src->xsize;
      int iy = ry / f->src->ysize;

      if (f->xoff) {
        rx += iy * f->xoff;
        ix = rx / f->src->xsize;
      }
      else if (f->yoff) {
        ry += ix * f->yoff;
        iy = ry / f->src->xsize;
      }
      rx -= ix * f->src->xsize;
      ry -= iy * f->src->ysize;
      i_gpixf(f->src, rx, ry, data);
      ++data;
      ++i;
    }
  }
}

static void combine_replace(i_color *, i_color *, int, int);
static void combine_replacef(i_fcolor *, i_fcolor *, int, int);
static void combine_alphablend(i_color *, i_color *, int, int);
static void combine_alphablendf(i_fcolor *, i_fcolor *, int, int);
static void combine_mult(i_color *, i_color *, int, int);
static void combine_multf(i_fcolor *, i_fcolor *, int, int);
static void combine_dissolve(i_color *, i_color *, int, int);
static void combine_dissolvef(i_fcolor *, i_fcolor *, int, int);
static void combine_add(i_color *, i_color *, int, int);
static void combine_addf(i_fcolor *, i_fcolor *, int, int);
static void combine_subtract(i_color *, i_color *, int, int);
static void combine_subtractf(i_fcolor *, i_fcolor *, int, int);
static void combine_diff(i_color *, i_color *, int, int);
static void combine_difff(i_fcolor *, i_fcolor *, int, int);
static void combine_darken(i_color *, i_color *, int, int);
static void combine_darkenf(i_fcolor *, i_fcolor *, int, int);
static void combine_lighten(i_color *, i_color *, int, int);
static void combine_lightenf(i_fcolor *, i_fcolor *, int, int);
static void combine_hue(i_color *, i_color *, int, int);
static void combine_huef(i_fcolor *, i_fcolor *, int, int);
static void combine_sat(i_color *, i_color *, int, int);
static void combine_satf(i_fcolor *, i_fcolor *, int, int);
static void combine_value(i_color *, i_color *, int, int);
static void combine_valuef(i_fcolor *, i_fcolor *, int, int);
static void combine_color(i_color *, i_color *, int, int);
static void combine_colorf(i_fcolor *, i_fcolor *, int, int);

static struct i_combines {
  i_fill_combine_f combine;
  i_fill_combinef_f combinef;
} combines[] =
{
  { /* replace */
    combine_replace,
    combine_replacef,
  },
  { /* alpha blend */
    combine_alphablend,
    combine_alphablendf,
  },
  {
    /* multiply */
    combine_mult,
    combine_multf,
  },
  {
    /* dissolve */
    combine_dissolve,
    combine_dissolvef,
  },
  {
    /* add */
    combine_add,
    combine_addf,
  },
  {
    /* subtract */
    combine_subtract,
    combine_subtractf,
  },
  {
    /* diff */
    combine_diff,
    combine_difff,
  },
  {
    combine_lighten,
    combine_lightenf,
  },
  {
    combine_darken,
    combine_darkenf,
  },
  {
    combine_hue,
    combine_huef,
  },
  {
    combine_sat,
    combine_satf,
  },
  {
    combine_value,
    combine_valuef,
  },
  {
    combine_color,
    combine_colorf,
  },
};

/*
=item i_get_combine(combine, color_func, fcolor_func)

=cut
*/

void i_get_combine(int combine, i_fill_combine_f *color_func, 
                   i_fill_combinef_f *fcolor_func) {
  if (combine < 0 || combine > sizeof(combines) / sizeof(*combines))
    combine = 0;

  *color_func = combines[combine].combine;
  *fcolor_func = combines[combine].combinef;
}

static void combine_replace(i_color *out, i_color *in, int channels, int count) {
  while (count--) {
    *out++ = *in++;
  }
}

static void combine_replacef(i_fcolor *out, i_fcolor *in, int channels, int count) {
  while (count--) {
    *out++ = *in++;
  }
}

static void combine_alphablend(i_color *out, i_color *in, int channels, int count) {
  while (count--) {
    COMBINE(*out, *in, channels);
    ++out;
    ++in;
  }
}

static void combine_alphablendf(i_fcolor *out, i_fcolor *in, int channels, int count) {
  while (count--) {
    COMBINEF(*out, *in, channels);
    ++out;
    ++in;
  }
}

static void combine_mult(i_color *out, i_color *in, int channels, int count) {
  int ch;

  while (count--) {
    i_color c = *in;
    double mult[MAXCHANNELS];
    mult[3] = in->channel[3];
    for (ch = 0; ch < (channels); ++ch) { 
      if (ch != 3)
        mult[ch] = (out->channel[ch] * in->channel[ch]) * (1.0 / 255);
    } 
    COMBINEA(*out, mult, channels);
    ++out;
    ++in;
  }
}

static void combine_multf(i_fcolor *out, i_fcolor *in, int channels, int count) {
  int ch;

  while (count--) {
    i_fcolor c = *in;
    for (ch = 0; ch < channels; ++ch) { 
      if (ch != 3)
        c.channel[ch] = out->channel[ch] * in->channel[ch];
    } 
    COMBINEF(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_dissolve(i_color *out, i_color *in, int channels, int count) {
  int ch;

  while (count--) {
    if (in->channel[3] > rand() * (255.0 / RAND_MAX))
      COMBINE(*out, *in, channels);
    ++out;
    ++in;
  }
}

static void combine_dissolvef(i_fcolor *out, i_fcolor *in, int channels, int count) {
  int ch;

  while (count--) {
    if (in->channel[3] > rand() * (1.0 / RAND_MAX))
      COMBINEF(*out, *in, channels);
    ++out;
    ++in;
  }
}

static void combine_add(i_color *out, i_color *in, int channels, int count) {
  int ch;

  while (count--) {
    i_color c = *in;
    for (ch = 0; ch < (channels); ++ch) { 
      if (ch != 3) {
        int total = out->channel[ch] + in->channel[ch];
        if (total > 255)
          total = 255;
        c.channel[ch] = total;
      }
    } 
    COMBINE(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_addf(i_fcolor *out, i_fcolor *in, int channels, int count) {
  int ch;

  while (count--) {
    i_fcolor c = *in;
    for (ch = 0; ch < (channels); ++ch) { 
      if (ch != 3) {
        double total = out->channel[ch] + in->channel[ch];
        if (total > 1.0)
          total = 1.0;
        out->channel[ch] = total;
      }
    } 
    COMBINEF(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_subtract(i_color *out, i_color *in, int channels, int count) {
  int ch;

  while (count--) {
    i_color c = *in;
    for (ch = 0; ch < (channels); ++ch) { 
      if (ch != 3) {
        int total = out->channel[ch] - in->channel[ch];
        if (total < 0)
          total = 0;
        c.channel[ch] = total;
      }
    } 
    COMBINE(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_subtractf(i_fcolor *out, i_fcolor *in, int channels, int count) {
  int ch;

  while (count--) {
    i_fcolor c = *in;
    for (ch = 0; ch < channels; ++ch) { 
      if (ch != 3) {
        double total = out->channel[ch] - in->channel[ch];
        if (total < 0)
          total = 0;
        c.channel[ch] = total;
      }
    } 
    COMBINEF(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_diff(i_color *out, i_color *in, int channels, int count) {
  int ch;

  while (count--) {
    i_color c = *in;
    for (ch = 0; ch < (channels); ++ch) { 
      if (ch != 3) 
        c.channel[ch] = abs(out->channel[ch] - in->channel[ch]);
    } 
    COMBINE(*out, c, channels)
    ++out;
    ++in;
  }
}

static void combine_difff(i_fcolor *out, i_fcolor *in, int channels, int count) {
  int ch;

  while (count--) {
    i_fcolor c = *in;
    for (ch = 0; ch < (channels); ++ch) { 
      if (ch != 3)
        c.channel[ch] = fabs(out->channel[ch] - in->channel[ch]);
    }
    COMBINEF(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_darken(i_color *out, i_color *in, int channels, int count) {
  int ch;

  while (count--) {
    for (ch = 0; ch < channels; ++ch) { 
      if (ch != 3 && out->channel[ch] < in->channel[ch])
        in->channel[ch] = out->channel[ch];
    } 
    COMBINE(*out, *in, channels);
    ++out;
    ++in;
  }
}

static void combine_darkenf(i_fcolor *out, i_fcolor *in, int channels, int count) {
  int ch;

  while (count--) {
    for (ch = 0; ch < channels; ++ch) { 
      if (ch != 3 && out->channel[ch] < in->channel[ch])
        in->channel[ch] = out->channel[ch];
    } 
    COMBINEF(*out, *in, channels);
    ++out;
    ++in;
  }
}

static void combine_lighten(i_color *out, i_color *in, int channels, int count) {
  int ch;

  while (count--) {
    for (ch = 0; ch < channels; ++ch) { 
      if (ch != 3 && out->channel[ch] > in->channel[ch])
        in->channel[ch] = out->channel[ch];
    } 
    COMBINE(*out, *in, channels);
    ++out;
    ++in;
  }
}

static void combine_lightenf(i_fcolor *out, i_fcolor *in, int channels, int count) {
  int ch;

  while (count--) {
    for (ch = 0; ch < channels; ++ch) { 
      if (ch != 3 && out->channel[ch] > in->channel[ch])
        in->channel[ch] = out->channel[ch];
    } 
    COMBINEF(*out, *in, channels);
    ++out;
    ++in;
  }
}

static void combine_hue(i_color *out, i_color *in, int channels, int count) {
  while (count--) {
    i_color c = *out;
    i_rgb_to_hsv(&c);
    i_rgb_to_hsv(in);
    c.channel[0] = in->channel[0];
    i_hsv_to_rgb(&c);
    c.channel[3] = in->channel[3];
    COMBINE(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_huef(i_fcolor *out, i_fcolor *in, int channels, int count) {
  while (count--) {
    i_fcolor c = *out;
    i_rgb_to_hsvf(&c);
    i_rgb_to_hsvf(in);
    c.channel[0] = in->channel[0];
    i_hsv_to_rgbf(&c);
    c.channel[3] = in->channel[3];
    COMBINEF(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_sat(i_color *out, i_color *in, int channels, int count) {
  while (count--) {
    i_color c = *out;
    i_rgb_to_hsv(&c);
    i_rgb_to_hsv(in);
    c.channel[1] = in->channel[1];
    i_hsv_to_rgb(&c);
    c.channel[3] = in->channel[3];
    COMBINE(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_satf(i_fcolor *out, i_fcolor *in, int channels, int count) {
  while (count--) {
    i_fcolor c = *out;
    i_rgb_to_hsvf(&c);
    i_rgb_to_hsvf(in);
    c.channel[1] = in->channel[1];
    i_hsv_to_rgbf(&c);
    c.channel[3] = in->channel[3];
    COMBINEF(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_value(i_color *out, i_color *in, int channels, int count) {
  while (count--) {
    i_color c = *out;
    i_rgb_to_hsv(&c);
    i_rgb_to_hsv(in);
    c.channel[2] = in->channel[2];
    i_hsv_to_rgb(&c);
    c.channel[3] = in->channel[3];
    COMBINE(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_valuef(i_fcolor *out, i_fcolor *in, int channels, 
                           int count) {
  while (count--) {
    i_fcolor c = *out;
    i_rgb_to_hsvf(&c);
    i_rgb_to_hsvf(in);
    c.channel[2] = in->channel[2];
    i_hsv_to_rgbf(&c);
    c.channel[3] = in->channel[3];
    COMBINEF(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_color(i_color *out, i_color *in, int channels, int count) {
  while (count--) {
    i_color c = *out;
    i_rgb_to_hsv(&c);
    i_rgb_to_hsv(in);
    c.channel[0] = in->channel[0];
    c.channel[1] = in->channel[1];
    i_hsv_to_rgb(&c);
    c.channel[3] = in->channel[3];
    COMBINE(*out, c, channels);
    ++out;
    ++in;
  }
}

static void combine_colorf(i_fcolor *out, i_fcolor *in, int channels, 
                           int count) {
  while (count--) {
    i_fcolor c = *out;
    i_rgb_to_hsvf(&c);
    i_rgb_to_hsvf(in);
    c.channel[0] = in->channel[0];
    c.channel[1] = in->channel[1];
    i_hsv_to_rgbf(&c);
    c.channel[3] = in->channel[3];
    COMBINEF(*out, c, channels);
    ++out;
    ++in;
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
