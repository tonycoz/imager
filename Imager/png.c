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
 








/*
  png_set_read_fn(png_structp read_ptr, voidp read_io_ptr, png_rw_ptr read_data_fn)
  png_set_write_fn(png_structp write_ptr, voidp write_io_ptr, png_rw_ptr write_data_fn,
  png_flush_ptr output_flush_fn);
  voidp read_io_ptr = png_get_io_ptr(read_ptr);
  voidp write_io_ptr = png_get_io_ptr(write_ptr);
*/

struct png_scalar_info {
  char *data;
  int length;
  int cpos;
};


struct png_wiol_info {
  io_glue *cp;
  int length;
  int cpos;
};

static void
user_read_data(png_structp png_ptr,png_bytep data, png_size_t length) {
  struct png_scalar_info *sci=(struct png_scalar_info *)png_ptr->io_ptr;
  
  /*   fprintf(stderr,"user_read_data: cpos %d/%d (%d)\n",sci->cpos,sci->length,length); */
  
  if (sci->cpos+(ssize_t)length > sci->length ) { png_error(png_ptr, "Read overflow error on a scalar."); }
  
  memcpy(data, sci->data+sci->cpos , length);
  sci->cpos+=length;
}

/*
static void
user_write_data(png_structp png_ptr, png_bytep data, png_uint_32 length) {
FIXME: implement these 
}

static void
user_flush_data(png_structp png_ptr) {
  FIXME: implement these 
}
*/


void
scalar_png_init_io(png_structp png_ptr,struct png_scalar_info *sci) {
  png_ptr->io_ptr = (png_voidp)sci;
}






int
check_if_png(char *file_name, FILE **fp) {
   char buf[PNG_BYTES_TO_CHECK];
   
   /* Open the prospective PNG file. */
   if ((*fp = fopen(file_name, "rb")) != NULL) return 0;
   
   /* Read in some of the signature bytes */
   if (fread(buf, 1, PNG_BYTES_TO_CHECK, *fp) != PNG_BYTES_TO_CHECK) return 0;

   /* Compare the first PNG_BYTES_TO_CHECK bytes of the signature.
      Return nonzero (true) if they match */

   return(!png_sig_cmp((png_bytep)buf, (png_size_t)0, PNG_BYTES_TO_CHECK));
}

/* Read a PNG file.  You may want to return an error code if the read
 * fails (depending upon the failure).  There are two "prototypes" given
 * here - one where we are given the filename, and we need to open the
 * file, and the other where we are given an open file (possibly with
 * some or all of the magic bytes read - see comments above).
 */

i_img *
i_readpng(int fd) {
  i_img *im;
  png_structp png_ptr;
  png_infop info_ptr;
  png_uint_32 width, height, y;
  int bit_depth, color_type, interlace_type;
  int number_passes;
  int channels, pass;
  
  FILE *fp;
  unsigned int sig_read;

  sig_read = 0;

  if ((fp = fdopen(fd,"r")) == NULL) {
    mm_log((1,"can't fdopen.\n"));
    exit(1);
  }

  mm_log((1,"i_readpng(fd %d)\n",fd));

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
  
  if (png_ptr == NULL) {
    fclose(fp);
    return 0;
  }
  
  /* Allocate/initialize the memory for image information.  REQUIRED. */
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    fclose(fp);
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    return 0;
  }
  
  /* Set error handling if you are using the setjmp/longjmp method (this is
   * the normal method of doing things with libpng).  REQUIRED unless you
   * set up your own error handlers in the png_create_read_struct() earlier.
   */
  
  if (setjmp(png_ptr->jmpbuf)) {
    /* Free all of the memory associated with the png_ptr and info_ptr */
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    fclose(fp);
    /* If we get here, we had a problem reading the file */
    return NULL;
  }
  
  /* Set up the input control if you are using standard C streams */
  png_init_io(png_ptr, fp);
  /* If we have already read some of the signature */
  png_set_sig_bytes(png_ptr, sig_read);
  
  /* The call to png_read_info() gives us all of the information from the
   * PNG file before the first IDAT (image data chunk).  REQUIRED
   */
  
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
	       &interlace_type, NULL, NULL);
  
  mm_log((1,
	  "png_get_IHDR results: width %d, height %d, bit_depth %d,color_type %d,interlace_type %d\n",
	  width, height, bit_depth, color_type, interlace_type));

  CC2C[PNG_COLOR_TYPE_GRAY]=1;
  CC2C[PNG_COLOR_TYPE_PALETTE]=3;
  CC2C[PNG_COLOR_TYPE_RGB]=3;
  CC2C[PNG_COLOR_TYPE_RGB_ALPHA]=4;
  CC2C[PNG_COLOR_TYPE_GRAY_ALPHA]=2;

  channels=CC2C[color_type];
  mm_log((1,"channels %d\n", channels));
  
  /**** Set up the data transformations you want.  Note that these are all
   **** optional.  Only call them if you want/need them.  Many of the
   **** transformations only work on specific types of images, and many
   **** are mutually exclusive.
   ****/

  /* tell libpng to strip 16 bit/color files down to 8 bits/color */
  png_set_strip_16(png_ptr);
  
 
  /* Extract multiple pixels with bit depths of 1, 2, and 4 from a single
   * byte into separate bytes (useful for paletted and grayscale images).
   */

  png_set_packing(png_ptr);

  /* Expand paletted colors into true RGB triplets */
  if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_expand(png_ptr);
  
  /* Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel */
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand(png_ptr);

  /* Expand paletted or RGB images with transparency to full alpha channels
   * so the data will be available as RGBA quartets.
   */

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    channels++;
    mm_log((1, "image has transparency, adding alpha: channels = %d\n", channels));
    png_set_expand(png_ptr);
  }
  
  /* Strip alpha bytes from the input data without combining with the
   * background (not recommended).
   */
  /*
    if ( (color_type != PNG_COLOR_TYPE_RGB_ALPHA) && (color_type != PNG_COLOR_TYPE_GRAY_ALPHA) )
    png_set_strip_alpha(png_ptr);
  */
  number_passes = png_set_interlace_handling(png_ptr);

  mm_log((1,"number of passes=%d\n",number_passes));
  
  png_read_update_info(png_ptr, info_ptr);

  im = i_img_empty_ch(NULL, width, height, channels);
  for (pass = 0; pass < number_passes; pass++)
    for (y = 0; y < height; y++) {
      png_read_row(png_ptr,(png_bytep) &(im->data[channels*width*y]), NULL);
    }
  /* read rest of file, and get additional chunks in info_ptr - REQUIRED */
  
  png_read_end(png_ptr, info_ptr); 
  /* clean up after the read, and free any memory allocated - REQUIRED */
  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

  fclose(fp);
  return im;
}

  


