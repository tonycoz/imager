#ifndef _IMAGE_H_
#define _IMAGE_H_

#include "imconfig.h"
#include "imio.h"
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

#include "imdatatypes.h"

undef_int i_has_format(char *frmt);

/* constructors and destructors */

i_color *ICL_new_internal(            unsigned char r,unsigned char g,unsigned char b,unsigned char a);
i_color *ICL_set_internal(i_color *cl,unsigned char r,unsigned char g,unsigned char b,unsigned char a);
void     ICL_info        (const i_color *cl);
void     ICL_DESTROY     (i_color *cl);
void     ICL_add         (i_color *dst, i_color *src, int ch);

extern i_fcolor *i_fcolor_new(double r, double g, double b, double a);
extern void i_fcolor_destroy(i_fcolor *cl);

extern void i_rgb_to_hsvf(i_fcolor *color);
extern void i_hsv_to_rgbf(i_fcolor *color);
extern void i_rgb_to_hsv(i_color *color);
extern void i_hsv_to_rgb(i_color *color);

i_img *IIM_new(int x,int y,int ch);
#define i_img_8_new IIM_new
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
i_img_dim i_img_get_width(i_img *im);
i_img_dim i_img_get_height(i_img *im);

/* Base functions */

extern int i_ppix(i_img *im,int x,int y, const i_color *val);
extern int i_gpix(i_img *im,int x,int y,i_color *val);
extern int i_ppixf(i_img *im,int x,int y, const i_fcolor *val);
extern int i_gpixf(i_img *im,int x,int y,i_fcolor *val);

#define i_ppix(im, x, y, val) (((im)->i_f_ppix)((im), (x), (y), (val)))
#define i_gpix(im, x, y, val) (((im)->i_f_gpix)((im), (x), (y), (val)))
#define i_ppixf(im, x, y, val) (((im)->i_f_ppixf)((im), (x), (y), (val)))
#define i_gpixf(im, x, y, val) (((im)->i_f_gpixf)((im), (x), (y), (val)))

extern int i_plin(i_img *im, int l, int r, int y, const i_color *vals);
extern int i_glin(i_img *im, int l, int r, int y, i_color *vals);
extern int i_plinf(i_img *im, int l, int r, int y, const i_fcolor *vals);
extern int i_glinf(i_img *im, int l, int r, int y, i_fcolor *vals);
extern int i_gsamp(i_img *im, int l, int r, int y, i_sample_t *samp, 
                   const int *chans, int chan_count);
extern int i_gsampf(i_img *im, int l, int r, int y, i_fsample_t *samp, 
                   const int *chans, int chan_count);
extern int i_gpal(i_img *im, int x, int r, int y, i_palidx *vals);
extern int i_ppal(i_img *im, int x, int r, int y, const i_palidx *vals);
extern int i_addcolors(i_img *im, const i_color *colors, int count);
extern int i_getcolors(i_img *im, int i, i_color *, int count);
extern int i_colorcount(i_img *im);
extern int i_maxcolors(i_img *im);
extern int i_findcolor(i_img *im, const i_color *color, i_palidx *entry);
extern int i_setcolors(i_img *im, int index, const i_color *colors, 
                       int count);

#define i_plin(im, l, r, y, val) (((im)->i_f_plin)(im, l, r, y, val))
#define i_glin(im, l, r, y, val) (((im)->i_f_glin)(im, l, r, y, val))
#define i_plinf(im, l, r, y, val) (((im)->i_f_plinf)(im, l, r, y, val))
#define i_glinf(im, l, r, y, val) (((im)->i_f_glinf)(im, l, r, y, val))

#define i_gsamp(im, l, r, y, samps, chans, count) \
  (((im)->i_f_gsamp)((im), (l), (r), (y), (samps), (chans), (count)))
#define i_gsampf(im, l, r, y, samps, chans, count) \
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

extern i_fill_t *i_new_fill_solidf(const i_fcolor *c, int combine);
extern i_fill_t *i_new_fill_solid(const i_color *c, int combine);
extern i_fill_t *
i_new_fill_hatch(const i_color *fg, const i_color *bg, int combine, int hatch, 
                 const unsigned char *cust_hatch, int dx, int dy);
extern i_fill_t *
i_new_fill_hatchf(const i_fcolor *fg, const i_fcolor *bg, int combine, int hatch, 
                  const unsigned char *cust_hatch, int dx, int dy);
extern i_fill_t *
i_new_fill_image(i_img *im, const double *matrix, int xoff, int yoff, int combine);
extern void i_fill_destroy(i_fill_t *fill);

float i_gpix_pch(i_img *im,int x,int y,int ch);

