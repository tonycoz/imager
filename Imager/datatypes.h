#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#include "io.h"

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
typedef int (*i_f_ppix_t)(i_img *im, int x, int y, i_color *pix);
typedef int (*i_f_ppixf_t)(i_img *im, int x, int y, i_fcolor *pix);
typedef int (*i_f_plin_t)(i_img *im, int x, int r, int y, i_color *vals);
typedef int (*i_f_plinf_t)(i_img *im, int x, int r, int y, i_fcolor *vals);
typedef int (*i_f_gpix_t)(i_img *im, int x, int y, i_color *pix);
typedef int (*i_f_gpixf_t)(i_img *im, int x, int y, i_fcolor *pix);
typedef int (*i_f_glin_t)(i_img *im, int x, int r, int y, i_color *vals);
typedef int (*i_f_glinf_t)(i_img *im, int x, int r, int y, i_fcolor *vals);

typedef int (*i_f_gsamp_t)(i_img *im, int x, int r, int y, i_sample_t *samp,
                           const int *chans, int chan_count);
typedef int (*i_f_gsampf_t)(i_img *im, int x, int r, int y, i_fsample_t *samp,
                            const int *chan, int chan_count);

typedef int (*i_f_gpal_t)(i_img *im, int x, int r, int y, i_palidx *vals);
typedef int (*i_f_ppal_t)(i_img *im, int x, int r, int y, i_palidx *vals);
typedef int (*i_f_addcolors_t)(i_img *im, i_color *colors, int count);
typedef int (*i_f_getcolors_t)(i_img *im, int i, i_color *, int count);
typedef int (*i_f_colorcount_t)(i_img *im);
typedef int (*i_f_maxcolors_t)(i_img *im);
typedef int (*i_f_findcolor_t)(i_img *im, i_color *color, i_palidx *entry);
typedef int (*i_f_setcolors_t)(i_img *im, int index, i_color *colors, 
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
  BOUNDING_BOX_COUNT
};

#endif

