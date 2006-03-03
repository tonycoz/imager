#ifndef IMAGER_IMEXTTYPES_H_
#define IMAGER_IMEXTTYPES_H_

/* keep this file simple - apidocs.perl parses it. */

#include "imdatatypes.h"

/*
 IMAGER_API_VERSION is similar to the version number in the third and
 fourth bytes of TIFF files - if it ever changes then the API has changed
 too much for any application to remain compatible.
*/
#define IMAGER_API_VERSION 1

/*
 IMAGER_API_LEVEL is the level of the structure.  New function pointers
 will always remain at the end (unless IMAGER_API_VERSION changes), and
 will result in an increment of IMAGER_API_LEVEL.
*/

#define IMAGER_API_LEVEL 1

typedef struct {
  int version;
  int level;

  /* IMAGER_API_LEVEL 1 functions */
  void * (*f_mymalloc)(int size);
  void (*f_myfree)(void *block);
  void * (*f_myrealloc)(void *block, size_t newsize);
  void* (*f_mymalloc_file_line)(size_t size, char* file, int line);
  void  (*f_myfree_file_line)(void *p, char*file, int line);
  void* (*f_myrealloc_file_line)(void *p, size_t newsize, char* file,int line);

  i_img *(*f_i_img_8_new)(int xsize, int ysize, int channels);
  i_img *(*f_i_img_16_new)(int xsize, int ysize, int channels);
  i_img *(*f_i_img_double_new)(int xsize, int ysize, int channels);
  i_img *(*f_i_img_pal_new)(int xsize, int ysize, int channels, int maxpal);
  void (*f_i_img_destroy)(i_img *im);
  i_img *(*f_i_sametype)(i_img *im, int xsize, int ysize);
  i_img *(*f_i_sametype_chans)(i_img *im, int xsize, int ysize, int channels);
  void (*f_i_img_info)(i_img *im, int *info);

  int (*f_i_ppix)(i_img *im, int x, int y, const i_color *val);
  int (*f_i_gpix)(i_img *im, int x, int y, i_color *val);
  int (*f_i_ppixf)(i_img *im, int x, int y, const i_fcolor *val);
  int (*f_i_gpixf)(i_img *im, int x, int y, i_fcolor *val);
  int (*f_i_plin)(i_img *im, int l, int r, int y, const i_color *vals);
  int (*f_i_glin)(i_img *im, int l, int r, int y, i_color *vals);
  int (*f_i_plinf)(i_img *im, int l, int r, int y, const i_fcolor *vals);
  int (*f_i_glinf)(i_img *im, int l, int r, int y, i_fcolor *vals);
  int (*f_i_gsamp)(i_img *im, int l, int r, int y, i_sample_t *samp, 
                   const int *chans, int chan_count);
  int (*f_i_gsampf)(i_img *im, int l, int r, int y, i_fsample_t *samp, 
                   const int *chans, int chan_count);
  int (*f_i_gpal)(i_img *im, int x, int r, int y, i_palidx *vals);
  int (*f_i_ppal)(i_img *im, int x, int r, int y, const i_palidx *vals);
  int (*f_i_addcolors)(i_img *im, const i_color *colors, int count);
  int (*f_i_getcolors)(i_img *im, int i, i_color *, int count);
  int (*f_i_colorcount)(i_img *im);
  int (*f_i_maxcolors)(i_img *im);
  int (*f_i_findcolor)(i_img *im, const i_color *color, i_palidx *entry);
  int (*f_i_setcolors)(i_img *im, int index, const i_color *colors, 
                       int count);

  i_fill_t *(*f_i_new_fill_solid)(const i_color *c, int combine);
  i_fill_t *(*f_i_new_fill_solidf)(const i_fcolor *c, int combine);

  i_fill_t *(*f_i_new_fill_hatch)(const i_color *fg, const i_color *bg, int combine, 
                                  int hatch, const unsigned char *cust_hatch, 
                                  int dx, int dy);
  i_fill_t *(*f_i_new_fill_hatchf)(const i_fcolor *fg, const i_fcolor *bg, int combine, 
                                  int hatch, const unsigned char *cust_hatch, 
                                  int dx, int dy);
  i_fill_t *(*f_i_new_fill_image)(i_img *im, const double *matrix, int xoff, 
                                int yoff, int combine);
  i_fill_t *(*f_i_new_fill_fount)(double xa, double ya, double xb, double yb, 
                 i_fountain_type type, i_fountain_repeat repeat, 
                 int combine, int super_sample, double ssample_param, 
                 int count, i_fountain_seg *segs);  

  void (*f_i_fill_destroy)(i_fill_t *fill);

  void (*f_i_quant_makemap)(i_quantize *quant, i_img **imgs, int count);
  i_palidx * (*f_i_quant_translate)(i_quantize *quant, i_img *img);
  void (*f_i_quant_transparent)(i_quantize *quant, i_palidx *indices, 
                                i_img *img, i_palidx trans_index);

  void (*f_i_clear_error)(void);
  void (*f_i_push_error)(int code, char const *msg);
  void (*f_i_push_errorf)(int code, char const *fmt, ...);
  void (*f_i_push_errorvf)(int code, char const *fmt, va_list);
  
  void (*f_i_tags_new)(i_img_tags *tags);
  int (*f_i_tags_set)(i_img_tags *tags, char const *name, char const *data, 
                      int size);
  int (*f_i_tags_setn)(i_img_tags *tags, char const *name, int idata);
  void (*f_i_tags_destroy)(i_img_tags *tags);
  int (*f_i_tags_find)(i_img_tags *tags, char const *name, int start, 
                       int *entry);
  int (*f_i_tags_findn)(i_img_tags *tags, int code, int start, int *entry);
  int (*f_i_tags_delete)(i_img_tags *tags, int entry);
  int (*f_i_tags_delbyname)(i_img_tags *tags, char const *name);
  int (*f_i_tags_delbycode)(i_img_tags *tags, int code);
  int (*f_i_tags_get_float)(i_img_tags *tags, char const *name, int code, 
                            double *value);
  int (*f_i_tags_set_float)(i_img_tags *tags, char const *name, int code, 
                            double value);
  int (*f_i_tags_set_float2)(i_img_tags *tags, char const *name, int code,
                             double value, int places);
  int (*f_i_tags_get_int)(i_img_tags *tags, char const *name, int code, 
                          int *value);
  int (*f_i_tags_get_string)(i_img_tags *tags, char const *name, int code,
                             char *value, size_t value_size);
  int (*f_i_tags_get_color)(i_img_tags *tags, char const *name, int code,
                            i_color *value);
  int (*f_i_tags_set_color)(i_img_tags *tags, char const *name, int code,
                            i_color const *value);

  void (*f_i_box)(i_img *im, int x1, int y1, int x2, int y2, const i_color *val);
  void (*f_i_box_filled)(i_img *im, int x1, int y1, int x2, int y2, const i_color *val);
  void (*f_i_box_cfill)(i_img *im, int x1, int y1, int x2, int y2, i_fill_t *fill);
  void (*f_i_line)(i_img *im, int x1, int y1, int x2, int y2, const i_color *val, int endp);
  void (*f_i_line_aa)(i_img *im, int x1, int y1, int x2, int y2, const i_color *val, int endp);
  void (*f_i_arc)(i_img *im, int x, int y, float rad, float d1, float d2, const i_color *val);
  void (*f_i_arc_aa)(i_img *im, double x, double y, double rad, double d1, double d2, const i_color *val);
  void (*f_i_arc_cfill)(i_img *im, int x, int y, float rad, float d1, float d2, i_fill_t *val);
  void (*f_i_arc_aa_cfill)(i_img *im, double x, double y, double rad, double d1, double d2, i_fill_t *fill);
  void (*f_i_circle_aa)(i_img *im, float x, float y, float rad, const i_color *val);
  int (*f_i_flood_fill)(i_img *im, int seedx, int seedy, const i_color *dcol);
  int (*f_i_flood_cfill)(i_img *im, int seedx, int seedy, i_fill_t *fill);

  void (*f_i_copyto)(i_img *im, i_img *src, int x1, int y1, int x2, int y2, int tx, int ty);
  void (*f_i_copyto_trans)(i_img *im, i_img *src, int x1, int y1, int x2, int y2, int tx, int ty, const i_color *trans);
  i_img *(*f_i_copy)(i_img *im);
  int (*f_i_rubthru)(i_img *im, i_img *src, int tx, int ty, int src_minx, int src_miny, int src_maxx, int src_maxy);

  /* IMAGER_API_LEVEL 2 functions will be added here */
} im_ext_funcs;

#define PERL_FUNCTION_TABLE_NAME "Imager::__ext_func_table"

#endif
