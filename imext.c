#include "imexttypes.h"
#include "imager.h"
#include "imio.h"
#include "imexif.h"

static im_context_t get_context(void);
static i_img *mathom_i_img_8_new(i_img_dim, i_img_dim, int);
static i_img *mathom_i_img_pal_new(i_img_dim, i_img_dim, int, int);
static void mathom_i_clear_error(void);
static void mathom_i_push_error(int, const char *);
static void mathom_i_push_errorvf(int, const char *, va_list);
static int mathom_i_set_image_file_limits(i_img_dim, i_img_dim, size_t);
static int mathom_i_get_image_file_limits(i_img_dim*, i_img_dim*, size_t*);
static int
mathom_i_int_check_image_file_limits(i_img_dim, i_img_dim, int, size_t);
static i_img *mathom_i_img_alloc(void);
static void mathom_i_img_init(i_img *);
static i_io_glue_t *mathom_io_new_fd(int);
static i_io_glue_t *mathom_io_new_bufchain(void);
static i_io_glue_t *
mathom_io_new_buffer(const char *data, size_t, i_io_closebufp_t, void *);
static i_io_glue_t *
mathom_io_new_cb(void *, i_io_readl_t, i_io_writel_t, i_io_seekl_t,
		 i_io_closel_t, i_io_destroyl_t);

/*
 DON'T ADD CASTS TO THESE
*/
im_ext_funcs imager_function_table =
  {
    IMAGER_API_VERSION,
    IMAGER_API_LEVEL,

    mymalloc,
    myfree,
    myrealloc,

    mymalloc_file_line,
    myfree_file_line,
    myrealloc_file_line,

    mathom_i_img_pal_new,
    i_img_destroy,
    i_sametype,
    i_sametype_chans_extra,
    i_img_info,

    i_plin,
    i_glin,
    i_plinf,
    i_glinf,
    i_gsamp,
    i_gsampf,
    i_gpal,
    i_ppal,
    i_addcolors,
    i_getcolors,
    i_colorcount,
    i_maxcolors,
    i_findcolor,
    i_setcolors,

    i_new_fill_solid,
    i_new_fill_solidf,
    i_new_fill_hatch,
    i_new_fill_hatchf,
    i_new_fill_image,
    i_new_fill_fount,
    i_fill_destroy,

    i_quant_makemap,
    i_quant_translate,
    i_quant_transparent,

    mathom_i_clear_error,
    mathom_i_push_error,
    i_push_errorf,
    mathom_i_push_errorvf,

    i_tags_new,
    i_tags_set,
    i_tags_setn,
    i_tags_destroy,
    i_tags_find,
    i_tags_findn,
    i_tags_delete,
    i_tags_delbyname,
    i_tags_delbycode,
    i_tags_get_float,
    i_tags_set_float,
    i_tags_set_float2,
    i_tags_get_int,
    i_tags_get_string,
    i_tags_get_color,
    i_tags_set_color,

    i_box,
    i_box_filled,
    i_box_cfill,
    i_line,
    i_line_aa,
    i_arc,
    i_arc_aa,
    i_arc_cfill,
    i_arc_aa_cfill,
    i_circle_aa,
    i_flood_fill,
    i_flood_cfill,

    i_copyto,
    i_copyto_trans,
    i_copy,
    i_rubthru,

    /* IMAGER_API_LEVEL 2 functions */
    mathom_i_set_image_file_limits,
    mathom_i_get_image_file_limits,
    mathom_i_int_check_image_file_limits,

    i_flood_fill_border,
    i_flood_cfill_border,

    /* IMAGER_API_LEVEL 3 functions */
    i_img_setmask,
    i_img_getmask,
    i_lhead,
    i_loog,

    /* IMAGER_API_LEVEL 4 functions */
    mathom_i_img_alloc,
    mathom_i_img_init,

    /* IMAGER_API_LEVEL 5 functions */
    i_img_is_monochrome,
    i_gsamp_bg,
    i_gsampf_bg,
    i_get_file_background,
    i_get_file_backgroundf,
    i_utf8_advance,
    i_render_new,
    i_render_delete,
    i_render_color,
    i_render_fill,
    i_render_line,
    i_render_linef,

    /* level 6 */
    i_io_getc_imp,
    i_io_peekc_imp,
    i_io_peekn,
    i_io_putc_imp,
    i_io_read,
    i_io_write,
    i_io_seek,
    i_io_flush,
    i_io_close,
    i_io_set_buffered,
    i_io_gets,
    mathom_io_new_fd,
    mathom_io_new_bufchain,
    mathom_io_new_buffer,
    mathom_io_new_cb,
    io_slurp,
    io_glue_destroy,

    /* level 8 */
    im_img_pal_new,
    im_clear_error,
    im_push_error,
    im_push_errorvf,
    im_push_errorf,
    im_set_image_file_limits,
    im_get_image_file_limits,
    im_int_check_image_file_limits,
    im_img_alloc,
    im_img_init,
    im_io_new_fd,
    im_io_new_bufchain,
    im_io_new_buffer,
    im_io_new_cb,
    get_context,
    im_lhead,
    im_loog,
    im_context_refinc,
    im_context_refdec,
    im_errors,
    i_mutex_new,
    i_mutex_destroy,
    i_mutex_lock,
    i_mutex_unlock,
    im_context_slot_new,
    im_context_slot_set,
    im_context_slot_get,

    /* level 9 */
    i_poly_poly_aa,
    i_poly_poly_aa_cfill,
    i_poly_aa_m,
    i_poly_aa_cfill_m,

    /* level 10 */
    im_decode_exif,

    /* level 11 */
    im_io_set_max_mmap_size,
    im_io_get_max_mmap_size,
    i_img_refcnt_inc,
    i_img_check_entries,
    im_img_new,
    im_img_8_new_extra,
    im_img_16_new_extra,
    im_img_double_new_extra,
    i_new_image_data_alloc_def,
    i_new_image_data_alloc_free,
    myzmalloc,
    myzmalloc_file_line,
    i_gsampf_fp,
    i_psampf_fp,
    i_gsamp_bits_fb,
    i_psamp_bits_fb,
    i_img_data_fallback,
    i_addcolors_forward,
    i_getcolors_forward,
    i_setcolors_forward,
    i_colorcount_forward,
    i_maxcolors_forward,
    i_findcolor_forward,
    i_gslin_fallback,
    i_gslinf_fallback,
    i_pslin_fallback,
    i_pslinf_fallback,
    im_img_float_new_extra,
    im_lin_img_16_new_extra,
    im_lin_img_double_new_extra,
    im_fatal,
    im_aligned_alloc_low,
    im_aligned_free,
    im_model_curves
  };

