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

extern i_fcolor *i_fcolor_new(double r, double g, double b, double a);
extern void i_fcolor_destroy(i_fcolor *cl);

extern void i_rgb_to_hsvf(i_fcolor *color);
extern void i_hsv_to_rgbf(i_fcolor *color);
extern void i_rgb_to_hsv(i_color *color);
extern void i_hsv_to_rgb(i_color *color);

i_img *IIM_new(int x,int y,int ch);
void   IIM_DESTROY(i_img *im);
i_img *i_img_new( void );
i_img *i_img_empty(i_img *im,int x,int y);
i_img *i_img_empty_ch(i_img *im,int x,int y,int ch);
void   i_img_exorcise(i_img *im);
void   i_img_destroy(i_img *im);

void   i_img_info(i_img *im,int *info);

extern i_img *i_sametype(i_img *im, int xsize, int ysize);
extern i_img *i_sametype_chans(i_img *im, int xsize, int ysize, int channels);

i_img *i_img_pal_new(int x, int y, int ch, int maxpal);

/* Image feature settings */

void   i_img_setmask    (i_img *im,int ch_mask);
int    i_img_getmask    (i_img *im);
int    i_img_getchannels(i_img *im);

/* Base functions */

#if 0
int i_ppix(i_img *im,int x,int y,i_color *val);
int i_gpix(i_img *im,int x,int y,i_color *val);
int i_ppixf(i_img *im,int x,int y,i_color *val);
int i_gpixf(i_img *im,int x,int y,i_color *val);
#endif

#define i_ppix(im, x, y, val) (((im)->i_f_ppix)((im), (x), (y), (val)))
#define i_gpix(im, x, y, val) (((im)->i_f_gpix)((im), (x), (y), (val)))
#define i_ppixf(im, x, y, val) (((im)->i_f_ppixf)((im), (x), (y), (val)))
#define i_gpixf(im, x, y, val) (((im)->i_f_gpixf)((im), (x), (y), (val)))

#if 0
int i_ppix_d(i_img *im,int x,int y,i_color *val);
int i_gpix_d(i_img *im,int x,int y,i_color *val);
int i_plin_d(i_img *im,int l, int r, int y, i_color *val);
int i_glin_d(i_img *im,int l, int r, int y, i_color *val);
#endif

#define i_plin(im, l, r, y, val) (((im)->i_f_plin)(im, l, r, y, val))
#define i_glin(im, l, r, y, val) (((im)->i_f_glin)(im, l, r, y, val))
#define i_plinf(im, l, r, y, val) (((im)->i_f_plinf)(im, l, r, y, val))
#define i_glinf(im, l, r, y, val) (((im)->i_f_glinf)(im, l, r, y, val))

#define i_gsamp(im, l, r, y, samps, chans, count) \
  (((im)->i_f_gsamp)((im), (l), (r), (y), (samps), (chans), (count)))
#define i_gsampf(im, l, r, y, samps, chans, count) \
  (((im)->i_f_gsampf)((im), (l), (r), (y), (samps), (chans), (count)))

#define i_psamp(im, l, r, y, samps, chans, count) \
  (((im)->i_f_gsamp)((im), (l), (r), (y), (samps), (chans), (count)))
#define i_psampf(im, l, r, y, samps, chans, count) \
  (((im)->i_f_gsampf)((im), (l), (r), (y), (samps), (chans), (count)))




#define i_findcolor(im, color, entry) \
  (((im)->i_f_findcolor) ? ((im)->i_f_findcolor)((im), (color), (entry)) : 0)

#define i_gpal(im, l, r, y, vals) \
  (((im)->i_f_gpal) ? ((im)->i_f_gpal)((im), (l), (r), (y), (vals)) : 0)
#define i_ppal(im, l, r, y, vals) \
  (((im)->i_f_ppal) ? ((im)->i_f_ppal)((im), (l), (r), (y), (vals)) : 0)
#define i_addcolors(im, colors, count) \
  (((im)->i_f_addcolors) ? ((im)->i_f_addcolors)((im), (colors), (count)) : -1)
#define i_getcolors(im, index, color, count) \
  (((im)->i_f_getcolors) ? \
   ((im)->i_f_getcolors)((im), (index), (color), (count)) : 0)
#define i_setcolors(im, index, color, count) \
  (((im)->i_f_setcolors) ? \
   ((im)->i_f_setcolors)((im), (index), (color), (count)) : 0)
