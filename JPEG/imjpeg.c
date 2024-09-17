/*
=head1 NAME

jpeg.c - implement saving and loading JPEG images

=head1 SYNOPSIS

  io_glue *ig;
  if (!i_writejpeg_wiol(im, ig, quality)) {
    .. error ..
  }
  im = i_readjpeg_wiol(ig, length, iptc_text, itlength);

=head1 DESCRIPTION

Reads and writes JPEG images

=over

=cut
*/

#include "imjpeg.h"
#include "imext.h"
#include <stdio.h>
#include <sys/stat.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <setjmp.h>
#include <string.h>

#include "jpeglib.h"
#include "jerror.h"
#include <errno.h>
#include <stdlib.h>
#include <math.h>

#define JPEG_APP13       0xED    /* APP13 marker code */
#define JPEG_APP1 (JPEG_APP0 + 1)
#define JPGS 16384

#define JPEG_DIM_MAX JPEG_MAX_DIMENSION

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

static unsigned char fake_eoi[]={(JOCTET) 0xFF,(JOCTET) JPEG_EOI};

#ifdef JPEG_C_PARAM_SUPPORTED
#define IS_MOZJPEG
#endif

/* Source and Destination managers */

typedef struct {
  struct jpeg_source_mgr pub;	/* public fields */
  io_glue *data;
  JOCTET *buffer;		/* start of buffer */
  int length;			/* Do I need this? */
  boolean start_of_file;	/* have we gotten any data yet? */
} wiol_source_mgr;

typedef struct {
  struct jpeg_destination_mgr pub; /* public fields */
  io_glue *data;
  JOCTET *buffer;		/* start of buffer */
  boolean start_of_file;	/* have we gotten any data yet? */
} wiol_destination_mgr;

typedef wiol_source_mgr *wiol_src_ptr;
typedef wiol_destination_mgr *wiol_dest_ptr;

/*
 * Methods for io manager objects 
 * 
 * Methods for source managers: 
 *
 * init_source         (j_decompress_ptr cinfo);
 * skip_input_data     (j_decompress_ptr cinfo, long num_bytes);
 * fill_input_buffer   (j_decompress_ptr cinfo);
 * term_source         (j_decompress_ptr cinfo);
 */



static void
wiol_init_source (j_decompress_ptr cinfo) {
  wiol_src_ptr src = (wiol_src_ptr) cinfo->src;
  
  /* We reset the empty-input-file flag for each image, but we don't clear
   * the input buffer.   This is correct behavior for reading a series of
   * images from one source.
   */
  src->start_of_file = TRUE;
}



static boolean
wiol_fill_input_buffer(j_decompress_ptr cinfo) {
  wiol_src_ptr src = (wiol_src_ptr) cinfo->src;
  ssize_t nbytes; /* We assume that reads are "small" */
  
  mm_log((1,"wiol_fill_input_buffer(cinfo %p)\n", cinfo));
  
  nbytes = i_io_read(src->data, src->buffer, JPGS);
  
  if (nbytes <= 0) { /* Insert a fake EOI marker */
    src->pub.next_input_byte = fake_eoi;
    src->pub.bytes_in_buffer = 2;
  } else {
    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = nbytes;
  }
  src->start_of_file = FALSE;
  return TRUE;
}


