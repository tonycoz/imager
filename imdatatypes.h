#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#include "imio.h"
#include "imconfig.h"

#define MAXCHANNELS 4

/* used for palette indices in some internal code (which might be 
   exposed at some point
*/
typedef unsigned char i_palidx;

/* We handle 2 types of sample, this is hopefully the most common, and the
   smaller of the ones we support */
typedef unsigned char i_sample_t;

typedef struct { i_sample_t gray_color; } gray_color;
typedef struct { i_sample_t r,g,b; } rgb_color;
typedef struct { i_sample_t r,g,b,a; } rgba_color;
typedef struct { i_sample_t c,m,y,k; } cmyk_color;

typedef int undef_int; /* special value to put in typemaps to retun undef on 0 and 1 on 1 */

typedef union {
  gray_color gray;
  rgb_color rgb;
  rgba_color rgba;
  cmyk_color cmyk;
  i_sample_t channel[MAXCHANNELS];
  unsigned int ui;
} i_color;

/* this is the larger sample type, it should be able to accurately represent
   any sample size we use */
typedef double i_fsample_t;

typedef struct { i_fsample_t gray_color; } i_fgray_color_t;
typedef struct { i_fsample_t r, g, b; } i_frgb_color_t;
typedef struct { i_fsample_t r, g, b, a; } i_frgba_color_t;
typedef struct { i_fsample_t c, m, y, k; } i_fcmyk_color_t;

typedef union {
  i_fgray_color_t gray;
  i_frgb_color_t rgb;
  i_frgba_color_t rgba;
  i_fcmyk_color_t cmyk;
  i_fsample_t channel[MAXCHANNELS];
} i_fcolor;

typedef enum {
  i_direct_type, /* direct colour, keeps RGB values per pixel */
  i_palette_type /* keeps a palette index per pixel */
} i_img_type_t;

typedef enum { 
  /* bits per sample, not per pixel */
  /* a paletted image might have one bit per sample */
  i_8_bits = 8,
  i_16_bits = 16,
  i_double_bits = sizeof(double) * 8
} i_img_bits_t;

typedef struct {
  char *name; /* name of a given tag, might be NULL */
  int code; /* number of a given tag, -1 if it has no meaning */
  char *data; /* value of a given tag if it's not an int, may be NULL */
  int size; /* size of the data */
  int idata; /* value of a given tag if data is NULL */
} i_img_tag;

typedef struct {
  int count; /* how many tags have been set */
  int alloc; /* how many tags have been allocated for */
  i_img_tag *tags;
} i_img_tags;

typedef struct i_img_ i_img;
typedef int (*i_f_ppix_t)(i_img *im, int x, int y, const i_color *pix);
typedef int (*i_f_ppixf_t)(i_img *im, int x, int y, const i_fcolor *pix);
typedef int (*i_f_plin_t)(i_img *im, int x, int r, int y, const i_color *vals);
typedef int (*i_f_plinf_t)(i_img *im, int x, int r, int y, const i_fcolor *vals);
typedef int (*i_f_gpix_t)(i_img *im, int x, int y, i_color *pix);
typedef int (*i_f_gpixf_t)(i_img *im, int x, int y, i_fcolor *pix);
typedef int (*i_f_glin_t)(i_img *im, int x, int r, int y, i_color *vals);
typedef int (*i_f_glinf_t)(i_img *im, int x, int r, int y, i_fcolor *vals);

typedef int (*i_f_gsamp_t)(i_img *im, int x, int r, int y, i_sample_t *samp,
                           const int *chans, int chan_count);
typedef int (*i_f_gsampf_t)(i_img *im, int x, int r, int y, i_fsample_t *samp,
                            const int *chan, int chan_count);

