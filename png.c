#include "iolayer.h"
#include "image.h"
#include "png.h"

/* Check to see if a file is a PNG file using png_sig_cmp().  png_sig_cmp()
 * returns zero if the image is a PNG and nonzero if it isn't a PNG.
 *
 * The function check_if_png() shown here, but not used, returns nonzero (true)
 * if the file can be opened and is a PNG, 0 (false) otherwise.
 *
 * If this call is successful, and you are going to keep the file open,
 * you should call png_set_sig_bytes(png_ptr, PNG_BYTES_TO_CHECK); once
 * you have created the png_ptr, so that libpng knows your application
 * has read that many bytes from the start of the file.  Make sure you
 * don't call png_set_sig_bytes() with more than 8 bytes read or give it
 * an incorrect number of bytes read, or you will either have read too
 * many bytes (your fault), or you are telling libpng to read the wrong
 * number of magic bytes (also your fault).
 *
 * Many applications already read the first 2 or 4 bytes from the start
 * of the image to determine the file type, so it would be easiest just
 * to pass the bytes to png_sig_cmp() or even skip that if you know
 * you have a PNG file, and call png_set_sig_bytes().
 */

/* this is a way to get number of channels from color space 
 * Color code to channel number */

int CC2C[PNG_COLOR_MASK_PALETTE|PNG_COLOR_MASK_COLOR|PNG_COLOR_MASK_ALPHA];

#define PNG_BYTES_TO_CHECK 4
 


static void
wiol_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
  io_glue *ig = (io_glue *)png_ptr->io_ptr;
  int rc = ig->readcb(ig, data, length);
  if (rc != length) png_error(png_ptr, "Read overflow error on an iolayer source.");
}

static void
wiol_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
  int rc;
  io_glue *ig = (io_glue *)png_ptr->io_ptr;
  rc = ig->writecb(ig, data, length);
  if (rc != length) png_error(png_ptr, "Write error on an iolayer source.");
}

static void
wiol_flush_data(png_structp png_ptr) {
  /* XXX : This needs to be added to the io layer */
}


/* Check function demo 

int
check_if_png(char *file_name, FILE **fp) {
  char buf[PNG_BYTES_TO_CHECK];
  if ((*fp = fopen(file_name, "rb")) != NULL) return 0;
  if (fread(buf, 1, PNG_BYTES_TO_CHECK, *fp) != PNG_BYTES_TO_CHECK) return 0;
  return(!png_sig_cmp((png_bytep)buf, (png_size_t)0, PNG_BYTES_TO_CHECK));
}
*/

undef_int
i_writepng_wiol(i_img *im, io_glue *ig) {
  png_structp png_ptr;
  png_infop info_ptr;
  int width,height,y;
  volatile int cspace,channels;
  double xres, yres;
  int aspect_only, have_res;
  double offx, offy;
  char offunit[20] = "pixel";

  io_glue_commit_types(ig);
  mm_log((1,"i_writepng(im %p ,ig %p)\n", im, ig));
  
  height = im->ysize;
  width  = im->xsize;

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
  
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
  
  if (png_ptr == NULL) return 0;

  
  /* Allocate/initialize the image information data.  REQUIRED */
  info_ptr = png_create_info_struct(png_ptr);

  if (info_ptr == NULL) {
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    return 0;
  }
  
  /* Set error handling.  REQUIRED if you aren't supplying your own
   * error hadnling functions in the png_create_write_struct() call.
   */
  if (setjmp(png_ptr->jmpbuf)) {
    png_destroy_write_struct(&png_ptr,  (png_infopp)NULL);
    return(0);
  }
  
  png_set_write_fn(png_ptr, (png_voidp) (ig), wiol_write_data, wiol_flush_data);
  png_ptr->io_ptr = (png_voidp) ig;

  /* Set the image information here.  Width and height are up to 2^31,
   * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
   * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
   * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
   * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
   * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
   * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
   */

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

  if (!im->virtual && im->type == i_direct_type && im->bits == i_8_bits) {
    for (y = 0; y < height; y++) 
      png_write_row(png_ptr, (png_bytep) &(im->idata[channels*width*y]));
  }
  else {
    unsigned char *data = mymalloc(im->xsize * im->channels);
    if (data) {
      for (y = 0; y < height; y++) {
        i_gsamp(im, 0, im->xsize, y, data, NULL, im->channels);
        png_write_row(png_ptr, (png_bytep)data);
      }
      myfree(data);
    }
    else {
      png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
      return 0;
    }
  }

  png_write_end(png_ptr, info_ptr);

  png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

  return(1);
}



static void get_png_tags(i_img *im, png_structp png_ptr, png_infop info_ptr);

i_img*
i_readpng_wiol(io_glue *ig, int length) {
  i_img *im;
  png_structp png_ptr;
  png_infop info_ptr;
  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type;
  int number_passes,y;
  int channels,pass;
  unsigned int sig_read;

  sig_read  = 0;

  io_glue_commit_types(ig);
  mm_log((1,"i_readpng_wiol(ig %p, length %d)\n", ig, length));

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
  png_set_read_fn(png_ptr, (png_voidp) (ig), wiol_read_data);
  
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    return NULL;
  }
  
  if (setjmp(png_ptr->jmpbuf)) {
    mm_log((1,"i_readpng_wiol: error.\n"));
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    return NULL;
  }

  png_ptr->io_ptr = (png_voidp) ig;
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

  png_set_strip_16(png_ptr);
  png_set_packing(png_ptr);
  if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_expand(png_ptr);
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand(png_ptr);

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    channels++;
    mm_log((1, "image has transparency, adding alpha: channels = %d\n", channels));
    png_set_expand(png_ptr);
  }
  
  number_passes = png_set_interlace_handling(png_ptr);
  mm_log((1,"number of passes=%d\n",number_passes));
  png_read_update_info(png_ptr, info_ptr);
  
  im = i_img_empty_ch(NULL,width,height,channels);

  for (pass = 0; pass < number_passes; pass++)
    for (y = 0; y < height; y++) { png_read_row(png_ptr,(png_bytep) &(im->idata[channels*width*y]), NULL); }
  
  png_read_end(png_ptr, info_ptr); 
  
  get_png_tags(im, png_ptr, info_ptr);

  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
  
  mm_log((1,"(0x%08X) <- i_readpng_scalar\n", im));  
  
  return im;
}

static void get_png_tags(i_img *im, png_structp png_ptr, png_infop info_ptr) {
  png_uint_32 xres, yres;
  int unit_type;
  if (png_get_pHYs(png_ptr, info_ptr, &xres, &yres, &unit_type)) {
    mm_log((1,"pHYs (%d, %d) %d\n", xres, yres, unit_type));
    if (unit_type == PNG_RESOLUTION_METER) {
      i_tags_set_float(&im->tags, "i_xres", 0, xres * 0.0254);
      i_tags_set_float(&im->tags, "i_yres", 0, xres * 0.0254);
    }
    else {
      i_tags_addn(&im->tags, "i_xres", 0, xres);
      i_tags_addn(&im->tags, "i_yres", 0, yres);
      i_tags_addn(&im->tags, "i_aspect_only", 0, 1);
    }
  }
}