#define i_colorcount(im) \
  (((im)->i_f_colorcount) ? ((im)->i_f_colorcount)(im) : -1)
#define i_maxcolors(im) \
  (((im)->i_f_maxcolors) ? ((im)->i_f_maxcolors)(im) : -1)
#define i_findcolor(im, color, entry) \
  (((im)->i_f_findcolor) ? ((im)->i_f_findcolor)((im), (color), (entry)) : 0)

#define i_img_virtual(im) ((im)->virtual)
#define i_img_type(im) ((im)->type)
#define i_img_bits(im) ((im)->bits)

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

extern i_fill_t *i_new_fill_solidf(i_fcolor *c, int combine);
extern i_fill_t *i_new_fill_solid(i_color *c, int combine);
extern i_fill_t *
i_new_fill_hatch(i_color *fg, i_color *bg, int combine, int hatch, 
                 unsigned char *cust_hatch, int dx, int dy);
extern i_fill_t *
i_new_fill_hatchf(i_fcolor *fg, i_fcolor *bg, int combine, int hatch, 
                  unsigned char *cust_hatch, int dx, int dy);
extern i_fill_t *
i_new_fill_image(i_img *im, double *matrix, int xoff, int yoff, int combine);
extern void i_fill_destroy(i_fill_t *fill);

float i_gpix_pch(i_img *im,int x,int y,int ch);

/* functions for drawing primitives */

void i_box         (i_img *im,int x1,int y1,int x2,int y2,i_color *val);
void i_box_filled  (i_img *im,int x1,int y1,int x2,int y2,i_color *val);
void i_box_cfill(i_img *im, int x1, int y1, int x2, int y2, i_fill_t *fill);
void i_line        (i_img *im,int x1,int y1,int x2,int y2,i_color *val, int endp);
void i_line_aa     (i_img *im,int x1,int y1,int x2,int y2,i_color *val, int endp);
void i_arc         (i_img *im,int x,int y,float rad,float d1,float d2,i_color *val);
void i_arc_cfill(i_img *im,int x,int y,float rad,float d1,float d2,i_fill_t *fill);
void i_circle_aa   (i_img *im,float x, float y,float rad,i_color *val);
void i_copyto      (i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty);
void i_copyto_trans(i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty,i_color *trans);
void i_copy        (i_img *im,i_img *src);
int  i_rubthru     (i_img *im, i_img *src, int tx, int ty, int src_minx, int src_miny, int src_maxx, int src_maxy);


undef_int i_flipxy (i_img *im, int direction);
extern i_img *i_rotate90(i_img *im, int degrees);
extern i_img *i_rotate_exact(i_img *im, double amount);
extern i_img *i_matrix_transform(i_img *im, int xsize, int ysize, double *matrix);

void i_bezier_multi(i_img *im,int l,double *x,double *y,i_color *val);
void i_poly_aa     (i_img *im,int l,double *x,double *y,i_color *val);
void i_poly_aa_cfill(i_img *im,int l,double *x,double *y,i_fill_t *fill);

undef_int i_flood_fill  (i_img *im,int seedx,int seedy,i_color *dcol);
undef_int i_flood_cfill(i_img *im, int seedx, int seedy, i_fill_t *fill);


/* image processing functions */

void i_gaussian    (i_img *im,float stdev);
void i_conv        (i_img *im,float *coeff,int len);
void i_unsharp_mask(i_img *im, double stddev, double scale);

/* colour manipulation */
extern int i_convert(i_img *im, i_img *src, float *coeff, int outchan, int inchan);
extern void i_map(i_img *im, unsigned char (*maps)[256], unsigned int mask);

float i_img_diff   (i_img *im1,i_img *im2);

/* font routines */

undef_int i_init_fonts( int t1log );

#ifdef HAVE_LIBT1
#include <t1lib.h>

undef_int i_init_t1( int t1log );
int       i_t1_new( char *pfb, char *afm );
int       i_t1_destroy( int font_id );
undef_int i_t1_cp( i_img *im, int xb, int yb, int channel, int fontnum, float points, char* str, int len, int align, int utf8, char const *flags );
undef_int i_t1_text( i_img *im, int xb, int yb, i_color *cl, int fontnum, float points, char* str, int len, int align, int utf8, char const *flags );
int      i_t1_bbox( int fontnum, float point, char *str, int len, int cords[6], int utf8, char const *flags );
void      i_t1_set_aa( int st );
void      close_t1( void );
int       i_t1_has_chars(int font_num, char const *text, int len, int utf8, char *out);
extern int i_t1_face_name(int font_num, char *name_buf, size_t name_buf_size);
extern int i_t1_glyph_name(int font_num, unsigned long ch, char *name_buf, 
                           size_t name_buf_size);