undef_int
i_writepng(i_img *im,int fd) {
  FILE *fp;
  png_structp png_ptr;
  png_infop info_ptr;
  int width,height,y;
  volatile int cspace,channels;

  mm_log((1,"i_writepng(0x%x,fd %d)\n",im,fd));
  
  if ((fp = fdopen(fd,"w")) == NULL) {
    mm_log((1,"can't fdopen.\n"));
    exit(1);
  }

  height=im->ysize;
  width=im->xsize;

  channels=im->channels;

  if (channels>2) { cspace=PNG_COLOR_TYPE_RGB; channels-=3; }
  else { cspace=PNG_COLOR_TYPE_GRAY; channels--; }
  
  if (channels) cspace|=PNG_COLOR_MASK_ALPHA;
  mm_log((1,"cspace=%d\n",cspace));

  channels=im->channels;

  /* Create and initialize the png_struct with the desired error handler
   * functions.  If you want to use the default stderr and longjump method,
   * you can supply NULL for the last three parameters.  We also check that
   * the library version is compatible with the one used at compile time,
   * in case we are using dynamically linked libraries.  REQUIRED.
   */
  
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
  
  if (png_ptr == NULL) {
    fclose(fp);
    return 0;
  }
  
  /* Allocate/initialize the image information data.  REQUIRED */
  info_ptr = png_create_info_struct(png_ptr);

  if (info_ptr == NULL) {
    fclose(fp);
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    return(0);
  }
  
  /* Set error handling.  REQUIRED if you aren't supplying your own
   * error hadnling functions in the png_create_write_struct() call.
   */
  if (setjmp(png_ptr->jmpbuf)) {
    /* If we get here, we had a problem reading the file */
    fclose(fp);
    png_destroy_write_struct(&png_ptr,  (png_infopp)NULL);
    return(0);
  }
  
  png_init_io(png_ptr, fp);

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

  png_write_info(png_ptr, info_ptr);
  for (y = 0; y < height; y++) png_write_row(png_ptr, (png_bytep) &(im->data[channels*width*y]));
  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

  fclose(fp);
  return(1);
}