typedef int (*i_f_gpal_t)(i_img *im, int x, int r, int y, i_palidx *vals);
typedef int (*i_f_ppal_t)(i_img *im, int x, int r, int y, const i_palidx *vals);
typedef int (*i_f_addcolors_t)(i_img *im, const i_color *colors, int count);
typedef int (*i_f_getcolors_t)(i_img *im, int i, i_color *, int count);
typedef int (*i_f_colorcount_t)(i_img *im);
typedef int (*i_f_maxcolors_t)(i_img *im);
typedef int (*i_f_findcolor_t)(i_img *im, const i_color *color, i_palidx *entry);
typedef int (*i_f_setcolors_t)(i_img *im, int index, const i_color *colors, 
                              int count);

typedef void (*i_f_destroy_t)(i_img *im);

struct i_img_ {
  int channels;
  int xsize,ysize,bytes;
  unsigned int ch_mask;
  i_img_bits_t bits;
  i_img_type_t type;
  int virtual; /* image might not keep any data, must use functions */
  unsigned char *idata; /* renamed to force inspection of existing code */
                        /* can be NULL if virtual is non-zero */
  i_img_tags tags;

  void *ext_data;

  /* interface functions */
  i_f_ppix_t i_f_ppix;
  i_f_ppixf_t i_f_ppixf;
  i_f_plin_t i_f_plin;
  i_f_plinf_t i_f_plinf;
  i_f_gpix_t i_f_gpix;
  i_f_gpixf_t i_f_gpixf;
  i_f_glin_t i_f_glin;
  i_f_glinf_t i_f_glinf;
  i_f_gsamp_t i_f_gsamp;
  i_f_gsampf_t i_f_gsampf;
  
  /* only valid for type == i_palette_type */
  i_f_gpal_t i_f_gpal;
  i_f_ppal_t i_f_ppal;
  i_f_addcolors_t i_f_addcolors;
  i_f_getcolors_t i_f_getcolors;
  i_f_colorcount_t i_f_colorcount;
  i_f_maxcolors_t i_f_maxcolors;
  i_f_findcolor_t i_f_findcolor;
  i_f_setcolors_t i_f_setcolors;

  i_f_destroy_t i_f_destroy;
};

/* ext_data for paletted images
 */
typedef struct {
  int count; /* amount of space used in palette (in entries) */
  int alloc; /* amount of space allocated for palette (in entries) */
  i_color *pal;
  int last_found;
} i_img_pal_ext;

/* Helper datatypes
  The types in here so far are:

  doubly linked bucket list - pretty efficient
  octtree - no idea about goodness
  
  needed: hashes.

*/





/* bitmap mask */

struct i_bitmap {
  int xsize,ysize;
  char *data;
};

struct i_bitmap* btm_new(int xsize,int ysize);
void btm_destroy(struct i_bitmap *btm);
int btm_test(struct i_bitmap *btm,int x,int y);
void btm_set(struct i_bitmap *btm,int x,int y);








/* Stack/Linked list */

struct llink {
  struct llink *p,*n;
  void *data;
  int fill;		/* Number used in this link */
};

struct llist {
  struct llink *h,*t;
  int multip;		/* # of copies in a single chain  */
  int ssize;		/* size of each small element     */
  int count;           /* number of elements on the list */
};


/* Links */

struct llink *llink_new( struct llink* p,int size );
int  llist_llink_push( struct llist *lst, struct llink *lnk, void *data );

/* Lists */

struct llist *llist_new( int multip, int ssize );
void llist_destroy( struct llist *l );
void llist_push( struct llist *l, void *data );
void llist_dump( struct llist *l );
int llist_pop( struct llist *l,void *data );




/* Octtree */

struct octt {
  struct octt *t[8]; 
  int cnt;
};

struct octt *octt_new(void);
int octt_add(struct octt *ct,unsigned char r,unsigned char g,unsigned char b);
void octt_dump(struct octt *ct);
void octt_count(struct octt *ct,int *tot,int max,int *overflow);
void octt_delete(struct octt *ct);

/* font bounding box results */
enum bounding_box_index_t {
  BBOX_NEG_WIDTH,
  BBOX_GLOBAL_DESCENT,
  BBOX_POS_WIDTH,
  BBOX_GLOBAL_ASCENT,
  BBOX_DESCENT,
  BBOX_ASCENT,
  BBOX_ADVANCE_WIDTH,
  BBOX_RIGHT_BEARING,
  BOUNDING_BOX_COUNT
};