#endif

#ifdef HAVE_LIBTT

struct TT_Fonthandle_;

typedef struct TT_Fonthandle_ TT_Fonthandle;

undef_int i_init_tt( void );
TT_Fonthandle* i_tt_new(char *fontname);
void i_tt_destroy( TT_Fonthandle *handle );
undef_int i_tt_cp( TT_Fonthandle *handle,i_img *im,int xb,int yb,int channel,float points,char const* txt,int len,int smooth, int utf8);
undef_int i_tt_text( TT_Fonthandle *handle, i_img *im, int xb, int yb, i_color *cl, float points, char const* txt, int len, int smooth, int utf8);
undef_int i_tt_bbox( TT_Fonthandle *handle, float points,char *txt,int len,int cords[6], int utf8);
int i_tt_has_chars(TT_Fonthandle *handle, char const *text, int len, int utf8, char *out);
void i_tt_dump_names(TT_Fonthandle *handle);
int i_tt_face_name(TT_Fonthandle *handle, char *name_buf, 
                   size_t name_buf_size);
int i_tt_glyph_name(TT_Fonthandle *handle, unsigned long ch, char *name_buf,
                    size_t name_buf_size);

#endif  /* End of freetype headers */

#ifdef HAVE_FT2

typedef struct FT2_Fonthandle FT2_Fonthandle;
extern int i_ft2_init(void);
extern FT2_Fonthandle * i_ft2_new(char *name, int index);
extern void i_ft2_destroy(FT2_Fonthandle *handle);
extern int i_ft2_setdpi(FT2_Fonthandle *handle, int xdpi, int ydpi);
extern int i_ft2_getdpi(FT2_Fonthandle *handle, int *xdpi, int *ydpi);
extern int i_ft2_settransform(FT2_Fonthandle *handle, double *matrix);
extern int i_ft2_sethinting(FT2_Fonthandle *handle, int hinting);
extern int i_ft2_bbox(FT2_Fonthandle *handle, double cheight, double cwidth, 
                      char const *text, int len, int *bbox, int utf8);
extern int i_ft2_text(FT2_Fonthandle *handle, i_img *im, int tx, int ty, 
                      i_color *cl, double cheight, double cwidth, 
                      char const *text, int len, int align, int aa, 
                      int vlayout, int utf8);
extern int i_ft2_cp(FT2_Fonthandle *handle, i_img *im, int tx, int ty, 
                    int channel, double cheight, double cwidth, 
                    char const *text, int len, int align, int aa, int vlayout, 
                    int utf8);
extern int i_ft2_has_chars(FT2_Fonthandle *handle, char const *text, int len,
                           int utf8, char *work);
extern int i_ft2_face_name(FT2_Fonthandle *handle, char *name_buf, 
                           size_t name_buf_size);
extern int i_ft2_glyph_name(FT2_Fonthandle *handle, unsigned char ch, 
                            char *name_buf, size_t name_buf_size);
extern int i_ft2_can_do_glyph_names(void);
extern int i_ft2_face_has_glyph_names(FT2_Fonthandle *handle);

#endif

#ifdef WIN32

extern int i_wf_bbox(char *face, int size, char *text, int length, int *bbox);
extern int i_wf_text(char *face, i_img *im, int tx, int ty, i_color *cl, 
		     int size, char *text, int len, int align, int aa);
extern int i_wf_cp(char *face, i_img *im, int tx, int ty, int channel, 
		   int size, char *text, int len, int align, int aa);

#endif

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
extern void i_free_gen_read_data(i_gen_read_data *);

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
extern int i_free_gen_write_data(i_gen_write_data *, int flush);

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

extern void quant_makemap(i_quantize *quant, i_img **imgs, int count);
extern i_palidx *quant_translate(i_quantize *quant, i_img *img);
extern void quant_transparent(i_quantize *quant, i_palidx *indices, i_img *img, i_palidx trans_index);

