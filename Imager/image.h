#ifndef _IMAGE_H_
#define _IMAGE_H_

#include "io.h"
#include "iolayer.h"
#include "log.h"
#include "stackmach.h"


#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#ifdef SUNOS
#include <strings.h>
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif

#ifndef MAXINT
#define MAXINT 2147483647
#endif

#include "datatypes.h"

undef_int i_has_format(char *frmt);

/* constructors and destructors */

i_color *ICL_new_internal(            unsigned char r,unsigned char g,unsigned char b,unsigned char a);
i_color *ICL_set_internal(i_color *cl,unsigned char r,unsigned char g,unsigned char b,unsigned char a);
void     ICL_info        (i_color *cl);
void     ICL_DESTROY     (i_color *cl);
void     ICL_add         (i_color *dst, i_color *src, int ch);

i_img *IIM_new(int x,int y,int ch);
void   IIM_DESTROY(i_img *im);
i_img *i_img_new( void );
i_img *i_img_empty(i_img *im,int x,int y);
i_img *i_img_empty_ch(i_img *im,int x,int y,int ch);
void   i_img_exorcise(i_img *im);
void   i_img_destroy(i_img *im);

void   i_img_info(i_img *im,int *info);

/* Image feature settings */

void   i_img_setmask    (i_img *im,int ch_mask);
int    i_img_getmask    (i_img *im);
int    i_img_getchannels(i_img *im);

/* Base functions */

int i_ppix(i_img *im,int x,int y,i_color *val);
int i_gpix(i_img *im,int x,int y,i_color *val);

int i_ppix_d(i_img *im,int x,int y,i_color *val);
int i_gpix_d(i_img *im,int x,int y,i_color *val);
int i_plin_d(i_img *im,int l, int r, int y, i_color *val);
int i_glin_d(i_img *im,int l, int r, int y, i_color *val);

#define i_plin(im, l, r, y, val) (((im)->i_f_plin)(im, l, r, y, val))
#define i_glin(im, l, r, y, val) (((im)->i_f_glin)(im, l, r, y, val))

float i_gpix_pch(i_img *im,int x,int y,int ch);

/* functions for drawing primitives */

void i_box         (i_img *im,int x1,int y1,int x2,int y2,i_color *val);
void i_box_filled  (i_img *im,int x1,int y1,int x2,int y2,i_color *val);
void i_draw        (i_img *im,int x1,int y1,int x2,int y2,i_color *val);
void i_line_aa     (i_img *im,int x1,int y1,int x2,int y2,i_color *val);
void i_arc         (i_img *im,int x,int y,float rad,float d1,float d2,i_color *val);
void i_copyto      (i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty);
void i_copyto_trans(i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty,i_color *trans);
void i_copy        (i_img *im,i_img *src);
void i_rubthru     (i_img *im,i_img *src,int tx,int ty);

undef_int i_flipxy (i_img *im, int direction);



void i_bezier_multi(i_img *im,int l,double *x,double *y,i_color *val);
void i_poly_aa     (i_img *im,int l,double *x,double *y,i_color *val);

void i_flood_fill  (i_img *im,int seedx,int seedy,i_color *dcol);

/* image processing functions */

void i_gaussian    (i_img *im,float stdev);
void i_conv        (i_img *im,float *coeff,int len);

/* colour manipulation */
extern int i_convert(i_img *im, i_img *src, float *coeff, int outchan, int inchan);

float i_img_diff   (i_img *im1,i_img *im2);

/* font routines */

undef_int i_init_fonts( void );

#ifdef HAVE_LIBT1
#include <t1lib.h>

undef_int init_t1( void );
int       i_t1_new( char *pfb, char *afm );
int       i_t1_destroy( int font_id );
undef_int i_t1_cp( i_img *im, int xb, int yb, int channel, int fontnum, float points, char* str, int len, int align );
undef_int i_t1_text( i_img *im, int xb, int yb, i_color *cl, int fontnum, float points, char* str, int len, int align );
void      i_t1_bbox( int fontnum, float point, char *str, int len, int cords[6] );
void      i_t1_set_aa( int st );
void      close_t1( void );

#endif

#ifdef HAVE_LIBTT

#include <freetype.h>
#define TT_CHC 5

struct TT_Instancehandle_ {
  TT_Instance instance;
  TT_Instance_Metrics imetrics;
  TT_Glyph_Metrics gmetrics[256];
  TT_Glyph glyphs[256];
  int smooth;
  int ptsize;
  int order;
};