/* Generic fills */
struct i_fill_tag;

typedef void (*i_fill_with_color_f)
     (struct i_fill_tag *fill, int x, int y, int width, int channels, 
      i_color *data);
typedef void (*i_fill_with_fcolor_f)
     (struct i_fill_tag *fill, int x, int y, int width, int channels,
      i_fcolor *data);
typedef void (*i_fill_destroy_f)(struct i_fill_tag *fill);
typedef void (*i_fill_combine_f)(i_color *out, i_color *in, int channels, 
                                 int count);
typedef void (*i_fill_combinef_f)(i_fcolor *out, i_fcolor *in, int channels,
                                  int count);

/* fountain fill types */
typedef enum {
  i_fst_linear,
  i_fst_curved,
  i_fst_sine,
  i_fst_sphere_up,
  i_fst_sphere_down,
  i_fst_end
} i_fountain_seg_type;
typedef enum {
  i_fc_direct,
  i_fc_hue_up,
  i_fc_hue_down,
  i_fc_end
} i_fountain_color;
typedef struct {
  double start, middle, end;
  i_fcolor c[2];
  i_fountain_seg_type type;
  i_fountain_color color;
} i_fountain_seg;
typedef enum {
  i_fr_none,
  i_fr_sawtooth,
  i_fr_triangle,
  i_fr_saw_both,
  i_fr_tri_both
} i_fountain_repeat;
typedef enum {
  i_ft_linear,
  i_ft_bilinear,
  i_ft_radial,
  i_ft_radial_square,
  i_ft_revolution,
  i_ft_conical,
  i_ft_end
} i_fountain_type;
typedef enum {
  i_fts_none,
  i_fts_grid,
  i_fts_random,
  i_fts_circle
} i_ft_supersample;


typedef struct i_fill_tag
{
  /* called for 8-bit/sample image (and maybe lower) */
  /* this may be NULL, if so call fill_with_fcolor */
  i_fill_with_color_f fill_with_color;

  /* called for other sample sizes */
  /* this must be non-NULL */
  i_fill_with_fcolor_f fill_with_fcolor;

  /* called if non-NULL to release any extra resources */
  i_fill_destroy_f destroy;

  /* if non-zero the caller will fill data with the original data
     from the image */
  i_fill_combine_f combine;
  i_fill_combinef_f combinef;
} i_fill_t;

typedef enum {
  ic_none,
  ic_normal,
  ic_multiply,
  ic_dissolve,
  ic_add,
  ic_subtract,
  ic_diff,
  ic_lighten,
  ic_darken,
  ic_hue,
  ic_sat,
  ic_value,
  ic_color
} i_combine_t;

/*
   describes an axis of a MM font.
   Modelled on FT2's FT_MM_Axis.
   It would be nice to have a default entry too, but FT2 
   doesn't support it.
*/
typedef struct i_font_mm_axis_tag {
  char const *name;
  int minimum;
  int maximum;
} i_font_mm_axis;

#define IM_FONT_MM_MAX_AXES 4

/* 
   multiple master information for a font, if any 
   modelled on FT2's FT_Multi_Master.
*/
typedef struct i_font_mm_tag {
  int num_axis;
  int num_designs; /* provided but not necessarily useful */
  i_font_mm_axis axis[IM_FONT_MM_MAX_AXES];
} i_font_mm;

#ifdef HAVE_LIBTT

struct TT_Fonthandle_;

typedef struct TT_Fonthandle_ TT_Fonthandle;

#endif

#ifdef HAVE_FT2

typedef struct FT2_Fonthandle FT2_Fonthandle;

#endif

/* transparency handling for quantized output */
typedef enum i_transp_tag {
  tr_none, /* ignore any alpha channel */
  tr_threshold, /* threshold the transparency - uses tr_threshold */
  tr_errdiff, /* error diffusion */
  tr_ordered /* an ordered dither */
} i_transp;

