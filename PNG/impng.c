#include "impng.h"
#include "png.h"

/* this is a way to get number of channels from color space 
 * Color code to channel number */

static int CC2C[PNG_COLOR_MASK_PALETTE|PNG_COLOR_MASK_COLOR|PNG_COLOR_MASK_ALPHA];

#define PNG_BYTES_TO_CHECK 4

static i_img *
read_direct8(png_structp png_ptr, png_infop info_ptr, int channels, i_img_dim width, i_img_dim height);

unsigned
i_png_lib_version(void) {
  return png_access_version_number();
}

static void
wiol_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
  io_glue *ig = png_get_io_ptr(png_ptr);
  ssize_t rc = i_io_read(ig, data, length);
  if (rc != length) png_error(png_ptr, "Read overflow error on an iolayer source.");
}

static void
wiol_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
  ssize_t rc;
  io_glue *ig = png_get_io_ptr(png_ptr);
  rc = i_io_write(ig, data, length);
  if (rc != length) png_error(png_ptr, "Write error on an iolayer source.");
}

static void
wiol_flush_data(png_structp png_ptr) {
  /* XXX : This needs to be added to the io layer */
}

static void
error_handler(png_structp png_ptr, png_const_charp msg) {
  mm_log((1, "PNG error: '%s'\n", msg));

  i_push_error(0, msg);
  longjmp(png_jmpbuf(png_ptr), 1);
}

/*

For writing a warning might have information about an error, so send
it to the error stack.

*/
static void
write_warn_handler(png_structp png_ptr, png_const_charp msg) {
  mm_log((1, "PNG write warning '%s'\n", msg));

  i_push_error(0, msg);
}

#define PNG_DIM_MAX 0x7fffffffL

undef_int
i_writepng_wiol(i_img *im, io_glue *ig) {
  png_structp png_ptr;
  png_infop info_ptr = NULL;
  i_img_dim width,height,y;
  volatile int cspace,channels;
  double xres, yres;
  int aspect_only, have_res;
  unsigned char *data;
  unsigned char * volatile vdata = NULL;

  mm_log((1,"i_writepng(im %p ,ig %p)\n", im, ig));

  i_clear_error();

  if (im->xsize > PNG_UINT_31_MAX || im->ysize > PNG_UINT_31_MAX) {
    i_push_error(0, "image too large for PNG");
    return 0;
  }

  height = im->ysize;
  width  = im->xsize;

  /* if we ever have 64-bit i_img_dim
   * the libpng docs state that png_set_user_limits() can be used to
   * override the PNG_USER_*_MAX limits, but as implemented they
   * don't.  We check against the theoretical limit of PNG here, and
   * try to override the limits below, in case the libpng
   * implementation ever matches the documentation.
   *
   * https://sourceforge.net/tracker/?func=detail&atid=105624&aid=3314943&group_id=5624
   * fixed in libpng 1.5.3
   */
  if (width > PNG_DIM_MAX || height > PNG_DIM_MAX) {
    i_push_error(0, "Image too large for PNG");
    return 0;
  }

  channels=im->channels;

  if (channels > 2) { cspace = PNG_COLOR_TYPE_RGB; channels-=3; }
  else { cspace=PNG_COLOR_TYPE_GRAY; channels--; }
  
  if (channels) cspace|=PNG_COLOR_MASK_ALPHA;
  mm_log((1,"cspace=%d\n",cspace));

  channels = im->channels;

  /* Create and initialize the png_struct with the desired error handler
   * functions.  If you want to use the default stderr and longjump method,
   * you can supply NULL for the last three parameters.  We also check that
   * the library version is compatible with the one used at compile time,
   * in case we are using dynamically linked libraries.  REQUIRED.
   */
  
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, 
				    error_handler, write_warn_handler);
  
  if (png_ptr == NULL) return 0;

  
  /* Allocate/initialize the image information data.  REQUIRED */
  info_ptr = png_create_info_struct(png_ptr);

  if (info_ptr == NULL) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return 0;
  }
  
  /* Set error handling.  REQUIRED if you aren't supplying your own
   * error hadnling functions in the png_create_write_struct() call.
   */
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    if (vdata)
      myfree(vdata);
    return(0);
  }
  
  png_set_write_fn(png_ptr, (png_voidp) (ig), wiol_write_data, wiol_flush_data);

  /* Set the image information here.  Width and height are up to 2^31,
   * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
   * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
   * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
   * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
   * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
   * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
   */

  /* by default, libpng (not PNG) limits the image size to a maximum
   * 1000000 pixels in each direction, but Imager doesn't.
   * Configure libpng to avoid that limit.
   */
  png_set_user_limits(png_ptr, width, height);

  png_set_IHDR(png_ptr, info_ptr, width, height, 8, cspace,
	       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  have_res = 1;
  if (i_tags_get_float(&im->tags, "i_xres", 0, &xres)) {
    if (i_tags_get_float(&im->tags, "i_yres", 0, &yres))
      ; /* nothing to do */
    else
      yres = xres;
  }
  else {
    if (i_tags_get_float(&im->tags, "i_yres", 0, &yres))
      xres = yres;
    else
      have_res = 0;
  }
  if (have_res) {
    aspect_only = 0;
    i_tags_get_int(&im->tags, "i_aspect_only", 0, &aspect_only);
    xres /= 0.0254;
    yres /= 0.0254;
    png_set_pHYs(png_ptr, info_ptr, xres + 0.5, yres + 0.5, 
                 aspect_only ? PNG_RESOLUTION_UNKNOWN : PNG_RESOLUTION_METER);
  }

  png_write_info(png_ptr, info_ptr);

  vdata = data = mymalloc(im->xsize * im->channels);
  for (y = 0; y < height; y++) {
    i_gsamp(im, 0, im->xsize, y, data, NULL, im->channels);
    png_write_row(png_ptr, (png_bytep)data);
  }
  myfree(data);

  png_write_end(png_ptr, info_ptr);

  png_destroy_write_struct(&png_ptr, &info_ptr);

  if (i_io_close(ig))
    return 0;

  return(1);
}