typedef struct TT_Instancehandle_ TT_Instancehandle;

struct TT_Fonthandle_ {
  TT_Face face;
  TT_Face_Properties properties;
  TT_Instancehandle instanceh[TT_CHC];
  TT_CharMap char_map;
};

typedef struct TT_Fonthandle_ TT_Fonthandle;



undef_int init_tt( void );
TT_Fonthandle* i_tt_new(char *fontname);
void i_tt_destroy( TT_Fonthandle *handle );
undef_int i_tt_cp( TT_Fonthandle *handle,i_img *im,int xb,int yb,int channel,float points,char* txt,int len,int smooth);
undef_int i_tt_text( TT_Fonthandle *handle, i_img *im, int xb, int yb, i_color *cl, float points, char* txt, int len, int smooth);
undef_int i_tt_bbox( TT_Fonthandle *handle, float points,char *txt,int len,int cords[6]);


#endif  /* End of freetype headers */







/* functions for reading and writing formats */

/* general reader callback 
 userdata - data the user passed into the reader
 buffer - the buffer to fill with data
 need - the amount of data needed
 want - the amount of space we have to store data
 fill buffer and return the number of bytes read, 0 for eof, -1 for error
*/

typedef int (*i_read_callback_t)(char *userdata, char *buffer, int need, 
				 int want);

/* i_gen_reader() translates the low-level requests from whatever library
   into buffered requests.
   but the called function can always bypass buffering by only ever 
   reading I<need> bytes.
*/
#define CBBUFSIZ 4096

typedef struct {
  i_read_callback_t cb;
  char *userdata;
  char buffer[CBBUFSIZ];
  int length;
  int cpos;
} i_gen_read_data;

extern int  i_gen_reader(i_gen_read_data *info, char *buffer, int need);
extern      i_gen_read_data *i_gen_read_data_new(i_read_callback_t cb, char *userdata);
extern void free_gen_read_data(i_gen_read_data *);

/* general writer callback
   userdata - the data the user passed into the writer
   data - the data to write
   data_size - the number of bytes to write
   write the data, return non-zero on success, zero on failure.
*/
typedef int (*i_write_callback_t)(char *userdata, char const *data, int size);

typedef struct {
  i_write_callback_t cb;
  char *userdata;
  char buffer[CBBUFSIZ];
  int maxlength;
  int filledto;
} i_gen_write_data;

extern int i_gen_writer(i_gen_write_data *info, char const *data, int size);
extern i_gen_write_data *i_gen_write_data_new(i_write_callback_t cb, char *userdata, int maxlength);
extern int free_gen_write_data(i_gen_write_data *, int flush);

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
} i_gif_opts;

extern void quant_makemap(i_quantize *quant, i_img **imgs, int count);
extern i_palidx *quant_translate(i_quantize *quant, i_img *img);
extern void quant_transparent(i_quantize *quant, i_palidx *indices, i_img *img, i_palidx trans_index);

#ifdef HAVE_LIBJPEG
i_img* i_readjpeg(int fd,char** iptc_itext,int *tlength);
i_img* i_readjpeg_scalar(char *data, int length,char** iptc_itext,int *itlength);
i_img* i_readjpeg_wiol(io_glue *ig, int length, char** iptc_itext, int *itlength);

i_img* i_readjpeg_extra2(int fd,char** iptc_itext);
undef_int i_writejpeg(i_img *im,int fd,int qfactor);
#endif /* HAVE_LIBJPEG */

#ifdef HAVE_LIBTIFF
i_img* i_readtiff_wiol(io_glue *ig, int length);
undef_int i_writetiff_wiol(i_img *im, io_glue *ig);
undef_int i_writetiff_wiol_faxable(i_img *im, io_glue *ig, int fine);

#endif /* HAVE_LIBTIFF */

#ifdef HAVE_LIBPNG
i_img *i_readpng(int fd);
i_img *i_readpng_scalar(char *data, int length);
undef_int i_writepng(i_img *im,int fd);
#endif /* HAVE_LIBPNG */

#ifdef HAVE_LIBGIF
i_img *i_readgif(int fd, int **colour_table, int *colours);
i_img *i_readgif_scalar(char *data, int length, int **colour_table, int *colours);
i_img *i_readgif_callback(i_read_callback_t callback, char *userdata, int **colour_table, int *colours);
undef_int i_writegif(i_img *im,int fd,int colors,int pixdev,int fixedlen,i_color fixed[]);
undef_int i_writegifmc(i_img *im,int fd,int colors);
undef_int i_writegifex(i_img *im,int fd);
undef_int i_writegif_gen(i_quantize *quant, int fd, i_img **imgs, int count, i_gif_opts *opts);
undef_int i_writegif_callback(i_quantize *quant, i_write_callback_t cb, char *userdata, int maxbuffer, i_img **imgs, int count, i_gif_opts *opts);