extern i_img *i_img_pal_new(int x, int y, int channels, int maxpal);
extern i_img *i_img_pal_new_low(i_img *im, int x, int y, int channels, int maxpal);
extern i_img *i_img_to_pal(i_img *src, i_quantize *quant);
extern i_img *i_img_to_rgb(i_img *src);
extern i_img *i_img_masked_new(i_img *targ, i_img *mask, int x, int y, 
                               int w, int h);
extern i_img *i_img_16_new(int x, int y, int ch);
extern i_img *i_img_16_new_low(i_img *im, int x, int y, int ch);
extern i_img *i_img_double_new(int x, int y, int ch);
extern i_img *i_img_double_new_low(i_img *im, int x, int y, int ch);


char * i_test_format_probe(io_glue *data, int length);


#ifdef HAVE_LIBJPEG
i_img *   
i_readjpeg_wiol(io_glue *ig, int length, char** iptc_itext, int *itlength);
undef_int i_writejpeg_wiol(i_img *im, io_glue *ig, int qfactor);
#endif /* HAVE_LIBJPEG */

#ifdef HAVE_LIBTIFF
i_img   * i_readtiff_wiol(io_glue *ig, int length);
i_img  ** i_readtiff_multi_wiol(io_glue *ig, int length, int *count);
undef_int i_writetiff_wiol(i_img *im, io_glue *ig);
undef_int i_writetiff_multi_wiol(io_glue *ig, i_img **imgs, int count);
undef_int i_writetiff_wiol_faxable(i_img *im, io_glue *ig, int fine);
undef_int i_writetiff_multi_wiol_faxable(io_glue *ig, i_img **imgs, int count, int fine);

#endif /* HAVE_LIBTIFF */

#ifdef HAVE_LIBPNG
i_img    *i_readpng_wiol(io_glue *ig, int length);
undef_int i_writepng_wiol(i_img *im, io_glue *ig);
#endif /* HAVE_LIBPNG */

#ifdef HAVE_LIBGIF
i_img *i_readgif(int fd, int **colour_table, int *colours);
i_img *i_readgif_wiol(io_glue *ig, int **colour_table, int *colours);
i_img *i_readgif_scalar(char *data, int length, int **colour_table, int *colours);
i_img *i_readgif_callback(i_read_callback_t callback, char *userdata, int **colour_table, int *colours);
extern i_img **i_readgif_multi(int fd, int *count);
extern i_img **i_readgif_multi_scalar(char *data, int length, int *count);
extern i_img **i_readgif_multi_callback(i_read_callback_t callback, char *userdata, int *count);
extern i_img **i_readgif_multi_wiol(io_glue *ig, int *count);
undef_int i_writegif(i_img *im,int fd,int colors,int pixdev,int fixedlen,i_color fixed[]);
undef_int i_writegifmc(i_img *im,int fd,int colors);
undef_int i_writegifex(i_img *im,int fd);
undef_int i_writegif_gen(i_quantize *quant, int fd, i_img **imgs, int count);
undef_int i_writegif_callback(i_quantize *quant, i_write_callback_t cb, char *userdata, int maxbuffer, i_img **imgs, int count);
undef_int i_writegif_wiol(io_glue *ig, i_quantize *quant, 
                          i_img **imgs, int count);
void i_qdist(i_img *im);

#endif /* HAVE_LIBGIF */

i_img   * i_readraw_wiol(io_glue *ig, int x, int y, int datachannels, int storechannels, int intrl);
undef_int i_writeraw_wiol(i_img* im, io_glue *ig);

i_img   * i_readpnm_wiol(io_glue *ig, int length);
undef_int i_writeppm_wiol(i_img *im, io_glue *ig);

extern int    i_writebmp_wiol(i_img *im, io_glue *ig);
extern i_img *i_readbmp_wiol(io_glue *ig);

int tga_header_verify(unsigned char headbuf[18]);

i_img   * i_readtga_wiol(io_glue *ig, int length);
undef_int i_writetga_wiol(i_img *img, io_glue *ig, int wierdpack, int compress, char *idstring, size_t idlen);

i_img   * i_readrgb_wiol(io_glue *ig, int length);
undef_int i_writergb_wiol(i_img *img, io_glue *ig, int wierdpack, int compress, char *idstring, size_t idlen);

i_img * i_scaleaxis(i_img *im, float Value, int Axis);
i_img * i_scale_nn(i_img *im, float scx, float scy);
i_img * i_haar(i_img *im);
int     i_count_colors(i_img *im,int maxc);

i_img * i_transform(i_img *im, int *opx,int opxl,int *opy,int opyl,double parm[],int parmlen);