/* controls how we build the colour map */
typedef enum i_make_colors_tag {
  mc_none, /* user supplied colour map only */
  mc_web_map, /* Use the 216 colour web colour map */
  mc_addi, /* Addi's algorithm */
  mc_median_cut, /* median cut - similar to giflib, hopefully */
  mc_mask = 0xFF /* (mask for generator) */
} i_make_colors;

/* controls how we translate the colours */
typedef enum i_translate_tag {
  pt_giflib, /* get gif lib to do it (ignores make_colours) */
  pt_closest, /* just use the closest match within the hashbox */
  pt_perturb, /* randomly perturb the data - uses perturb_size*/
  pt_errdiff /* error diffusion dither - uses errdiff */
} i_translate;

/* Which error diffusion map to use */
typedef enum i_errdiff_tag {
  ed_floyd, /* floyd-steinberg */
  ed_jarvis, /* Jarvis, Judice and Ninke */
  ed_stucki, /* Stucki */
  ed_custom, /* the map found in ed_map|width|height|orig */
  ed_mask = 0xFF, /* mask to get the map */
  ed_bidir = 0x100 /* change direction for each row */
} i_errdiff;

/* which ordered dither map to use
   currently only available for transparency
   I don't know of a way to do ordered dither of an image against some 
   general palette
 */
typedef enum i_ord_dith_tag
{
  od_random, /* sort of random */
  od_dot8, /* large dot */
  od_dot4,
  od_hline,
  od_vline,
  od_slashline, /* / line dither */
  od_backline, /* \ line dither */
  od_tiny, /* small checkerbox */
  od_custom /* custom 8x8 map */
} i_ord_dith;

typedef struct i_gif_pos_tag {
  int x, y;
} i_gif_pos;

/* passed into i_writegif_gen() to control quantization */
typedef struct i_quantize_tag {
  /* how to handle transparency */
  i_transp transp;
  /* the threshold at which to make pixels opaque */
  int tr_threshold;
  i_errdiff tr_errdiff;
  i_ord_dith tr_orddith;
  unsigned char tr_custom[64];
  
  /* how to make the colour map */
  i_make_colors make_colors;

  /* any existing colours 
     mc_existing is an existing colour table
     mc_count is the number of existing colours
     mc_size is the total size of the array that mc_existing points
     at - this must be at least 256
  */
  i_color *mc_colors;
  int mc_size;
  int mc_count;

  /* how we translate the colours */
  i_translate translate;

  /* the error diffusion map to use if translate is mc_errdiff */
  i_errdiff errdiff;
  /* the following define the error diffusion values to use if 
     errdiff is ed_custom.  ed_orig is the column on the top row that
     represents the current 
  */
  int *ed_map;
  int ed_width, ed_height, ed_orig;

  /* the amount of perturbation to use for translate is mc_perturb */
  int perturb;
} i_quantize;

typedef struct i_gif_opts {
  /* each image has a local color map */
  int each_palette;

  /* images are interlaced */
  int interlace;

  /* time for which image is displayed 
   (in 1/100 seconds)
   default: 0
  */
  int delay_count;
  int *delays;

  /* user input flags 
     default: 0
   */
  int user_input_count;
  char *user_input_flags;

  /* disposal
     default: 0 */
  int disposal_count;
  char *disposal;

  /* this is added to the color table when we make an image transparent */
  i_color tran_color;

  /* image positions */
  int position_count;
  i_gif_pos *positions;

  /* Netscape loop extension - number of loops */
  int loop_count;

  /* should be eliminate unused colors? */
  int eliminate_unused;
} i_gif_opts;

/* distance measures used by some filters */
enum {
  i_dmeasure_euclidean = 0,
  i_dmeasure_euclidean_squared = 1,
  i_dmeasure_manhatten = 2,
  i_dmeasure_limit = 2,
};

#include "iolayert.h"

#endif