void i_qdist(i_img *im);

#endif /* HAVE_LIBGIF */

i_img *i_readraw(int fd,int x,int y,int datachannels,int storechannels,int intrl);
undef_int i_writeraw(i_img* im,int fd);

i_img *i_readpnm_wiol(io_glue *ig, int length);
undef_int i_writeppm(i_img *im,int fd);


i_img* i_scaleaxis(i_img *im, float Value, int Axis);
i_img* i_scale_nn(i_img *im, float scx, float scy);
i_img* i_haar(i_img *im);
int i_count_colors(i_img *im,int maxc);

i_img* i_transform(i_img *im, int *opx,int opxl,int *opy,int opyl,double parm[],int parmlen);

struct rm_op;
i_img* i_transform2(int width, int height, int channels,
		    struct rm_op *ops, int ops_count, 
		    double *n_regs, int n_regs_count, 
		    i_color *c_regs, int c_regs_count, 
		    i_img **in_imgs, int in_imgs_count);
/* filters */

void i_contrast(i_img *im, float intensity);
void i_hardinvert(i_img *im);
void i_noise(i_img *im, float amount, unsigned char type);
void i_bumpmap(i_img *im,i_img *bump,int channel,int light_x,int light_y,int strength);
void i_postlevels(i_img *im,int levels);
void i_mosaic(i_img *im,int size);
void i_watermark(i_img *im,i_img *wmark,int tx,int ty,int pixdiff);
void i_autolevels(i_img *im,float lsat,float usat,float skew);
void i_radnoise(i_img *im,int xo,int yo,float rscale,float ascale);
void i_turbnoise(i_img *im,float xo,float yo,float scale);
void i_gradgen(i_img *im, int num, int *xo, int *yo, i_color *ival, int dmeasure);
void i_nearest_color(i_img *im, int num, int *xo, int *yo, i_color *ival, int dmeasure);

/* Debug only functions */

void malloc_state( void );

/* this is sort of obsolete now */

typedef struct {
  undef_int (*i_has_format)(char *frmt);
  i_color*(*ICL_set)(i_color *cl,unsigned char r,unsigned char g,unsigned char b,unsigned char a);
  void (*ICL_info)(i_color *cl);

  i_img*(*i_img_new)( void );
  i_img*(*i_img_empty)(i_img *im,int x,int y);
  i_img*(*i_img_empty_ch)(i_img *im,int x,int y,int ch);
  void(*i_img_exorcise)(i_img *im);

  void(*i_img_info)(i_img *im,int *info);
  
  void(*i_img_setmask)(i_img *im,int ch_mask);
  int (*i_img_getmask)(i_img *im);
  
  int (*i_ppix)(i_img *im,int x,int y,i_color *val);
  int (*i_gpix)(i_img *im,int x,int y,i_color *val);

  void(*i_box)(i_img *im,int x1,int y1,int x2,int y2,i_color *val);
  void(*i_draw)(i_img *im,int x1,int y1,int x2,int y2,i_color *val);
  void(*i_arc)(i_img *im,int x,int y,float rad,float d1,float d2,i_color *val);
  void(*i_copyto)(i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty);
  void(*i_copyto_trans)(i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty,i_color *trans);
  void(*i_rubthru)(i_img *im,i_img *src,int tx,int ty);

} symbol_table_t;

/* error handling 
   see error.c for documentation
   the error information is currently global
*/
typedef struct {
  char *msg;
  int code;
} i_errmsg;

typedef void (*i_error_cb)(int code, char const *msg);
typedef void (*i_failed_cb)(i_errmsg *msgs);
extern i_error_cb i_set_error_cb(i_error_cb);
extern i_failed_cb i_set_failed_cb(i_failed_cb);
extern void i_set_argv0(char const *);
extern int i_set_errors_fatal(int new_fatal);
extern i_errmsg *i_errors();

extern void i_push_error(int code, char const *msg);
extern void i_push_errorf(int code, char const *fmt, ...);
extern void i_push_errorvf(int code, char const *fmt, va_list);
extern void i_clear_error();
extern int i_failed(int code, char const *msg);


#endif