/* functions for drawing primitives */

void i_box         (i_img *im,int x1,int y1,int x2,int y2,const i_color *val);
void i_box_filled  (i_img *im,int x1,int y1,int x2,int y2,const i_color *val);
void i_box_cfill(i_img *im, int x1, int y1, int x2, int y2, i_fill_t *fill);
void i_line        (i_img *im,int x1,int y1,int x2,int y2,const i_color *val, int endp);
void i_line_aa     (i_img *im,int x1,int y1,int x2,int y2,const i_color *val, int endp);
void i_arc         (i_img *im,int x,int y,float rad,float d1,float d2,const i_color *val);
void i_arc_aa         (i_img *im, double x, double y, double rad, double d1, double d2, const i_color *val);
void i_arc_cfill(i_img *im,int x,int y,float rad,float d1,float d2,i_fill_t *fill);
void i_arc_aa_cfill(i_img *im,double x,double y,double rad,double d1,double d2,i_fill_t *fill);
void i_circle_aa   (i_img *im,float x, float y,float rad,const i_color *val);
void i_copyto      (i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty);
void i_copyto_trans(i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty,const i_color *trans);
i_img* i_copy        (i_img *src);
int  i_rubthru     (i_img *im, i_img *src, int tx, int ty, int src_minx, int src_miny, int src_maxx, int src_maxy);


undef_int i_flipxy (i_img *im, int direction);
extern i_img *i_rotate90(i_img *im, int degrees);
extern i_img *i_rotate_exact(i_img *im, double amount);
extern i_img *i_rotate_exact_bg(i_img *im, double amount, const i_color *backp, const i_fcolor *fbackp);
extern i_img *i_matrix_transform(i_img *im, int xsize, int ysize, const double *matrix);
extern i_img *i_matrix_transform_bg(i_img *im, int xsize, int ysize, const double *matrix,  const i_color *backp, const i_fcolor *fbackp);

void i_bezier_multi(i_img *im,int l,const double *x,const double *y,const i_color *val);
void i_poly_aa     (i_img *im,int l,const double *x,const double *y,const i_color *val);
void i_poly_aa_cfill(i_img *im,int l,const double *x,const double *y,i_fill_t *fill);

undef_int i_flood_fill  (i_img *im,int seedx,int seedy, const i_color *dcol);
undef_int i_flood_cfill(i_img *im, int seedx, int seedy, i_fill_t *fill);
undef_int i_flood_fill_border  (i_img *im,int seedx,int seedy, const i_color *dcol, const i_color *border);
undef_int i_flood_cfill_border(i_img *im, int seedx, int seedy, i_fill_t *fill, const i_color *border);


/* image processing functions */

int i_gaussian    (i_img *im, double stdev);
void i_conv        (i_img *im,const float *coeff,int len);
void i_unsharp_mask(i_img *im, double stddev, double scale);

/* colour manipulation */
extern i_img *i_convert(i_img *src, const float *coeff, int outchan, int inchan);
extern void i_map(i_img *im, unsigned char (*maps)[256], unsigned int mask);

float i_img_diff   (i_img *im1,i_img *im2);

/* font routines */

undef_int i_init_fonts( int t1log );

#ifdef HAVE_LIBT1

undef_int i_init_t1( int t1log );
int       i_t1_new( char *pfb, char *afm );
int       i_t1_destroy( int font_id );
undef_int i_t1_cp( i_img *im, int xb, int yb, int channel, int fontnum, float points, char* str, int len, int align, int utf8, char const *flags );
undef_int i_t1_text( i_img *im, int xb, int yb, const i_color *cl, int fontnum, float points, const char* str, int len, int align, int utf8, char const *flags );
int      i_t1_bbox( int fontnum, float point, const char *str, int len, int cords[6], int utf8, char const *flags );
void      i_t1_set_aa( int st );
void      close_t1( void );
int       i_t1_has_chars(int font_num, char const *text, int len, int utf8, char *out);
extern int i_t1_face_name(int font_num, char *name_buf, size_t name_buf_size);
extern int i_t1_glyph_name(int font_num, unsigned long ch, char *name_buf, 
                           size_t name_buf_size);
#endif

#ifdef HAVE_LIBTT

