/* Declares utility functions useful across various files which
   aren't meant to be available externally
*/

#ifndef IMAGEI_H_
#define IMAGEI_H_

#include "imager.h"

/* wrapper functions that implement the floating point sample version of a 
   function in terms of the 8-bit sample version
*/
extern int i_ppixf_fp(i_img *im, int x, int y, const i_fcolor *pix);
extern int i_gpixf_fp(i_img *im, int x, int y, i_fcolor *pix);
extern int i_plinf_fp(i_img *im, int l, int r, int y, const i_fcolor *pix);
extern int i_glinf_fp(i_img *im, int l, int r, int y, i_fcolor *pix);
extern int i_gsampf_fp(i_img *im, int l, int r, int y, i_fsample_t *samp,
                       int const *chans, int chan_count);

/* wrapper functions that forward palette calls to the underlying image,
   assuming the underlying image is the first pointer in whatever
   ext_data points at
*/
extern int i_addcolors_forward(i_img *im, const i_color *, int count);
extern int i_getcolors_forward(i_img *im, int i, i_color *, int count);
extern int i_colorcount_forward(i_img *im);
extern int i_maxcolors_forward(i_img *im);
extern int i_findcolor_forward(i_img *im, const i_color *color, 
			       i_palidx *entry);
extern int i_setcolors_forward(i_img *im, int index, const i_color *colors, 
                               int count);

#define SampleFTo16(num) ((int)((num) * 65535.0 + 0.01))
/* we add that little bit to avoid rounding issues */
#define Sample16ToF(num) ((num) / 65535.0)

#define SampleFTo8(num) ((int)((num) * 255.0 + 0.01))
#define Sample8ToF(num) ((num) / 255.0)

#define Sample16To8(num) ((num) / 257)
#define Sample8To16(num) ((num) * 257)

extern void i_get_combine(int combine, i_fill_combine_f *, i_fill_combinef_f *);

#define im_min(a, b) ((a) < (b) ? (a) : (b))
#define im_max(a, b) ((a) > (b) ? (a) : (b))

#include "ext.h"

extern UTIL_table_t i_UTIL_table;

/* Ideally this will move into imconfig.h if we ever probe */
#if defined(_GNU_SOURCE) || __STDC_VERSION__ >= 199901L
/* snprintf() is part of C99 and provided by Glibc */
#define HAVE_SNPRINTF
#endif

/* test if all channels are writable */
#define I_ALL_CHANNELS_WRITABLE(im) (((im)->ch_mask & 0xF) == 0xf)

typedef struct i_int_hline_seg_tag {
  int minx, x_limit;
} i_int_hline_seg;

typedef struct i_int_hline_entry_tag {
  int count;
  int alloc;
  i_int_hline_seg segs[1];
} i_int_hline_entry;

/* represents a set of horizontal line segments to be filled in later */
typedef struct i_int_hlines_tag {
  int start_y, limit_y;
  int start_x, limit_x;
  i_int_hline_entry **entries;
} i_int_hlines;

extern void 
i_int_init_hlines(
		  i_int_hlines *hlines, 
		  int start_y,
		  int count_y, 
		  int start_x, 
		  int width_x
		  );
extern void i_int_init_hlines_img(i_int_hlines *hlines, i_img *img);
extern void i_int_hlines_add(i_int_hlines *hlines, int y, int minx, int width);
extern void i_int_hlines_destroy(i_int_hlines *hlines);

extern void i_int_hlines_fill_color(i_img *im, i_int_hlines *hlines, const i_color *val);
extern void i_int_hlines_fill_fill(i_img *im, i_int_hlines *hlines, i_fill_t *fill);

#define I_LIMIT_8(x) ((x) < 0 ? 0 : (x) > 255 ? 255 : (x))
#define I_LIMIT_DOUBLE(x) ((x) < 0.0 ? 0.0 : (x) > 1.0 ? 1.0 : (x))

#endif