static void 
get_png_tags(i_img *im, png_structp png_ptr, png_infop info_ptr);

typedef struct {
  char *warnings;
} i_png_read_state, *i_png_read_statep;

static void
read_warn_handler(png_structp, png_const_charp);

static void
cleanup_read_state(i_png_read_statep);

i_img*
i_readpng_wiol(io_glue *ig) {
  i_img *im = NULL;
  png_structp png_ptr;
  png_infop info_ptr;
  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type;
  int number_passes,y;
  int channels,pass;
  unsigned int sig_read;
  i_png_read_state rs;
  i_img_dim wmax, hmax;
  size_t bytes;

  rs.warnings = NULL;
  sig_read  = 0;

  mm_log((1,"i_readpng_wiol(ig %p)\n", ig));
  i_clear_error();

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, &rs, 
				   error_handler, read_warn_handler);
  if (!png_ptr) {
    i_push_error(0, "Cannot create PNG read structure");
    return NULL;
  }
  png_set_read_fn(png_ptr, (png_voidp) (ig), wiol_read_data);
  
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    i_push_error(0, "Cannot create PNG info structure");
    return NULL;
  }
  
  if (setjmp(png_jmpbuf(png_ptr))) {
    if (im) i_img_destroy(im);
    mm_log((1,"i_readpng_wiol: error.\n"));
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    cleanup_read_state(&rs);
    return NULL;
  }
  
  /* we do our own limit checks */
  png_set_user_limits(png_ptr, PNG_DIM_MAX, PNG_DIM_MAX);

  png_set_sig_bytes(png_ptr, sig_read);
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  
  mm_log((1,
	  "png_get_IHDR results: width %d, height %d, bit_depth %d, color_type %d, interlace_type %d\n",
	  width,height,bit_depth,color_type,interlace_type));
  
  CC2C[PNG_COLOR_TYPE_GRAY]=1;
  CC2C[PNG_COLOR_TYPE_PALETTE]=3;
  CC2C[PNG_COLOR_TYPE_RGB]=3;
  CC2C[PNG_COLOR_TYPE_RGB_ALPHA]=4;
  CC2C[PNG_COLOR_TYPE_GRAY_ALPHA]=2;
  channels = CC2C[color_type];

  mm_log((1,"i_readpng_wiol: channels %d\n",channels));

  if (!i_int_check_image_file_limits(width, height, channels, sizeof(i_sample_t))) {
    mm_log((1, "i_readpnm: image size exceeds limits\n"));
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    return NULL;
  }

  im = read_direct8(png_ptr, info_ptr, channels, width, height);

  if (im)
    get_png_tags(im, png_ptr, info_ptr);

  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

  if (im) {
    if (rs.warnings) {
      i_tags_set(&im->tags, "png_warnings", rs.warnings, -1);
    }
  }
  cleanup_read_state(&rs);
  
  mm_log((1,"(%p) <- i_readpng_wiol\n", im));  
  
  return im;
}