undef_int i_init_tt( void );
TT_Fonthandle* i_tt_new(const char *fontname);
void i_tt_destroy( TT_Fonthandle *handle );
undef_int i_tt_cp( TT_Fonthandle *handle,i_img *im,int xb,int yb,int channel,float points,char const* txt,int len,int smooth, int utf8, int align);
undef_int i_tt_text( TT_Fonthandle *handle, i_img *im, int xb, int yb, const i_color *cl, float points, char const* txt, int len, int smooth, int utf8, int align);
undef_int i_tt_bbox( TT_Fonthandle *handle, float points,const char *txt,int len,int cords[6], int utf8);
int i_tt_has_chars(TT_Fonthandle *handle, char const *text, int len, int utf8, char *out);
void i_tt_dump_names(TT_Fonthandle *handle);
int i_tt_face_name(TT_Fonthandle *handle, char *name_buf, 
                   size_t name_buf_size);
int i_tt_glyph_name(TT_Fonthandle *handle, unsigned long ch, char *name_buf,
                    size_t name_buf_size);

#endif  /* End of freetype headers */

#ifdef HAVE_FT2

extern int i_ft2_init(void);
extern FT2_Fonthandle * i_ft2_new(const char *name, int index);
extern void i_ft2_destroy(FT2_Fonthandle *handle);
extern int i_ft2_setdpi(FT2_Fonthandle *handle, int xdpi, int ydpi);
extern int i_ft2_getdpi(FT2_Fonthandle *handle, int *xdpi, int *ydpi);
extern int i_ft2_settransform(FT2_Fonthandle *handle, const double *matrix);
extern int i_ft2_sethinting(FT2_Fonthandle *handle, int hinting);
extern int i_ft2_bbox(FT2_Fonthandle *handle, double cheight, double cwidth, 
                      char const *text, int len, int *bbox, int utf8);
extern int i_ft2_bbox_r(FT2_Fonthandle *handle, double cheight, double cwidth, 
		      char const *text, int len, int vlayout, int utf8, int *bbox);