static void
wiol_skip_input_data (j_decompress_ptr cinfo, long num_bytes) {
  wiol_src_ptr src = (wiol_src_ptr) cinfo->src;
  
  /* Just a dumb implementation for now.  Could use fseek() except
   * it doesn't work on pipes.  Not clear that being smart is worth
   * any trouble anyway --- large skips are infrequent.
   */
  
  if (num_bytes > 0) {
    while (num_bytes > (long) src->pub.bytes_in_buffer) {
      num_bytes -= (long) src->pub.bytes_in_buffer;
      (void) wiol_fill_input_buffer(cinfo);
      /* note we assume that fill_input_buffer will never return FALSE,
       * so suspension need not be handled.
       */
    }
    src->pub.next_input_byte += (size_t) num_bytes;
    src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
wiol_term_source (j_decompress_ptr cinfo) {
  /* no work necessary here */
  /* we used to free memory for the I/O buffer,
     but we let the library handle that now
  */
}


/* Source manager Constructor */

static void
jpeg_wiol_src(j_decompress_ptr cinfo, io_glue *ig, int length) {
  wiol_src_ptr src;
  
  if (cinfo->src == NULL) {	/* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small) 
      ((j_common_ptr) cinfo, JPOOL_PERMANENT,sizeof(wiol_source_mgr));
    src = (wiol_src_ptr) cinfo->src;
  }

  /* put the request method call in here later */
  
  src         = (wiol_src_ptr) cinfo->src;
  src->data   = ig;
  src->buffer = (*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, JPGS);
  src->length = length;

  src->pub.init_source       = wiol_init_source;
  src->pub.fill_input_buffer = wiol_fill_input_buffer;
  src->pub.skip_input_data   = wiol_skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->pub.term_source       = wiol_term_source;
  src->pub.bytes_in_buffer   = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte   = NULL; /* until buffer loaded */
}




/*
 * Methods for destination managers:
 *
 * init_destination    (j_compress_ptr cinfo);
 * empty_output_buffer (j_compress_ptr cinfo);
 * term_destination    (j_compress_ptr cinfo);
 *
 */

static void
wiol_init_destination (j_compress_ptr cinfo) {
  wiol_dest_ptr dest = (wiol_dest_ptr) cinfo->dest;
  
  /* We reset the empty-input-file flag for each image, but we don't clear
   * the input buffer.   This is correct behavior for reading a series of
   * images from one source.
   */
  dest->start_of_file = TRUE;
}

static boolean
wiol_empty_output_buffer(j_compress_ptr cinfo) {
  wiol_dest_ptr dest = (wiol_dest_ptr) cinfo->dest;
  ssize_t rc;
  /*
    Previously this code was checking free_in_buffer to see how much 
    needed to be written.  This does not follow the documentation:

                       "In typical applications, it should write out the
        *entire* buffer (use the saved start address and buffer length;
        ignore the current state of next_output_byte and free_in_buffer)."

  ssize_t nbytes     = JPGS - dest->pub.free_in_buffer;
  */

  mm_log((1,"wiol_empty_output_buffer(cinfo %p)\n", cinfo));
  rc = i_io_write(dest->data, dest->buffer, JPGS);

  if (rc != JPGS) { /* XXX: Should raise some jpeg error */
    mm_log((1, "wiol_empty_output_buffer: Error: nbytes = %d != rc = %d\n", JPGS, (int)rc));
    ERREXIT(cinfo, JERR_FILE_WRITE);
  }
  dest->pub.free_in_buffer = JPGS;
  dest->pub.next_output_byte = dest->buffer;
  return TRUE;
}

static void
wiol_term_destination (j_compress_ptr cinfo) {
  wiol_dest_ptr dest = (wiol_dest_ptr) cinfo->dest;
  size_t nbytes = JPGS - dest->pub.free_in_buffer;
  /* yes, this needs to flush the buffer */
  /* needs error handling */

  if (i_io_write(dest->data, dest->buffer, nbytes) != nbytes) {
    ERREXIT(cinfo, JERR_FILE_WRITE);
  }
}


/* Destination manager Constructor */

static void
jpeg_wiol_dest(j_compress_ptr cinfo, io_glue *ig) {
  wiol_dest_ptr dest;
  
  if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
    cinfo->dest = 
      (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small) 
      ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(wiol_destination_mgr));
  }
  
  dest = (wiol_dest_ptr) cinfo->dest;
  dest->data                    = ig;
  dest->buffer                  =
    (*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, JPGS);

  dest->pub.init_destination    = wiol_init_destination;
  dest->pub.empty_output_buffer = wiol_empty_output_buffer;
  dest->pub.term_destination    = wiol_term_destination;
  dest->pub.free_in_buffer      = JPGS;
  dest->pub.next_output_byte    = dest->buffer;
}

METHODDEF(void)
my_output_message (j_common_ptr cinfo) {
  char buffer[JMSG_LENGTH_MAX];

  /* Create the message */
  (*cinfo->err->format_message) (cinfo, buffer);

  i_push_error(0, buffer);

  /* Send it to stderr, adding a newline */
  mm_log((1, "%s\n", buffer));
}

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/* Here's the routine that will replace the standard error_exit method */

METHODDEF(void)
my_error_exit (j_common_ptr cinfo) {
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;
  
  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

static void 
transfer_cmyk_inverted(i_color *out, JSAMPARRAY in, int width) {
  JSAMPROW inrow = *in;
  while (width--) {
    /* extract and convert to real CMYK */
    /* horribly enough this is correct given cmyk values are inverted */
    int c = *inrow++;
    int m = *inrow++;
    int y = *inrow++;
    int k = *inrow++;
    out->rgba.r = (c * k) / MAXJSAMPLE;
    out->rgba.g = (m * k) / MAXJSAMPLE;
    out->rgba.b = (y * k) / MAXJSAMPLE;
    ++out;
  }
}

static void
transfer_rgb(i_color *out, JSAMPARRAY in, int width) {
  JSAMPROW inrow = *in;
  while (width--) {
    out->rgba.r = *inrow++;
    out->rgba.g = *inrow++;
    out->rgba.b = *inrow++;
    ++out;
  }
}

static void
transfer_gray(i_color *out, JSAMPARRAY in, int width) {
  JSAMPROW inrow = *in;
  while (width--) {
    out->gray.gray_color = *inrow++;
    ++out;
  }
}

typedef void (*transfer_function_t)(i_color *out, JSAMPARRAY in, int width);

static const char version_string[] =
#if defined(IS_MOZJPEG)
  "mozjpeg version " STRINGIFY(LIBJPEG_TURBO_VERSION) " api " STRINGIFY(JPEG_LIB_VERSION)
#elif defined(LIBJPEG_TURBO_VERSION)
  "libjpeg-turbo version " STRINGIFY(LIBJPEG_TURBO_VERSION) " api " STRINGIFY(JPEG_LIB_VERSION)
#else
  "libjpeg version " STRINGIFY(JPEG_LIB_VERSION) " api " STRINGIFY(JPEG_LIB_VERSION)
#endif
  ;

/*
=item i_libjpeg_version()

=cut
*/

const char *
i_libjpeg_version(void) {
  return version_string;
}

int
is_turbojpeg(void) {
#if defined(LIBJPEG_TURBO_VERSION)
  return 1;
#else
  return 0;
#endif
}

int
is_mozjpeg(void) {
#ifdef IS_MOZJPEG
  return 1;
#else
  return 0;
#endif
}

int
has_encode_arith_coding(void) {
#ifdef C_ARITH_CODING_SUPPORTED
  return 1;
#else
  return 0;
#endif
}

int
has_decode_arith_coding(void) {
#ifdef D_ARITH_CODING_SUPPORTED
  return 1;
#else
  return 0;
#endif
}

/*
=item i_readjpeg_wiol(data, length, iptc_itext, itlength)

=cut
*/
i_img*
i_readjpeg_wiol(io_glue *data, int length, char** iptc_itext, int *itlength) {
  i_img * volatile im = NULL;
  int seen_exif = 0;
  i_color * volatile line_buffer = NULL;
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */
  jpeg_saved_marker_ptr markerp;
  transfer_function_t transfer_f;
  int channels;
  volatile int src_set = 0;
  int jfif;

  mm_log((1,"i_readjpeg_wiol(data %p, length %d,iptc_itext %p)\n", data, length, iptc_itext));

  i_clear_error();

  *iptc_itext = NULL;
  *itlength = 0;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit     = my_error_exit;
  jerr.pub.output_message = my_output_message;

  /* Set error handler */
  if (setjmp(jerr.setjmp_buffer)) {
    if (src_set)
      wiol_term_source(&cinfo);
    jpeg_destroy_decompress(&cinfo); 
    if (line_buffer)
      myfree(line_buffer);
    if (im)
      i_img_destroy(im);
    return NULL;
  }
  
  jpeg_create_decompress(&cinfo);
  jpeg_save_markers(&cinfo, JPEG_APP13, 0xFFFF);
  jpeg_save_markers(&cinfo, JPEG_APP1, 0xFFFF);
  jpeg_save_markers(&cinfo, JPEG_COM, 0xFFFF);
  jpeg_wiol_src(&cinfo, data, length);
  src_set = 1;

  (void) jpeg_read_header(&cinfo, TRUE);
  (void) jpeg_start_decompress(&cinfo);

  channels = cinfo.output_components;
  switch (cinfo.out_color_space) {
  case JCS_GRAYSCALE:
    if (cinfo.output_components != 1) {
      mm_log((1, "i_readjpeg: grayscale image with %d channels\n", cinfo.output_components));
      i_push_errorf(0, "grayscale image with invalid components %d", cinfo.output_components);
      wiol_term_source(&cinfo);
      jpeg_destroy_decompress(&cinfo);
      return NULL;
    }
    transfer_f = transfer_gray;
    break;
  
  case JCS_RGB:
    transfer_f = transfer_rgb;
    if (cinfo.output_components != 3) {
      mm_log((1, "i_readjpeg: RGB image with %d channels\n", cinfo.output_components));
      i_push_errorf(0, "RGB image with invalid components %d", cinfo.output_components);
      wiol_term_source(&cinfo);
      jpeg_destroy_decompress(&cinfo);
      return NULL;
    }
    break;

  case JCS_CMYK:
    if (cinfo.output_components == 4) {
      /* we treat the CMYK values as inverted, because that's what that
	 buggy photoshop does, and everyone has to follow the gorilla.

	 Is there any app that still produces correct CMYK JPEGs?
      */
      transfer_f = transfer_cmyk_inverted;
      channels = 3;
    }
    else {
      mm_log((1, "i_readjpeg: cmyk image with %d channels\n", cinfo.output_components));
      i_push_errorf(0, "CMYK image with invalid components %d", cinfo.output_components);
      wiol_term_source(&cinfo);
      jpeg_destroy_decompress(&cinfo);
      return NULL;
    }
    break;

  default:
    mm_log((1, "i_readjpeg: unknown color space %d\n", cinfo.out_color_space));
    i_push_errorf(0, "Unknown color space %d", cinfo.out_color_space);
    wiol_term_source(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return NULL;
  }

  if (!i_int_check_image_file_limits(cinfo.output_width, cinfo.output_height,
				     channels, sizeof(i_sample_t))) {
    mm_log((1, "i_readjpeg: image size exceeds limits\n"));
    wiol_term_source(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return NULL;
  }

  im = i_img_8_new(cinfo.output_width, cinfo.output_height, channels);
  if (!im) {
    wiol_term_source(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return NULL;
  }
  row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
  line_buffer = mymalloc(sizeof(i_color) * cinfo.output_width);
  while (cinfo.output_scanline < cinfo.output_height) {
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
    transfer_f(line_buffer, buffer, cinfo.output_width);
    i_plin(im, 0, cinfo.output_width, cinfo.output_scanline-1, line_buffer);
  }
  myfree(line_buffer);
  line_buffer = NULL;

  /* check for APP1 marker and save */
  markerp = cinfo.marker_list;
  while (markerp != NULL) {
    if (markerp->marker == JPEG_COM) {
      i_tags_set(&im->tags, "jpeg_comment", (const char *)markerp->data,
		 markerp->data_length);
    }
    else if (markerp->marker == JPEG_APP1 && !seen_exif) {
      unsigned char *data = markerp->data;
      size_t len = markerp->data_length;
      if (len >= 6 && memcmp(data, "Exif\0\0", 6) == 0) {
	seen_exif = im_decode_exif(im, data+6, len-6);
      }
    }
    else if (markerp->marker == JPEG_APP13) {
      *iptc_itext = mymalloc(markerp->data_length);
      memcpy(*iptc_itext, markerp->data, markerp->data_length);
      *itlength = markerp->data_length;
    }

    markerp = markerp->next;
  }

  i_tags_setn(&im->tags, "jpeg_out_color_space", cinfo.out_color_space);
  i_tags_setn(&im->tags, "jpeg_color_space", cinfo.jpeg_color_space);
  i_tags_setn(&im->tags, "jpeg_read_arithmetic", cinfo.arith_code);

  i_tags_setn(&im->tags, "jpeg_read_jfif", !!cinfo.saw_JFIF_marker);
  if (cinfo.saw_JFIF_marker) {
    double xres = cinfo.X_density;
    double yres = cinfo.Y_density;
    
    i_tags_setn(&im->tags, "jpeg_density_unit", cinfo.density_unit);
    switch (cinfo.density_unit) {
    case 0: /* values are just the aspect ratio */
      i_tags_setn(&im->tags, "i_aspect_only", 1);
      i_tags_set(&im->tags, "jpeg_density_unit_name", "none", -1);
      break;

    case 1: /* per inch */
      i_tags_set(&im->tags, "jpeg_density_unit_name", "inch", -1);
      break;

    case 2: /* per cm */
      i_tags_set(&im->tags, "jpeg_density_unit_name", "centimeter", -1);
      xres *= 2.54;
      yres *= 2.54;
      break;
    }
    i_tags_set_float2(&im->tags, "i_xres", 0, xres, 6);
    i_tags_set_float2(&im->tags, "i_yres", 0, yres, 6);
  }

  /* I originally used jpeg_has_multiple_scans() here, but that can
   * return true for non-progressive files too.  The progressive_mode
   * member is available at least as far back as 6b and does the right
   * thing.
   */
  i_tags_setn(&im->tags, "jpeg_progressive", 
	      cinfo.progressive_mode ? 1 : 0);

  (void) jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  i_tags_set(&im->tags, "i_format", "jpeg", 4);

  mm_log((1,"i_readjpeg_wiol -> (%p)\n",im));
  return im;
}

struct moz_option {
  const char *name;
#ifdef IS_MOZJPEG
  unsigned tag;
#endif
};

#ifdef IS_MOZJPEG
#define MOZ_OPTION_ENTRY(name, tag) { name, tag }
#else
#define MOZ_OPTION_ENTRY(name, tag) { name }
#endif

static struct moz_option boolean_options[] = {
  MOZ_OPTION_ENTRY("jpeg_optimize_scans",        JBOOLEAN_OPTIMIZE_SCANS),
  MOZ_OPTION_ENTRY("jpeg_trellis_quant",         JBOOLEAN_TRELLIS_QUANT),
  MOZ_OPTION_ENTRY("jpeg_trellis_quant_dc",      JBOOLEAN_TRELLIS_QUANT_DC),
  MOZ_OPTION_ENTRY("jpeg_tresllis_eob_opt",      JBOOLEAN_TRELLIS_EOB_OPT),
  MOZ_OPTION_ENTRY("jpeg_use_lambda_weight_tbl", JBOOLEAN_USE_LAMBDA_WEIGHT_TBL),
  MOZ_OPTION_ENTRY("jpeg_use_scans_in_trellis",  JBOOLEAN_USE_SCANS_IN_TRELLIS),
  MOZ_OPTION_ENTRY("jpeg_overshoot_deringing",   JBOOLEAN_OVERSHOOT_DERINGING)
};

static struct moz_option float_options[] = {
  MOZ_OPTION_ENTRY("jpeg_lambda_log_scale1",       JFLOAT_LAMBDA_LOG_SCALE1),
  MOZ_OPTION_ENTRY("jpeg_lambda_log_scale2",       JFLOAT_LAMBDA_LOG_SCALE2),
  MOZ_OPTION_ENTRY("jpeg_trellis_delta_dc_weight", JFLOAT_TRELLIS_DELTA_DC_WEIGHT)
};

static struct moz_option int_options[] = {
  MOZ_OPTION_ENTRY("jpeg_trellis_freq_split", JINT_TRELLIS_FREQ_SPLIT),
  MOZ_OPTION_ENTRY("jpeg_trellis_num_loops",  JINT_TRELLIS_NUM_LOOPS),
  MOZ_OPTION_ENTRY("jpeg_base_quant_tbl_idx", JINT_BASE_QUANT_TBL_IDX),
  MOZ_OPTION_ENTRY("jpeg_dc_scan_opt_mode",   JINT_DC_SCAN_OPT_MODE)
};

/*
=item i_writejpeg_wiol(im, ig, qfactor)

=cut
*/

undef_int
i_writejpeg_wiol(i_img *im, io_glue *ig, int qfactor) {
  JSAMPLE *image_buffer;
  int got_xres, got_yres, aspect_only, resunit;
  double xres, yres;
  int comment_entry;
  int want_channels = im->channels;
  int progressive = 0;
  int optimize = 0;
  int arithmetic = 0;
  char profile_name[20] = "";
  int jfif;

  struct jpeg_compress_struct cinfo;
  struct my_error_mgr jerr;

  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */
  unsigned char * data = NULL;
  i_color *line_buf = NULL;

  mm_log((1,"i_writejpeg(im %p, ig %p, qfactor %d)\n", im, ig, qfactor));
  
  i_clear_error();

  if (im->xsize > JPEG_DIM_MAX || im->ysize > JPEG_DIM_MAX) {
    i_push_error(0, "image too large for JPEG");
    return 0;
  }

  if (!(im->channels==1 || im->channels==3)) { 
    want_channels = im->channels - 1;
  }

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  jerr.pub.output_message = my_output_message;
  
  jpeg_create_compress(&cinfo);

  if (setjmp(jerr.setjmp_buffer)) {
  fail:
    jpeg_destroy_compress(&cinfo);
    if (data)
      myfree(data);
    if (line_buf)
      myfree(line_buf);
    return 0;
  }

  jpeg_wiol_dest(&cinfo, ig);

  cinfo.image_width  = im -> xsize; 	/* image width and height, in pixels */
  cinfo.image_height = im -> ysize;

  if (want_channels==3) {
    cinfo.input_components = 3;		/* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  }

  if (want_channels==1) {
    cinfo.input_components = 1;		/* # of color components per pixel */
    cinfo.in_color_space = JCS_GRAYSCALE; 	/* colorspace of input image */
  }

  if (i_tags_get_string(&im->tags, "jpeg_compress_profile", 0,
                        profile_name, sizeof(profile_name))) {
    if (strcmp(profile_name, "fastest") == 0) {
#ifdef IS_MOZJPEG
      jpeg_c_set_int_param(&cinfo, JINT_COMPRESS_PROFILE, JCP_FASTEST);
#endif
      /* else default */
    }
    else if (strcmp(profile_name, "max") == 0) {
#ifdef IS_MOZJPEG
      jpeg_c_set_int_param(&cinfo, JINT_COMPRESS_PROFILE, JCP_MAX_COMPRESSION);
#else
      i_push_error(0, "jpeg_compress_profile=max requires mozjpeg");
      goto fail;
#endif
    }
    else {
      i_push_errorf(0, "jpeg_compress_profile=%s unknown", profile_name);
      goto fail;
    }
  }
#ifdef IS_MOZJPEG
  else {
    jpeg_c_set_int_param(&cinfo, JINT_COMPRESS_PROFILE, JCP_FASTEST);
  }
#endif

  jpeg_set_defaults(&cinfo);

  {
    char tune_str[20];
    if (i_tags_get_string(&im->tags, "jpeg_tune", 0, tune_str, sizeof(tune_str))) {
      if (strcmp(tune_str, "psnr") == 0) {
#ifdef IS_MOZJPEG
        jpeg_c_set_int_param(&cinfo, JINT_BASE_QUANT_TBL_IDX, 1);
        jpeg_c_set_float_param(&cinfo, JFLOAT_LAMBDA_LOG_SCALE1, 9.0);
        jpeg_c_set_float_param(&cinfo, JFLOAT_LAMBDA_LOG_SCALE2, 0.0);
        jpeg_c_set_bool_param(&cinfo, JBOOLEAN_USE_LAMBDA_WEIGHT_TBL, FALSE);
#endif
      }
      else if (strcmp(tune_str, "ssim") == 0) {
#ifdef IS_MOZJPEG
        jpeg_c_set_int_param(&cinfo, JINT_BASE_QUANT_TBL_IDX, 1);
        jpeg_c_set_float_param(&cinfo, JFLOAT_LAMBDA_LOG_SCALE1, 11.5);
        jpeg_c_set_float_param(&cinfo, JFLOAT_LAMBDA_LOG_SCALE2, 12.75);
        jpeg_c_set_bool_param(&cinfo, JBOOLEAN_USE_LAMBDA_WEIGHT_TBL, FALSE);
#endif
      }
      else if (strcmp(tune_str, "ms-ssim") == 0) {
#ifdef IS_MOZJPEG
        jpeg_c_set_int_param(&cinfo, JINT_BASE_QUANT_TBL_IDX, 3);
        jpeg_c_set_float_param(&cinfo, JFLOAT_LAMBDA_LOG_SCALE1, 12.0);
        jpeg_c_set_float_param(&cinfo, JFLOAT_LAMBDA_LOG_SCALE2, 13.0);
        jpeg_c_set_bool_param(&cinfo, JBOOLEAN_USE_LAMBDA_WEIGHT_TBL, TRUE);
#endif
      }
      else if (strcmp(tune_str, "hvs-psnr") == 0) {
#ifdef IS_MOZJPEG
        jpeg_c_set_int_param(&cinfo, JINT_BASE_QUANT_TBL_IDX, 3);
        jpeg_c_set_float_param(&cinfo, JFLOAT_LAMBDA_LOG_SCALE1, 14.75);
        jpeg_c_set_float_param(&cinfo, JFLOAT_LAMBDA_LOG_SCALE2, 16.5);
        jpeg_c_set_bool_param(&cinfo, JBOOLEAN_USE_LAMBDA_WEIGHT_TBL, TRUE);
#endif
      }
      else {
        i_push_errorf(0, "unknown value '%s' for jpeg_tune", tune_str);
        goto fail;
      }
#ifndef IS_MOZJPEG
      i_push_error(0, "jpeg_tune requires Imager::File::JPEG be built with mozjpeg");
      goto fail;
#endif
    }
  }

  jpeg_set_quality(&cinfo, qfactor, TRUE);  /* limit to baseline-JPEG values */

  if (i_tags_get_int(&im->tags, "jpeg_jfif", 0, &jfif) && !jfif) {
    cinfo.write_JFIF_header = 0;
  }

  {
    int i;
    for (i = 0; i < sizeof(boolean_options) / sizeof(boolean_options[0]); ++i) {
      int val;
      const struct moz_option *opt = boolean_options + i;
      if (i_tags_get_int(&im->tags, opt->name, 0, &val)) {
#ifdef IS_MOZJPEG
	jpeg_c_set_bool_param(&cinfo, opt->tag, !!val);
#else
	i_push_errorf(0, "option %s requires Imager::File::JPEG be built with mozjpeg",
		      opt->name);
	goto fail;
#endif
      }
    }
  }

  {
    int i;
    for (i = 0; i < sizeof(float_options) / sizeof(float_options[0]); ++i) {
      double val;
      const struct moz_option *opt = float_options + i;
      if (i_tags_get_float(&im->tags, opt->name, 0, &val)) {
#ifdef IS_MOZJPEG
	float f = val;
	float g;
	jpeg_c_set_float_param(&cinfo, opt->tag, f);
	g = jpeg_c_get_float_param(&cinfo, opt->tag);
	if (fabs(g-f) > 0.0001) {
	  i_push_errorf(0, "invalid value %g for tag %s", val, opt->name);
	}
#else
	i_push_errorf(0, "option %s requires Imager::File::JPEG be built with mozjpeg",
		      opt->name);
	goto fail;
#endif
      }
    }
  }

    {
    int i;
    for (i = 0; i < sizeof(int_options) / sizeof(int_options[0]); ++i) {
      int val;
      const struct moz_option *opt = int_options + i;
      if (i_tags_get_int(&im->tags, opt->name, 0, &val)) {
#ifdef IS_MOZJPEG
	jpeg_c_set_int_param(&cinfo, opt->tag, val);
	if (jpeg_c_get_int_param(&cinfo, opt->tag) != val) {
	  i_push_errorf(0, "invalid value %d for tag %s", val, opt->name);
	}
#else
	i_push_errorf(0, "option %s requires Imager::File::JPEG be built with mozjpeg",
		      opt->name);
	goto fail;
#endif
      }
    }
  }

  if (!i_tags_get_int(&im->tags, "jpeg_progressive", 0, &progressive))
    progressive = 0;
  if (progressive) {
    jpeg_simple_progression(&cinfo);
  }
  if (!i_tags_get_int(&im->tags, "jpeg_optimize", 0, &optimize))
    optimize = 0;
  cinfo.optimize_coding = optimize;
  if (i_tags_get_int(&im->tags, "jpeg_arithmetic", 0, &arithmetic)) {
    cinfo.arith_code = arithmetic != 0;
  }

  got_xres = i_tags_get_float(&im->tags, "i_xres", 0, &xres);
  got_yres = i_tags_get_float(&im->tags, "i_yres", 0, &yres);
  if (!i_tags_get_int(&im->tags, "i_aspect_only", 0,&aspect_only))
    aspect_only = 0;
  if (!i_tags_get_int(&im->tags, "jpeg_density_unit", 0, &resunit))
    resunit = 1; /* per inch */
  if (resunit < 0 || resunit > 2) /* default to inch if invalid */
    resunit = 1;
  if (got_xres || got_yres) {
    if (!got_xres)
      xres = yres;
    else if (!got_yres)
      yres = xres;
    if (aspect_only)
      resunit = 0; /* standard tags override format tags */
    if (resunit == 2) {
      /* convert to per cm */
      xres /= 2.54;
      yres /= 2.54;
    }
    cinfo.density_unit = resunit;
    cinfo.X_density = (int)(xres + 0.5);
    cinfo.Y_density = (int)(yres + 0.5);
  }

  {
    int smooth;
    if (i_tags_get_int(&im->tags, "jpeg_smooth", 0, &smooth)) {
      if (smooth < 0 || smooth > 100) {
        i_push_error(0, "jpeg_smooth must be an integer from 0 to 100");
        goto fail;
      }
      cinfo.smoothing_factor = smooth;
    }
  }

  {
    char restart_str[20];
    if (i_tags_get_string(&im->tags, "jpeg_restart", 0, restart_str, sizeof(restart_str))) {
      long restart_count;
      char block_flag = '\0';
      if (sscanf(restart_str, "%ld%c", &restart_count, &block_flag) > 0
          && restart_count >= 0 && restart_count <= 65535
          && (block_flag == '\0' || block_flag == 'b' || block_flag == 'B')) {
        if (block_flag) {
          cinfo.restart_interval = (unsigned)restart_count;
        }
        else {
          cinfo.restart_in_rows = (int)restart_count;
        }
      }
      else {
        i_push_error(0, "jpeg_restart must be an integer from 0 to 65535 followed by an optional b");
        goto fail;
      }
    }
  }

  {
    char sample_str[80];

    if (i_tags_get_string(&im->tags, "jpeg_sample", 0, sample_str, sizeof(sample_str))) {
      int x, y, n;
      char sep;
      char *p = sample_str;
      int index = 0;
      while (*p) {
        if (sscanf(p, "%d%c%d%n", &x, &sep, &y, &n) == 3 &&
            x >= 1 && x <= 4 && y >= 1 && y <= 4 &&
            (sep == 'x' || sep == 'X') &&
            index < MAX_COMPONENTS) {
          cinfo.comp_info[index].h_samp_factor = x;
          cinfo.comp_info[index].v_samp_factor = y;
          ++index;
        }
        else {
        failsample:
          i_push_error(0, "jpeg_sample: must match /^[1-4]x[1-4](,[1-4]x[1-4]){0,9}$/aai");
          goto fail;
        }
        p += n;
        if (p[0] == ',') {
          if (!p[1])
            goto failsample;
          ++p;
        }
        else if (p[0]) {
          goto failsample;
        }
      }
      /* fill the rest with 1x1 like cjpeg does */
      for ( ; index < MAX_COMPONENTS; ++index) {
        cinfo.comp_info[index].h_samp_factor = x;
        cinfo.comp_info[index].v_samp_factor = y;
      }
    }
  }

  jpeg_start_compress(&cinfo, TRUE);

  if (i_tags_find(&im->tags, "jpeg_comment", 0, &comment_entry)) {
    jpeg_write_marker(&cinfo, JPEG_COM, 
                      (const JOCTET *)im->tags.tags[comment_entry].data,
		      im->tags.tags[comment_entry].size);
  }

  row_stride = im->xsize * im->channels;	/* JSAMPLEs per row in image_buffer */

  if (!i_img_virtual(im) && im->type == i_direct_type && im->bits == i_8_bits
      && im->channels == want_channels) {
    image_buffer=im->idata;

    while (cinfo.next_scanline < cinfo.image_height) {
      /* jpeg_write_scanlines expects an array of pointers to scanlines.
       * Here the array is only one element long, but you could pass
       * more than one scanline at a time if that's more convenient.
       */
      row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
      (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
  }
  else {
    i_color bg;

    i_get_file_background(im, &bg);
    data = mymalloc(im->xsize * im->channels);
    if (data) {
      while (cinfo.next_scanline < cinfo.image_height) {
        /* jpeg_write_scanlines expects an array of pointers to scanlines.
         * Here the array is only one element long, but you could pass
         * more than one scanline at a time if that's more convenient.
         */
        i_gsamp_bg(im, 0, im->xsize, cinfo.next_scanline, data, 
		   want_channels, &bg);
        row_pointer[0] = data;
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
      }
      myfree(data);
    }
    else {
      jpeg_destroy_compress(&cinfo);
      i_push_error(0, "out of memory");
      return 0; /* out of memory? */
    }
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);

  jpeg_destroy_compress(&cinfo);

  if (i_io_close(ig))
    return 0;

  return(1);
}

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson, addi@umich.edu

=head1 SEE ALSO

Imager(3)

=cut
*/
