#include "image.h"
#include "imagei.h"

/*

Possible fill types:
 - solid colour
 - hatched (pattern, fg, bg)
 - tiled image
 - regmach
 - tiling?
 - generic?

*/

static i_color fcolor_to_color(i_fcolor *c) {
  int ch;
  i_color out;

  for (ch = 0; ch < MAXCHANNELS; ++ch)
    out.channel[ch] = SampleFTo8(c->channel[ch]);
}

static i_fcolor color_to_fcolor(i_color *c) {
  int ch;
  i_color out;

  for (ch = 0; ch < MAXCHANNELS; ++ch)
    out.channel[ch] = Sample8ToF(c->channel[ch]);
}

typedef struct
{
  i_fill_t base;
  i_color c;
  i_fcolor fc;
} i_fill_solid_t;

#define COMBINE(out, in, channels) \
  { \
    int ch; \
    for (ch = 0; ch < (channels); ++ch) { \
      (out).channel[ch] = ((out).channel[ch] * (255 - (in).channel[3]) \
        + (in).channel[ch] * (in).channel[3]) / 255; \
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
    0
  },
};
static i_fill_solid_t base_solid_fill_comb =
{
  {
    fill_solid_comb,
    fill_solidf_comb,
    NULL,
    1
  },
};

void
i_fill_destroy(i_fill_t *fill) {
  if (fill->destroy)
    (fill->destroy)(fill);
  myfree(fill);
}

i_fill_t *
i_new_fill_solidf(i_fcolor *c, int combine) {
  int ch;
  i_fill_solid_t *fill = mymalloc(sizeof(i_fill_solid_t));
  
  if (combine && c->channel[3] < 1.0)
    *fill = base_solid_fill_comb;
  else
    *fill = base_solid_fill;
  fill->fc = *c;
  for (ch = 0; ch < MAXCHANNELS; ++ch) {
    fill->c.channel[ch] = SampleFTo8(c->channel[ch]);
  }
  
  return &fill->base;
}

i_fill_t *
i_new_fill_solid(i_color *c, int combine) {
  int ch;
  i_fill_solid_t *fill = mymalloc(sizeof(i_fill_solid_t));

  if (combine && c->channel[3] < 255)
    *fill = base_solid_fill_comb;
  else
    *fill = base_solid_fill;
  fill->c = *c;
  for (ch = 0; ch < MAXCHANNELS; ++ch) {
    fill->fc.channel[ch] = Sample8ToF(c->channel[ch]);
  }
  
  return &fill->base;
}

#define T_SOLID_FILL(fill) ((i_fill_solid_t *)(fill))

static void
fill_solid(i_fill_t *fill, int x, int y, int width, int channels, 
           i_color *data) {
  while (width-- > 0) {
    *data++ = T_SOLID_FILL(fill)->c;
  }
}

static void
fill_solidf(i_fill_t *fill, int x, int y, int width, int channels, 
           i_fcolor *data) {
  while (width-- > 0) {
    *data++ = T_SOLID_FILL(fill)->fc;
  }
}

static void
fill_solid_comb(i_fill_t *fill, int x, int y, int width, int channels, 
           i_color *data) {
  i_color c = T_SOLID_FILL(fill)->c;

  while (width-- > 0) {
    COMBINE(*data, c, channels);
    ++data;
  }
}

static void
fill_solidf_comb(i_fill_t *fill, int x, int y, int width, int channels, 
           i_fcolor *data) {
  i_fcolor c = T_SOLID_FILL(fill)->fc;

  while (width-- > 0) {
    COMBINEF(*data, c, channels);
    ++data;
  }
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
                int dx, int dy) {
  i_fill_hatch_t *fill = mymalloc(sizeof(i_fill_hatch_t));

  fill->base.fill_with_color = fill_hatch;
  fill->base.fill_with_fcolor = fill_hatchf;
  fill->base.destroy = NULL;
  fill->fg = fg ? *fg : fcolor_to_color(ffg);
  fill->bg = bg ? *bg : fcolor_to_color(fbg);
  fill->ffg = ffg ? *ffg : color_to_fcolor(fg);
  fill->fbg = fbg ? *fbg : color_to_fcolor(bg);
  fill->base.combines = 
    combine && (fill->ffg.channel[0] < 1 || fill->fbg.channel[0] < 1);
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

i_fill_t *
i_new_fill_hatch(i_color *fg, i_color *bg, int combine, int hatch, 
            unsigned char *cust_hatch, int dx, int dy) {
  return i_new_hatch_low(fg, bg, NULL, NULL, combine, hatch, cust_hatch, 
                         dx, dy);
}

i_fill_t *
i_new_fill_hatchf(i_fcolor *fg, i_fcolor *bg, int combine, int hatch, 
            unsigned char *cust_hatch, int dx, int dy) {
  return i_new_hatch_low(NULL, NULL, fg, bg, combine, hatch, cust_hatch, 
                         dx, dy);
}

static void fill_hatch(i_fill_t *fill, int x, int y, int width, int channels, 
                       i_color *data) {
  i_fill_hatch_t *f = (i_fill_hatch_t *)fill;
  int byte = f->hatch[(y + f->dy) & 7];
  int xpos = (x + f->dx) & 7;
  int mask = 128 >> xpos;

  while (width-- > 0) {
    i_color c = (byte & mask) ? f->fg : f->bg;

    if (f->base.combines) {
      COMBINE(*data, c, channels);
    }
    else {
      *data = c;
    }
    ++data;
    if ((mask >>= 1) == 0)
      mask = 128;
  }
}

static void fill_hatchf(i_fill_t *fill, int x, int y, int width, int channels, 
                        i_fcolor *data) {
  i_fill_hatch_t *f = (i_fill_hatch_t *)fill;
  int byte = f->hatch[(y + f->dy) & 7];
  int xpos = (x + f->dx) & 7;
  int mask = 128 >> xpos;
  
  while (width-- > 0) {
    i_fcolor c = (byte & mask) ? f->ffg : f->fbg;

    if (f->base.combines) {
      COMBINE(*data, c, channels);
    }
    else {
      *data = c;
    }
    ++data;
    if ((mask >>= 1) == 0)
      mask = 128;
  }
}