i_img*
i_readpng_scalar(char *data, int length) {
  i_img *im;
  png_structp png_ptr;
  png_infop info_ptr;
  png_uint_32 width, height, y;
  int bit_depth, color_type, interlace_type;
  int number_passes;
  int channels, pass;
  unsigned int sig_read;

  struct png_scalar_info sci;

  sci.data=data;
  sci.length=length;
  sci.cpos=0;

  sig_read=0;
  mm_log((1,"i_readpng_scalar(char 0x%08X, length %d)\n",data,length));

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
  png_set_read_fn(png_ptr, (void*) (&sci), user_read_data);
  
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    return NULL;
  }
  
  if (setjmp(png_ptr->jmpbuf)) {
    mm_log((1,"i_readpng_scalar: error.\n"));
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    return NULL;
  }
  
  scalar_png_init_io(png_ptr, &sci);
  png_set_sig_bytes(png_ptr, sig_read);
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  
  mm_log((1,
	  "png_get_IHDR results: width %d, height %d, bit_depth %d,color_type %d,interlace_type %d\n",
	  width,height,bit_depth,color_type,interlace_type));
  
  CC2C[PNG_COLOR_TYPE_GRAY]=1;
  CC2C[PNG_COLOR_TYPE_PALETTE]=3;
  CC2C[PNG_COLOR_TYPE_RGB]=3;
  CC2C[PNG_COLOR_TYPE_RGB_ALPHA]=4;
  CC2C[PNG_COLOR_TYPE_GRAY_ALPHA]=2;
  channels=CC2C[color_type];
  mm_log((1,"channels %d\n",channels));
  im=i_img_empty_ch(NULL,width,height,channels);
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
  mm_log((1,"made it to here 1\n"));
  for (pass = 0; pass < number_passes; pass++)
    for (y = 0; y < height; y++) { png_read_row(png_ptr,(png_bytep) &(im->data[channels*width*y]), NULL); }
  mm_log((1,"made it to here 2\n"));
  png_read_end(png_ptr, info_ptr); 
  mm_log((1,"made it to here 3\n"));
  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
  mm_log((1,"made it to here 4\n"));
  mm_log((1,"(0x%08X) <- i_readpng_scalar\n",im));  
  return im;
}




/* i_img* */
/* i_readpng_wiol(io_glue *ig, int length) { */
/*   i_img *im; */
/*   png_structp png_ptr; */
/*   png_infop info_ptr; */
/*   png_uint_32 width, height; */
/*   int bit_depth, color_type, interlace_type; */
/*   int number_passes,y; */
/*   int channels,pass; */
/*   unsigned int sig_read; */

/*   struct png_wiol_info wi; */

/*   wi.data   = ig; */
/*   wi.length = length; */
/*   wi.cpos   = 0; */
    
/*   sig_read=0; */
/*   mm_log((1,"i_readpng_wiol(char 0x%p, length %d)\n", data, length)); */

/*   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL); */
/*   png_set_read_fn(png_ptr, (void*) (&wi), user_read_data); */
  
/*   info_ptr = png_create_info_struct(png_ptr); */
/*   if (info_ptr == NULL) { */
/*     png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL); */
/*     return NULL; */
/*   } */
  
/*   if (setjmp(png_ptr->jmpbuf)) { */
/*     mm_log((1,"i_readpng_wiol: error.\n")); */
/*     png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL); */
/*     return NULL; */
/*   } */
  
/*   scalar_png_init_io(png_ptr, &sci); */
/*   png_set_sig_bytes(png_ptr, sig_read); */
/*   png_read_info(png_ptr, info_ptr); */
/*   png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL); */
  
/*   mm_log((1, */
/* 	  "png_get_IHDR results: width %d, height %d, bit_depth %d,color_type %d,interlace_type %d\n", */
/* 	  width,height,bit_depth,color_type,interlace_type)); */
  
/*   CC2C[PNG_COLOR_TYPE_GRAY]=1; */
/*   CC2C[PNG_COLOR_TYPE_PALETTE]=3; */
/*   CC2C[PNG_COLOR_TYPE_RGB]=3; */
/*   CC2C[PNG_COLOR_TYPE_RGB_ALPHA]=4; */
/*   CC2C[PNG_COLOR_TYPE_GRAY_ALPHA]=2; */
/*   channels = CC2C[color_type]; */

/*   mm_log((1,"i_readpng_wiol: channels %d\n",channels)); */

/*   im = i_img_empty_ch(NULL,width,height,channels); */
/*   png_set_strip_16(png_ptr); */
/*   png_set_packing(png_ptr); */
/*   if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_expand(png_ptr); */
/*   if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand(png_ptr); */
/*   if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_expand(png_ptr); */
/*   number_passes = png_set_interlace_handling(png_ptr); */
/*   mm_log((1,"number of passes=%d\n",number_passes)); */
/*   png_read_update_info(png_ptr, info_ptr); */
/*   mm_log((1,"made it to here 1\n")); */
/*   for (pass = 0; pass < number_passes; pass++) */
/*     for (y = 0; y < height; y++) { png_read_row(png_ptr,(png_bytep) &(im->data[channels*width*y]), NULL); } */
/*   mm_log((1,"made it to here 2\n")); */
/*   png_read_end(png_ptr, info_ptr);  */
/*   mm_log((1,"made it to here 3\n")); */
/*   png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL); */
/*   mm_log((1,"made it to here 4\n")); */
/*   mm_log((1,"(0x%08X) <- i_readpng_scalar\n",im));   */

/*   return im; */
/* } */