static i_img *
read_direct8(png_structp png_ptr, png_infop info_ptr, int channels,
	     i_img_dim width, i_img_dim height) {
  i_img * volatile vim = NULL;
  int color_type = png_get_color_type(png_ptr, info_ptr);
  int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
  i_img_dim y;
  int number_passes, pass;
  i_img *im;
  unsigned char *line;
  unsigned char * volatile vline = NULL;

  if (setjmp(png_jmpbuf(png_ptr))) {
    if (vim) i_img_destroy(vim);
    if (vline) myfree(vline);

    return NULL;
  }

  number_passes = png_set_interlace_handling(png_ptr);
  mm_log((1,"number of passes=%d\n",number_passes));

  png_set_strip_16(png_ptr);
  png_set_packing(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand(png_ptr);
    
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    channels++;
    mm_log((1, "image has transparency, adding alpha: channels = %d\n", channels));
    png_set_expand(png_ptr);
  }
  
  png_read_update_info(png_ptr, info_ptr);
  
  im = vim = i_img_8_new(width,height,channels);
  if (!im) {
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    return NULL;
  }
  
  line = vline = mymalloc(channels * width);
  for (pass = 0; pass < number_passes; pass++) {
    for (y = 0; y < height; y++) {
      if (pass > 0)
	i_gsamp(im, 0, width, y, line, NULL, channels);
      png_read_row(png_ptr,(png_bytep)line, NULL);
      i_psamp(im, 0, width, y, line, NULL, channels);
    }
  }
  myfree(line);
  vline = NULL;
  
  png_read_end(png_ptr, info_ptr); 

  return im;
}

static void
get_png_tags(i_img *im, png_structp png_ptr, png_infop info_ptr) {
  png_uint_32 xres, yres;
  int unit_type;

  i_tags_set(&im->tags, "i_format", "png", -1);
  if (png_get_pHYs(png_ptr, info_ptr, &xres, &yres, &unit_type)) {
    mm_log((1,"pHYs (%d, %d) %d\n", xres, yres, unit_type));
    if (unit_type == PNG_RESOLUTION_METER) {
      i_tags_set_float2(&im->tags, "i_xres", 0, xres * 0.0254, 5);
      i_tags_set_float2(&im->tags, "i_yres", 0, yres * 0.0254, 5);
    }
    else {
      i_tags_setn(&im->tags, "i_xres", xres);
      i_tags_setn(&im->tags, "i_yres", yres);
      i_tags_setn(&im->tags, "i_aspect_only", 1);
    }
  }
  switch (png_get_interlace_type(png_ptr, info_ptr)) {
  case PNG_INTERLACE_NONE:
    i_tags_setn(&im->tags, "png_interlace", 0);
    break;
  case PNG_INTERLACE_ADAM7:
    i_tags_set(&im->tags, "png_interlace", "adam7", -1);
    break;

  default:
    i_tags_set(&im->tags, "png_interlace", "unknown", -1);
    break;
  }

  i_tags_setn(&im->tags, "png_bits", png_get_bit_depth(png_ptr, info_ptr));
}

static void
read_warn_handler(png_structp png_ptr, png_const_charp msg) {
  i_png_read_statep rs = (i_png_read_statep)png_get_error_ptr(png_ptr);
  char *workp;
  size_t new_size;

  mm_log((1, "PNG read warning '%s'\n", msg));

  /* in case this is part of an error report */
  i_push_error(0, msg);
  
  /* and save in the warnings so if we do manage to succeed, we 
   * can save it as a tag
   */
  new_size = (rs->warnings ? strlen(rs->warnings) : 0)
    + 1 /* NUL */
    + strlen(msg) /* new text */
    + 1; /* newline */
  workp = myrealloc(rs->warnings, new_size);
  if (!rs->warnings)
    *workp = '\0';
  strcat(workp, msg);
  strcat(workp, "\n");
  rs->warnings = workp;
}

static void
cleanup_read_state(i_png_read_statep rs) {
  if (rs->warnings)
    myfree(rs->warnings);
}