struct rm_op;
i_img * i_transform2(int width, int height, int channels,
		     struct rm_op *ops, int ops_count, 
		     double *n_regs, int n_regs_count, 
		     i_color *c_regs, int c_regs_count, 
		     i_img **in_imgs, int in_imgs_count);

/* filters */

void i_contrast(i_img *im, float intensity);
void i_hardinvert(i_img *im);
void i_noise(i_img *im, float amount, unsigned char type);
void i_bumpmap(i_img *im,i_img *bump,int channel,int light_x,int light_y,int strength);
void i_bumpmap_complex(i_img *im, i_img *bump, int channel, int tx, int ty, float Lx, float Ly, 
		       float Lz, float cd, float cs, float n, i_color *Ia, i_color *Il, i_color *Is);
void i_postlevels(i_img *im,int levels);
void i_mosaic(i_img *im,int size);
void i_watermark(i_img *im,i_img *wmark,int tx,int ty,int pixdiff);
void i_autolevels(i_img *im,float lsat,float usat,float skew);
void i_radnoise(i_img *im,int xo,int yo,float rscale,float ascale);
void i_turbnoise(i_img *im,float xo,float yo,float scale);
void i_gradgen(i_img *im, int num, int *xo, int *yo, i_color *ival, int dmeasure);
void i_nearest_color(i_img *im, int num, int *xo, int *yo, i_color *ival, int dmeasure);
i_img *i_diff_image(i_img *im, i_img *im2, int mindist);
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
void i_fountain(i_img *im, double xa, double ya, double xb, double yb, 
                i_fountain_type type, i_fountain_repeat repeat, 
                int combine, int super_sample, double ssample_param,
                int count, i_fountain_seg *segs);
extern i_fill_t *
i_new_fill_fount(double xa, double ya, double xb, double yb, 
                 i_fountain_type type, i_fountain_repeat repeat, 
                 int combine, int super_sample, double ssample_param, 
                 int count, i_fountain_seg *segs);

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
  
  /*
  int (*i_ppix)(i_img *im,int x,int y,i_color *val);
  int (*i_gpix)(i_img *im,int x,int y,i_color *val);
  */
  void(*i_box)(i_img *im,int x1,int y1,int x2,int y2,i_color *val);
  void(*i_line)(i_img *im,int x1,int y1,int x2,int y2,i_color *val,int endp);
  void(*i_arc)(i_img *im,int x,int y,float rad,float d1,float d2,i_color *val);
  void(*i_copyto)(i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty);
  void(*i_copyto_trans)(i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty,i_color *trans);
  int(*i_rubthru)(i_img *im,i_img *src,int tx,int ty);

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
extern i_errmsg *i_errors(void);

extern void i_push_error(int code, char const *msg);
extern void i_push_errorf(int code, char const *fmt, ...);
extern void i_push_errorvf(int code, char const *fmt, va_list);
extern void i_clear_error(void);
extern int i_failed(int code, char const *msg);

/* image tag processing */
extern void i_tags_new(i_img_tags *tags);
extern int i_tags_addn(i_img_tags *tags, char const *name, int code, 
                       int idata);
extern int i_tags_add(i_img_tags *tags, char const *name, int code, 
                      char const *data, int size, int idata);
extern void i_tags_destroy(i_img_tags *tags);
extern int i_tags_find(i_img_tags *tags, char const *name, int start, 
                       int *entry);
extern int i_tags_findn(i_img_tags *tags, int code, int start, int *entry);
extern int i_tags_delete(i_img_tags *tags, int entry);
extern int i_tags_delbyname(i_img_tags *tags, char const *name);
extern int i_tags_delbycode(i_img_tags *tags, int code);
extern int i_tags_get_float(i_img_tags *tags, char const *name, int code, 
			    double *value);
extern int i_tags_set_float(i_img_tags *tags, char const *name, int code, 
			    double value);
extern int i_tags_get_int(i_img_tags *tags, char const *name, int code, 
                          int *value);
extern int i_tags_get_string(i_img_tags *tags, char const *name, int code, 
			     char *value, size_t value_size);
extern int i_tags_get_color(i_img_tags *tags, char const *name, int code, 
                            i_color *value);
extern int i_tags_set_color(i_img_tags *tags, char const *name, int code, 
                            i_color const *value);
extern void i_tags_print(i_img_tags *tags);

#endif
