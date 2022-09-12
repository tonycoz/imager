/* Declares utility functions useful across various files which
   aren't meant to be available externally
*/

#ifndef IMAGERI_H_
#define IMAGERI_H_

#include "imager.h"
#include "imapiver.h"
#include <stddef.h>

#define SampleFTo16(num) ((int)((num) * 65535.0 + 0.5))
/* we add that little bit to avoid rounding issues */
#define Sample16ToF(num) ((num) / 65535.0)

#define SampleFTo8(num) ((int)((num) * 255.0 + 0.5))
#define Sample8ToF(num) ((num) / 255.0)

#define Sample16To8(num) (((num)+128) / 257)
#define Sample8To16(num) ((num) * 257)

extern void i_get_combine(int combine, i_fill_combine_f *, i_fill_combinef_f *);

#define im_min(a, b) ((a) < (b) ? (a) : (b))
#define im_max(a, b) ((a) > (b) ? (a) : (b))

#include "ext.h"

extern UTIL_table_t i_UTIL_table;

/* test if all channels are writable */
#define I_ALL_CHANNEL_MASK(im) ((1U << ((im)->channels + (im)->extrachannels))-1U)
#define I_ALL_CHANNELS_WRITABLE(im) (((im)->ch_mask & I_ALL_CHANNEL_MASK(im)) == I_ALL_CHANNEL_MASK(im))

typedef struct i_int_hline_seg_tag {
  i_img_dim minx, x_limit;
} i_int_hline_seg;

typedef struct i_int_hline_entry_tag {
  i_img_dim count;
  size_t alloc;
  i_int_hline_seg segs[1];
} i_int_hline_entry;

/* represents a set of horizontal line segments to be filled in later */
typedef struct i_int_hlines_tag {
  i_img_dim start_y, limit_y;
  i_img_dim start_x, limit_x;
  i_int_hline_entry **entries;
} i_int_hlines;

extern void 
i_int_init_hlines(
		  i_int_hlines *hlines, 
		  i_img_dim start_y,
		  i_img_dim count_y, 
		  i_img_dim start_x, 
		  i_img_dim width_x
		  );
extern void i_int_init_hlines_img(i_int_hlines *hlines, i_img *img);
extern void i_int_hlines_add(i_int_hlines *hlines, i_img_dim y, i_img_dim minx, i_img_dim width);
extern void i_int_hlines_destroy(i_int_hlines *hlines);

extern void i_int_hlines_fill_color(i_img *im, i_int_hlines *hlines, const i_color *val);
extern void i_int_hlines_fill_fill(i_img *im, i_int_hlines *hlines, i_fill_t *fill);

#define I_LIMIT_8(x) ((x) < 0 ? 0 : (x) > 255 ? 255 : (x))
#define I_LIMIT_DOUBLE(x) ((x) < 0.0 ? 0.0 : (x) > 1.0 ? 1.0 : (x))

#define IM_STRING(x) #x

/* I considered using assert.h here, but perl does it's own thing with 
   assert() and the NDEBUG test is opposite to the direction I prefer */
#ifdef IM_ASSERT
extern void im_assert_fail(char const *, int, char const *);
#define im_assert(x) ((x) ? (void)(0) : im_assert_fail(__FILE__, __LINE__, IM_STRING(x)))
#define IM_NOT_REACHED (im_assert_fail(__FILE__, __LINE__, "Not reachable"))
#else
#define im_assert(x) (void)(0)
#define IM_NOT_REACHED
#endif

#define im_assume(x) typedef struct { char x[x ? 1 : -1]; int y; } im_assume_ ## __LINE__

i_img_dim i_minx(i_img_dim a, i_img_dim b);
i_img_dim i_maxx(i_img_dim x, i_img_dim y);
i_img_dim i_abs(i_img_dim x);

#define i_min(a, b) i_minx((a), (b))
#define i_max(a, b) i_maxx((a), (b))

#define color_to_grey(col) ((col)->rgb.r * 0.222  + (col)->rgb.g * 0.707 + (col)->rgb.b * 0.071)

struct file_magic_entry {
  unsigned char *magic;
  size_t magic_size;
  char *name;
  unsigned char *mask;  
};


typedef struct im_file_magic im_file_magic;
struct im_file_magic {
  struct file_magic_entry m;

  /* more magic to check */
  im_file_magic *next;
};

#define IM_ERROR_COUNT 20
typedef struct im_context_tag {
  int error_sp;
  size_t error_alloc[IM_ERROR_COUNT];
  i_errmsg error_stack[IM_ERROR_COUNT];
#ifdef IMAGER_LOG
  /* the log file and level for this context */
  FILE *lg_file;
  int log_level;

  /* whether we own the lg_file, false for stderr and for cloned contexts */
  int own_log;

  /* values supplied by lhead */
  const char *filename;
  int line;
#endif

  /* file size limits */
  i_img_dim max_width, max_height;
  size_t max_bytes;

  /* per context storage */
  size_t slot_alloc;
  void **slots;

  /* registered file type magic */
  im_file_magic *file_magic;

  /* maximum size of i_io_mmap() */
  size_t max_mmap_size;

  /* color management */
  imcms_profile_t rgb_profile;
  imcms_profile_t gray_profile;
  imcms_curve_t rgb_curves[3];
  imcms_curve_t gray_curve;

  ptrdiff_t refcount;
} im_context_struct;

#define DEF_BYTES_LIMIT 0x40000000

#define im_size_t_max (~(size_t)0)

#endif