/*
=item im_get_context()

Retrieve the context object for the current thread.

Inside Imager itself this is just a function pointer, which the
F<Imager.xs> BOOT handler initializes for use within perl.  If you're
taking the Imager code and embedding it elsewhere you need to
initialize the C<im_get_context> pointer at some point.

=cut
*/

static im_context_t
get_context(void) {
  return im_get_context();
}

static i_img *
mathom_i_img_8_new(i_img_dim xsize, i_img_dim ysize, int channels) {
  return i_img_8_new(xsize, ysize, channels);
}

static i_img *
mathom_i_img_double_new(i_img_dim xsize, i_img_dim ysize, int channels) {
  return i_img_double_new(xsize, ysize, channels);
}

static i_img *
mathom_i_img_pal_new(i_img_dim xsize, i_img_dim ysize, int channels,
		     int maxpal) {
  return i_img_pal_new(xsize, ysize, channels, maxpal);
}

static void
mathom_i_clear_error(void) {
  i_clear_error();
}

static void
mathom_i_push_error(int code, const char *msg) {
  i_push_error(code, msg);
}

static void
mathom_i_push_errorvf(int code, const char *fmt, va_list args) {
  i_push_errorvf(code, fmt, args);
}

static int
mathom_i_set_image_file_limits(i_img_dim max_width, i_img_dim max_height,
			       size_t max_bytes) {
  return i_set_image_file_limits(max_width, max_height, max_bytes);
}

static int
mathom_i_get_image_file_limits(i_img_dim *pmax_width, i_img_dim *pmax_height,
			       size_t *pmax_bytes) {
  return i_get_image_file_limits(pmax_width, pmax_height, pmax_bytes);
}

static int
mathom_i_int_check_image_file_limits(i_img_dim width, i_img_dim height,
				     int channels, size_t sample_size) {
  return i_int_check_image_file_limits(width, height, channels, sample_size);
}

static i_img *
mathom_i_img_alloc(void) {
  return i_img_alloc();
}

static void
mathom_i_img_init(i_img *im) {
  i_img_init(im);
}

static i_io_glue_t *
mathom_io_new_fd(int fd) {
  return io_new_fd(fd);
}
static i_io_glue_t *
mathom_io_new_bufchain(void) {
  return io_new_bufchain();
}

static i_io_glue_t *
mathom_io_new_buffer(const char *data, size_t size, i_io_closebufp_t closefp,
		     void *close_data) {
  return io_new_buffer(data, size, closefp, close_data);
}

static i_io_glue_t *
mathom_io_new_cb(void *p, i_io_readl_t readcb, i_io_writel_t writecb,
		 i_io_seekl_t seekcb, i_io_closel_t closecb,
		 i_io_destroyl_t destroycb) {
  return io_new_cb(p, readcb, writecb, seekcb, closecb, destroycb);
}