extern int i_ft2_text(FT2_Fonthandle *handle, i_img *im, int tx, int ty, 
                      const i_color *cl, double cheight, double cwidth, 
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
extern int i_ft2_can_face_name(void);
extern int i_ft2_glyph_name(FT2_Fonthandle *handle, unsigned long ch, 
                            char *name_buf, size_t name_buf_size,
                            int reliable_only);
extern int i_ft2_can_do_glyph_names(void);
extern int i_ft2_face_has_glyph_names(FT2_Fonthandle *handle);

extern int i_ft2_get_multiple_masters(FT2_Fonthandle *handle,
                                      i_font_mm *mm);
extern int
i_ft2_is_multiple_master(FT2_Fonthandle *handle);
extern int
i_ft2_set_mm_coords(FT2_Fonthandle *handle, int coord_count, const long *coords);
#endif

#ifdef WIN32

extern int i_wf_bbox(const char *face, int size, const char *text, int length, int *bbox, int utf8);
extern int i_wf_text(const char *face, i_img *im, int tx, int ty, const i_color *cl, 
		     int size, const char *text, int len, int align, int aa, int utf8);
extern int i_wf_cp(const char *face, i_img *im, int tx, int ty, int channel, 
		   int size, const char *text, int len, int align, int aa, int utf8);
extern int i_wf_addfont(char const *file);
extern int i_wf_delfont(char const *file);

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

extern void i_quant_makemap(i_quantize *quant, i_img **imgs, int count);
extern i_palidx *i_quant_translate(i_quantize *quant, i_img *img);
extern void i_quant_transparent(i_quantize *quant, i_palidx *indices, i_img *img, i_palidx trans_index);

extern i_img *i_img_pal_new(int x, int y, int channels, int maxpal);
extern i_img *i_img_pal_new_low(i_img *im, int x, int y, int channels, int maxpal);
extern i_img *i_img_to_pal(i_img *src, i_quantize *quant);
extern i_img *i_img_to_rgb(i_img *src);
extern i_img *i_img_masked_new(i_img *targ, i_img *mask, int x, int y, 
                               int w, int h);
extern i_img *i_img_16_new(int x, int y, int ch);
extern i_img *i_img_16_new_low(i_img *im, int x, int y, int ch);
extern i_img *i_img_to_rgb16(i_img *im);
extern i_img *i_img_double_new(int x, int y, int ch);
extern i_img *i_img_double_new_low(i_img *im, int x, int y, int ch);

extern int i_img_is_monochrome(i_img *im, int *zero_is_white);

const char * i_test_format_probe(io_glue *data, int length);


#ifdef HAVE_LIBJPEG
i_img *   
i_readjpeg_wiol(io_glue *ig, int length, char** iptc_itext, int *itlength);
undef_int i_writejpeg_wiol(i_img *im, io_glue *ig, int qfactor);
#endif /* HAVE_LIBJPEG */

#ifdef HAVE_LIBTIFF
i_img   * i_readtiff_wiol(io_glue *ig, int allow_incomplete, int page);
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
i_img *i_readgif_single_wiol(io_glue *ig, int page);
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

i_img   * i_readpnm_wiol(io_glue *ig, int allow_incomplete);
undef_int i_writeppm_wiol(i_img *im, io_glue *ig);

extern int    i_writebmp_wiol(i_img *im, io_glue *ig);
extern i_img *i_readbmp_wiol(io_glue *ig, int allow_incomplete);

int tga_header_verify(unsigned char headbuf[18]);

i_img   * i_readtga_wiol(io_glue *ig, int length);
undef_int i_writetga_wiol(i_img *img, io_glue *ig, int wierdpack, int compress, char *idstring, size_t idlen);

i_img   * i_readrgb_wiol(io_glue *ig, int length);
undef_int i_writergb_wiol(i_img *img, io_glue *ig, int wierdpack, int compress, char *idstring, size_t idlen);

i_img * i_scaleaxis(i_img *im, float Value, int Axis);
i_img * i_scale_nn(i_img *im, float scx, float scy);
i_img * i_scale_mixing(i_img *src, int width, int height);
i_img * i_haar(i_img *im);
int     i_count_colors(i_img *im,int maxc);
int i_get_anonymous_color_histo(i_img *im, unsigned int **col_usage, int maxc);

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
int i_nearest_color(i_img *im, int num, int *xo, int *yo, i_color *ival, int dmeasure);
i_img *i_diff_image(i_img *im, i_img *im2, double mindist);
int
i_fountain(i_img *im, double xa, double ya, double xb, double yb, 
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
  void (*ICL_info)(const i_color *cl);

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
  void(*i_box)(i_img *im,int x1,int y1,int x2,int y2,const i_color *val);
  void(*i_line)(i_img *im,int x1,int y1,int x2,int y2,const i_color *val,int endp);
  void(*i_arc)(i_img *im,int x,int y,float rad,float d1,float d2,const i_color *val);
  void(*i_copyto)(i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty);
  void(*i_copyto_trans)(i_img *im,i_img *src,int x1,int y1,int x2,int y2,int tx,int ty,const i_color *trans);
  int(*i_rubthru)(i_img *im,i_img *src,int tx,int ty, int src_minx, int src_miny, int src_maxx, int src_maxy);

} symbol_table_t;

#include "imerror.h"

/* image tag processing */
extern void i_tags_new(i_img_tags *tags);
extern int i_tags_addn(i_img_tags *tags, char const *name, int code, 
                       int idata);
extern int i_tags_add(i_img_tags *tags, char const *name, int code, 
                      char const *data, int size, int idata);
extern int i_tags_set(i_img_tags *tags, char const *name,
                      char const *data, int size);
extern int i_tags_setn(i_img_tags *tags, char const *name, int idata);
                       
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
extern int i_tags_set_float2(i_img_tags *tags, char const *name, int code, 
			    double value, int places);
extern int i_tags_get_int(i_img_tags *tags, char const *name, int code, 
                          int *value);
extern int i_tags_get_string(i_img_tags *tags, char const *name, int code, 
			     char *value, size_t value_size);
extern int i_tags_get_color(i_img_tags *tags, char const *name, int code, 
                            i_color *value);
extern int i_tags_set_color(i_img_tags *tags, char const *name, int code, 
                            i_color const *value);
extern void i_tags_print(i_img_tags *tags);

/* image file limits */
extern int
i_set_image_file_limits(int width, int height, int bytes);
extern int
i_get_image_file_limits(int *width, int *height, int *bytes);
extern int
i_int_check_image_file_limits(int width, int height, int channels, int sample_size);

/* memory allocation */
void* mymalloc(int size);
void  myfree(void *p);
void* myrealloc(void *p, size_t newsize);
void* mymalloc_file_line (size_t size, char* file, int line);
void  myfree_file_line   (void *p, char*file, int line);
void* myrealloc_file_line(void *p, size_t newsize, char* file,int line);

#ifdef IMAGER_DEBUG_MALLOC

#define mymalloc(x) (mymalloc_file_line((x), __FILE__, __LINE__))
#define myrealloc(x,y) (myrealloc_file_line((x),(y), __FILE__, __LINE__))
#define myfree(x) (myfree_file_line((x), __FILE__, __LINE__))

void  malloc_state       (void);
void* mymalloc_comm      (int size, char *comm);
void  bndcheck_all       (void);

#else

#define malloc_comm(a,b) (mymalloc(a))
void  malloc_state(void);

#endif /* IMAGER_MALLOC_DEBUG */

#include "imrender.h"

#endif
