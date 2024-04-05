#include <tiffio.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "imtiff.h"
#include "imext.h"

/* 4.3.0 (aka 20210416) made the old tifflib specific types deprecated in
   favour of the C99 <inttypes.h> types.
*/
#if TIFFLIB_VERSION >= 20210416
typedef uint8_t tf_uint8;
typedef uint16_t tf_uint16;
typedef uint32_t tf_uint32;
#else
typedef uint8 tf_uint8;
typedef uint16 tf_uint16;
typedef uint32 tf_uint32;
#endif

#if TIFFLIB_VERSION >= 20221213
#  define USE_TIFFOPEN_OPTIONS
#endif

/*
=head1 NAME

tiff.c - implements reading and writing tiff files, uses io layer.

=head1 SYNOPSIS

   io_glue *ig = io_new_fd( fd );
   i_img *im   = i_readtiff_wiol(ig, -1); // no limit on how much is read
   // or 
   io_glue *ig = io_new_fd( fd );
   return_code = i_writetiff_wiol(im, ig); 

=head1 DESCRIPTION

tiff.c implements the basic functions to read and write tiff files.
It uses the iolayer and needs either a seekable source or an entire
memory mapped buffer.

=head1 FUNCTION REFERENCE

Some of these functions are internal.

=over

=cut
*/

#define CLAMP8(x) ((x) < 0 ? 0 : (x) > 255 ? 255 : (x))
#define CLAMP16(x) ((x) < 0 ? 0 : (x) > 65535 ? 65535 : (x))

#define Sample16To8(num) ((num) / 257)

struct tag_name {
  const char *name;
  tf_uint32 tag;
};

static i_img *read_one_rgb_tiled(TIFF *tif, i_img_dim width, i_img_dim height, int allow_incomplete);
static i_img *read_one_rgb_lines(TIFF *tif, i_img_dim width, i_img_dim height, int allow_incomplete);

static const struct tag_name
text_tag_names[] =
{
  { "tiff_documentname", TIFFTAG_DOCUMENTNAME, },
  { "tiff_imagedescription", TIFFTAG_IMAGEDESCRIPTION, },
  { "tiff_make", TIFFTAG_MAKE, },
  { "tiff_model", TIFFTAG_MODEL, },
  { "tiff_pagename", TIFFTAG_PAGENAME, },
  { "tiff_software", TIFFTAG_SOFTWARE, },
  { "tiff_datetime", TIFFTAG_DATETIME, },
  { "tiff_artist", TIFFTAG_ARTIST, },
  { "tiff_hostcomputer", TIFFTAG_HOSTCOMPUTER, },
};

static const struct tag_name
compress_values[] =
  {
    { "none",     COMPRESSION_NONE },
    { "ccittrle", COMPRESSION_CCITTRLE },
    { "fax3",     COMPRESSION_CCITTFAX3 },
    { "t4",       COMPRESSION_CCITTFAX3 },
    { "fax4",     COMPRESSION_CCITTFAX4 },
    { "t6",       COMPRESSION_CCITTFAX4 },
    { "lzw",      COMPRESSION_LZW },
    { "oldjpeg",  COMPRESSION_OJPEG },
    { "jpeg",     COMPRESSION_JPEG },
    { "next",     COMPRESSION_NEXT },
    { "packbits", COMPRESSION_PACKBITS },
    { "thunder",  COMPRESSION_THUNDERSCAN },
    { "deflate",  COMPRESSION_ADOBE_DEFLATE },
    { "zip",      COMPRESSION_ADOBE_DEFLATE },
    { "pixarlog", COMPRESSION_PIXARLOG },
    { "oldzip",   COMPRESSION_DEFLATE },
    { "ccittrlew", COMPRESSION_CCITTRLEW },
    { "jbig",     COMPRESSION_JBIG },
    { "sgilog",   COMPRESSION_SGILOG },
    { "sgilog24", COMPRESSION_SGILOG24 },
#ifdef COMPRESSION_LERC
    { "lerc",     COMPRESSION_LERC },
#endif
    { "lzma",     COMPRESSION_LZMA },
#ifdef COMPRESSION_ZSTD
    { "zstd",     COMPRESSION_ZSTD },
#endif
#ifdef COMPRESSION_WEBP
    { "webp",     COMPRESSION_WEBP },
#endif
  };

static const int compress_value_count = 
  sizeof(compress_values) / sizeof(*compress_values);

static const struct tag_name
sample_format_values[] =
  {
    { "uint",      SAMPLEFORMAT_UINT },
    { "int",       SAMPLEFORMAT_INT },
    { "ieeefp",    SAMPLEFORMAT_IEEEFP },
    { "undefined", SAMPLEFORMAT_VOID },
  };

static const int sample_format_value_count = 
  sizeof(sample_format_values) / sizeof(*sample_format_values);

typedef struct read_state_tag read_state_t;
/* the setup function creates the image object, allocates the line buffer */
typedef int (*read_setup_t)(read_state_t *state);

/* the putter writes the image data provided by the getter to the
   image, x, y, width, height describe the target area of the image,
   extras is the extra number of pixels stored for each scanline in
   the raster buffer, (for tiles against the right side of the
   image) */

typedef int (*read_putter_t)(read_state_t *state, i_img_dim x, i_img_dim y,
			     i_img_dim width, i_img_dim height, int extras);

/* reads from a tiled or strip image and calls the putter.
   This may need a second type for handling non-contiguous images
   at some point */
typedef int (*read_getter_t)(read_state_t *state, read_putter_t putter);

struct read_state_tag {
  TIFF *tif;
  i_img *img;
  void *raster;
  i_img_dim pixels_read;
  int allow_incomplete;
  void *line_buf;
  tf_uint32 width, height;
  tf_uint16 bits_per_sample;
  tf_uint16 photometric;

  /* the total number of channels (samples per pixel) */
  int samples_per_pixel;

  /* if non-zero, which channel is the alpha channel, typically 3 for rgb */
  int alpha_chan;

  /* whether or not to scale the color channels based on the alpha
     channel.  TIFF has 2 types of alpha channel, if the alpha channel
     we use is EXTRASAMPLE_ASSOCALPHA then the color data will need to
     be scaled to match Imager's conventions */
  int scale_alpha;

  /* number of color samples (not including alpha) */
  int color_channels;

  /* SampleFormat is 2 */
  int sample_signed;

  int sample_format;
};

static int tile_contig_getter(read_state_t *state, read_putter_t putter);
static int strip_contig_getter(read_state_t *state, read_putter_t putter);

static int setup_paletted(read_state_t *state);
static int paletted_putter8(read_state_t *, i_img_dim, i_img_dim, i_img_dim, i_img_dim, int);
static int paletted_putter4(read_state_t *, i_img_dim, i_img_dim, i_img_dim, i_img_dim, int);

static int setup_16_rgb(read_state_t *state);
static int setup_16_grey(read_state_t *state);
static int putter_16(read_state_t *, i_img_dim, i_img_dim, i_img_dim, i_img_dim, int);

static int setup_8_rgb(read_state_t *state);
static int setup_8_grey(read_state_t *state);
static int putter_8(read_state_t *, i_img_dim, i_img_dim, i_img_dim, i_img_dim, int);

static int setup_32_rgb(read_state_t *state);
static int setup_32_grey(read_state_t *state);
static int putter_32(read_state_t *, i_img_dim, i_img_dim, i_img_dim, i_img_dim, int);

static int setup_bilevel(read_state_t *state);
static int putter_bilevel(read_state_t *, i_img_dim, i_img_dim, i_img_dim, i_img_dim, int);

static int setup_cmyk8(read_state_t *state);
static int putter_cmyk8(read_state_t *, i_img_dim, i_img_dim, i_img_dim, i_img_dim, int);

static int setup_cmyk16(read_state_t *state);
static int putter_cmyk16(read_state_t *, i_img_dim, i_img_dim, i_img_dim, i_img_dim, int);
static void
rgb_channels(read_state_t *state, int *out_channels);
static void
grey_channels(read_state_t *state, int *out_channels);
static void
cmyk_channels(read_state_t *state, int *out_channels);
static void
fallback_rgb_channels(TIFF *tif, int *channels, int *alpha_chan);

static const int text_tag_count = 
  sizeof(text_tag_names) / sizeof(*text_tag_names);

#define TIFFIO_MAGIC 0xC6A340CC

typedef struct {
  unsigned magic;
  io_glue *ig;
  io_glue *warn_buf;
} tiffio_context_t;

static void
do_warn_handler(tiffio_context_t *c, const char *module, const char *fmt, va_list ap) {
  char buf[200];

  if (c->magic != TIFFIO_MAGIC)
    return;

  buf[0] = '\0';
  vsnprintf(buf, sizeof(buf), fmt, ap);
  mm_log((1, "tiff warning %s\n", buf));

  if (!c->warn_buf)
    c->warn_buf = io_new_bufchain();

  i_io_write(c->warn_buf, module, strlen(module));
  i_io_write(c->warn_buf, ": ", 2);
  i_io_write(c->warn_buf, buf, strlen(buf));
  i_io_write(c->warn_buf, "\n", 1);
}

#ifdef USE_TIFFOPEN_OPTIONS

static int
error_handler_extr(TIFF *tif, void *user_data, const char *module,
                   const char *fmt, va_list ap) {
  (void)tif;
  (void)user_data;
  (void)module;
  mm_log((1, "tiff error fmt %s\n", fmt));
  i_push_errorvf(0, fmt, ap);
  return 1;
}

static int
warn_handler_extr(TIFF *tif, void *user_data, const char *module,
                   const char *fmt, va_list ap) {
  tiffio_context_t *c = (tiffio_context_t *)user_data;

  do_warn_handler(c, module, fmt, ap);

  return 1;
}

#else

static void
error_handler(char const *module, char const *fmt, va_list ap) {
  (void)module;
  mm_log((1, "tiff error fmt %s\n", fmt));
  i_push_errorvf(0, fmt, ap);
}

static void
warn_handler_ex(thandle_t h, const char *module, const char *fmt, va_list ap) {
  tiffio_context_t *c = (tiffio_context_t *)h;

  do_warn_handler(c, module, fmt, ap);
}

#endif

static void
tiffio_context_init(tiffio_context_t *c, io_glue *ig);
static void
tiffio_context_final(tiffio_context_t *c);

static toff_t
sizeproc(thandle_t h) {
  io_glue *ig = ((tiffio_context_t *)h)->ig;

  /* iolayer doesn't have a size callback, use seek() to find
     the end
  */
  toff_t orig_off = i_io_seek(ig, 0, SEEK_CUR);
  if (orig_off < 0) {
    i_push_error(errno, "seek to current failed");
    return -1;
  }
  off_t size = i_io_seek(ig, 0, SEEK_END);
  if (size < 0) {
    i_push_error(errno, "seek to end failed");
    return -1;
  }
  if (i_io_seek(ig, orig_off, SEEK_SET) < 0) {
    i_push_error(errno, "seek restore failed");
    return -1;
  }
  return size;
}


/*
=item comp_seek(h, o, w)

Compatability for 64 bit systems like latest freebsd (internal)

   h - tiff handle, cast an io_glue object
   o - offset
   w - whence

=cut
*/

static
toff_t
comp_seek(thandle_t h, toff_t o, int w) {
  io_glue *ig = ((tiffio_context_t *)h)->ig;
  return (toff_t) i_io_seek(ig, o, w);
}

/*
=item comp_mmap(thandle_t, tdata_t*, toff_t*)

Dummy mmap stub.

This shouldn't ever be called but newer tifflibs want it anyway.

=cut
*/

static 
int
comp_mmap(thandle_t h, tdata_t*p, toff_t*off) {
  (void)h;
  (void)p;
  (void)off;
  return -1;
}

/*
=item comp_munmap(thandle_t h, tdata_t p, toff_t off)

Dummy munmap stub.

This shouldn't ever be called but newer tifflibs want it anyway.

=cut
*/

static void
comp_munmap(thandle_t h, tdata_t p, toff_t off) {
  (void)h;
  (void)p;
  (void)off;
  /* do nothing */
}

static tsize_t
comp_read(thandle_t h, tdata_t p, tsize_t size) {
  return i_io_read(((tiffio_context_t *)h)->ig, p, size);
}

static tsize_t
comp_write(thandle_t h, tdata_t p, tsize_t size) {
  return i_io_write(((tiffio_context_t *)h)->ig, p, size);
}

static int
comp_close(thandle_t h) {
  return i_io_close(((tiffio_context_t *)h)->ig);
}

#ifndef USE_TIFFOPEN_OPTIONS
static i_mutex_t mutex;
#endif

typedef struct {
  TIFF *tif;
  tiffio_context_t ctx;
#ifndef USE_TIFFOPEN_OPTIONS
  TIFFErrorHandler old_error;
  TIFFErrorHandler old_warn;
  TIFFErrorHandlerExt old_warn_ext;
#endif
} tiff_state;

static TIFF *
do_tiff_open(tiff_state *state, io_glue *ig, const char *mode) {
  memset(state, 0, sizeof(*state));
  tiffio_context_init(&state->ctx, ig);
#ifdef USE_TIFFOPEN_OPTIONS
  TIFFOpenOptions *options = TIFFOpenOptionsAlloc();
  TIFFOpenOptionsSetErrorHandlerExtR(options, error_handler_extr, &state->ctx);
  TIFFOpenOptionsSetWarningHandlerExtR(options, warn_handler_extr, &state->ctx);
  TIFF *tif = TIFFClientOpenExt("(Iolayer)", 
		       mode, 
		       (thandle_t) &state->ctx,
		       comp_read,
		       comp_write,
		       comp_seek,
		       comp_close,
		       sizeproc,
		       comp_mmap,
                       comp_munmap,
                       options);
  TIFFOpenOptionsFree(options);
#else
  i_mutex_lock(mutex);

  state->old_error = TIFFSetErrorHandler(error_handler);
  state->old_warn = TIFFSetWarningHandler(NULL);
  state->old_warn_ext = TIFFSetWarningHandlerExt(warn_handler_ex);

  TIFF *tif = TIFFClientOpen("(Iolayer)", 
		       mode, 
		       (thandle_t) &state->ctx,
		       comp_read,
		       comp_write,
		       comp_seek,
		       comp_close,
		       sizeproc,
		       comp_mmap,
		       comp_munmap);
  if (!tif) {
    TIFFSetErrorHandler(state->old_error);
    TIFFSetWarningHandler(state->old_warn);
    TIFFSetWarningHandlerExt(state->old_warn_ext);
    i_mutex_unlock(mutex);
  }
#endif
  if (!tif) {
    tiffio_context_final(&state->ctx);
  }
  state->tif = tif;

  return tif;
}

static void
do_tiff_close(tiff_state *state) {
  TIFFClose(state->tif);
#ifndef USE_TIFFOPEN_OPTIONS
  TIFFSetErrorHandler(state->old_error);
  TIFFSetWarningHandler(state->old_warn);
  TIFFSetWarningHandlerExt(state->old_warn_ext);
  i_mutex_unlock(mutex);
#endif
  tiffio_context_final(&state->ctx);
}

void
i_tiff_init(void) {
#ifndef USE_TIFFOPEN_OPTIONS
  mutex = i_mutex_new();
#endif
}

static int save_tiff_tags(TIFF *tif, i_img *im);

static void 
pack_4bit_to(unsigned char *dest, const unsigned char *src, i_img_dim count);


static i_img *read_one_tiff(TIFF *tif, int allow_incomplete) {
  i_img *im;
  tf_uint32 width, height;
  tf_uint16 samples_per_pixel;
  int tiled;
  float xres, yres;
  tf_uint16 resunit;
  int gotXres, gotYres;
  tf_uint16 photometric;
  tf_uint16 bits_per_sample;
  tf_uint16 planar_config;
  tf_uint16 inkset;
  tf_uint16 compress;
  tf_uint16 sample_format;
  int i;
  read_state_t state;
  read_setup_t setupf = NULL;
  read_getter_t getterf = NULL;
  read_putter_t putterf = NULL;
  int channels = MAXCHANNELS;
  size_t sample_size = ~0; /* force failure if some code doesn't set it */
  i_img_dim total_pixels;
  int samples_integral;

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
  tiled = TIFFIsTiled(tif);
  TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);
  TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
  TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar_config);
  TIFFGetFieldDefaulted(tif, TIFFTAG_INKSET, &inkset);

  if (samples_per_pixel == 0) {
    i_push_error(0, "invalid image: SamplesPerPixel is 0");
    return NULL;
  }

  TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sample_format);

  mm_log((1, "i_readtiff_wiol: width=%d, height=%d, channels=%d\n", width, height, samples_per_pixel));
  mm_log((1, "i_readtiff_wiol: %stiled\n", tiled?"":"not "));
  mm_log((1, "i_readtiff_wiol: %sbyte swapped\n", TIFFIsByteSwapped(tif)?"":"not "));

  total_pixels = (i_img_dim)width * height;
  memset(&state, 0, sizeof(state));
  state.tif = tif;
  state.allow_incomplete = allow_incomplete;
  state.width = width;
  state.height = height;
  state.bits_per_sample = bits_per_sample;
  state.samples_per_pixel = samples_per_pixel;
  state.photometric = photometric;
  state.sample_signed = sample_format == SAMPLEFORMAT_INT;
  state.sample_format = sample_format;

  samples_integral = sample_format == SAMPLEFORMAT_UINT
    || sample_format == SAMPLEFORMAT_INT 
    || sample_format == SAMPLEFORMAT_VOID;  /* sample as UINT */

  /* yes, this if() is horrible */
  if (photometric == PHOTOMETRIC_PALETTE && bits_per_sample <= 8
      && samples_integral) {
    setupf = setup_paletted;
    if (bits_per_sample == 8)
      putterf = paletted_putter8;
    else if (bits_per_sample == 4)
      putterf = paletted_putter4;
    else
      mm_log((1, "unsupported paletted bits_per_sample %d\n", bits_per_sample));

    sample_size = sizeof(i_sample_t);
    channels = 1;
  }
  else if (bits_per_sample == 16 
	   && photometric == PHOTOMETRIC_RGB
	   && samples_per_pixel >= 3
	   && samples_integral) {
    setupf = setup_16_rgb;
    putterf = putter_16;
    sample_size = 2;
    rgb_channels(&state, &channels);
  }
  else if (bits_per_sample == 16
	   && photometric == PHOTOMETRIC_MINISBLACK
	   && samples_integral) {
    setupf = setup_16_grey;
    putterf = putter_16;
    sample_size = 2;
    grey_channels(&state, &channels);
  }
  else if (bits_per_sample == 8
	   && photometric == PHOTOMETRIC_MINISBLACK
	   && samples_integral) {
    setupf = setup_8_grey;
    putterf = putter_8;
    sample_size = 1;
    grey_channels(&state, &channels);
  }
  else if (bits_per_sample == 8
	   && photometric == PHOTOMETRIC_RGB
	   && samples_integral) {
    setupf = setup_8_rgb;
    putterf = putter_8;
    sample_size = 1;
    rgb_channels(&state, &channels);
  }
  else if (bits_per_sample == 32 
	   && photometric == PHOTOMETRIC_RGB
	   && samples_per_pixel >= 3) {
    setupf = setup_32_rgb;
    putterf = putter_32;
    sample_size = sizeof(i_fsample_t);
    rgb_channels(&state, &channels);
  }
  else if (bits_per_sample == 32
	   && photometric == PHOTOMETRIC_MINISBLACK) {
    setupf = setup_32_grey;
    putterf = putter_32;
    sample_size = sizeof(i_fsample_t);
    grey_channels(&state, &channels);
  }
  else if (bits_per_sample == 1
	   && (photometric == PHOTOMETRIC_MINISBLACK
	       || photometric == PHOTOMETRIC_MINISWHITE)
	   && samples_per_pixel == 1) {
    setupf = setup_bilevel;
    putterf = putter_bilevel;
    sample_size = sizeof(i_palidx);
    channels = 1;
  }
  else if (bits_per_sample == 8
	   && photometric == PHOTOMETRIC_SEPARATED
	   && inkset == INKSET_CMYK
	   && samples_per_pixel >= 4
	   && samples_integral) {
    setupf = setup_cmyk8;
    putterf = putter_cmyk8;
    sample_size = 1;
    cmyk_channels(&state, &channels);
  }
  else if (bits_per_sample == 16
	   && photometric == PHOTOMETRIC_SEPARATED
	   && inkset == INKSET_CMYK
	   && samples_per_pixel >= 4
	   && samples_integral) {
    setupf = setup_cmyk16;
    putterf = putter_cmyk16;
    sample_size = 2;
    cmyk_channels(&state, &channels);
  }
  else {
    int alpha;
    fallback_rgb_channels(tif, &channels, &alpha);
    sample_size = 1;
  }

  if (!i_int_check_image_file_limits(width, height, channels, sample_size)) {
    return NULL;
  }

  if (tiled) {
    if (planar_config == PLANARCONFIG_CONTIG)
      getterf = tile_contig_getter;
  }
  else {
    if (planar_config == PLANARCONFIG_CONTIG)
      getterf = strip_contig_getter;
  }
  if (setupf && getterf && putterf) {

    if (!setupf(&state))
      return NULL;
    if (!getterf(&state, putterf) || !state.pixels_read) {
      if (state.img)
	i_img_destroy(state.img);
      if (state.raster)
	_TIFFfree(state.raster);
      if (state.line_buf)
	myfree(state.line_buf);
      
      return NULL;
    }

    if (allow_incomplete && state.pixels_read < total_pixels) {
      i_tags_setn(&(state.img->tags), "i_incomplete", 1);
      i_tags_setn(&(state.img->tags), "i_lines_read", 
		  state.pixels_read / width);
    }
    im = state.img;
    
    if (state.raster)
      _TIFFfree(state.raster);
    if (state.line_buf)
      myfree(state.line_buf);
  }
  else {
    if (tiled) {
      im = read_one_rgb_tiled(tif, width, height, allow_incomplete);
    }
    else {
      im = read_one_rgb_lines(tif, width, height, allow_incomplete);
    }
  }

  if (!im)
    return NULL;

  /* general metadata */
  i_tags_setn(&im->tags, "tiff_bitspersample", bits_per_sample);
  i_tags_setn(&im->tags, "tiff_photometric", photometric);
  TIFFGetFieldDefaulted(tif, TIFFTAG_COMPRESSION, &compress);
    
  /* resolution tags */
  TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
  gotXres = TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres);
  gotYres = TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres);
  if (gotXres || gotYres) {
    if (!gotXres)
      xres = yres;
    else if (!gotYres)
      yres = xres;
    i_tags_setn(&im->tags, "tiff_resolutionunit", resunit);
    if (resunit == RESUNIT_CENTIMETER) {
      /* from dots per cm to dpi */
      xres *= 2.54;
      yres *= 2.54;
      i_tags_set(&im->tags, "tiff_resolutionunit_name", "centimeter", -1);
    }
    else if (resunit == RESUNIT_NONE) {
      i_tags_setn(&im->tags, "i_aspect_only", 1);
      i_tags_set(&im->tags, "tiff_resolutionunit_name", "none", -1);
    }
    else if (resunit == RESUNIT_INCH) {
      i_tags_set(&im->tags, "tiff_resolutionunit_name", "inch", -1);
    }
    else {
      i_tags_set(&im->tags, "tiff_resolutionunit_name", "unknown", -1);
    }
    /* tifflib doesn't seem to provide a way to get to the original rational
       value of these, which would let me provide a more reasonable
       precision. So make up a number. */
    i_tags_set_float2(&im->tags, "i_xres", 0, xres, 6);
    i_tags_set_float2(&im->tags, "i_yres", 0, yres, 6);
  }

  /* Text tags */
  for (i = 0; i < text_tag_count; ++i) {
    char *data;
    if (TIFFGetField(tif, text_tag_names[i].tag, &data)) {
      mm_log((1, "i_readtiff_wiol: tag %d has value %s\n", 
	      text_tag_names[i].tag, data));
      i_tags_set(&im->tags, text_tag_names[i].name, data, -1);
    }
  }

  i_tags_set(&im->tags, "i_format", "tiff", 4);
  {
    tiffio_context_t *ctx = TIFFClientdata(tif);
    if (ctx->warn_buf) {
      i_io_flush(ctx->warn_buf);
      unsigned char *data = NULL;
      size_t len = io_slurp(ctx->warn_buf, &data);

      i_tags_set(&im->tags, "i_warning", (const char *)data, len);
      io_glue_destroy(ctx->warn_buf);
      ctx->warn_buf = NULL;
    }
  }

  for (i = 0; i < compress_value_count; ++i) {
    if (compress_values[i].tag == compress) {
      i_tags_set(&im->tags, "tiff_compression", compress_values[i].name, -1);
      break;
    }
  }

  if (TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &sample_format)) {
    /* only set the tag if the the TIFF tag is present */
    i_tags_setn(&im->tags, "tiff_sample_format", sample_format);

    for (i = 0; i < sample_format_value_count; ++i) {
      if (sample_format_values[i].tag == sample_format) {
	i_tags_set(&im->tags, "tiff_sample_format_name",
		   sample_format_values[i].name, -1);
	break;
      }
    }
  }

  return im;
}

/*
=item i_readtiff_wiol(ig, allow_incomplete, page)

=cut
*/
i_img*
i_readtiff_wiol(io_glue *ig, int allow_incomplete, int page) {
  int current_page;

  i_clear_error();

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */

  mm_log((1, "i_readtiff_wiol(ig %p, allow_incomplete %d, page %d)\n", ig, allow_incomplete, page));

  tiff_state ts;
  TIFF *tif = do_tiff_open(&ts, ig, "rm");
  
  if (!tif) {
    mm_log((1, "i_readtiff_wiol: Unable to open tif file\n"));
    i_push_error(0, "Error opening file");
    return NULL;
  }

  for (current_page = 0; current_page < page; ++current_page) {
    if (!TIFFReadDirectory(tif)) {
      mm_log((1, "i_readtiff_wiol: Unable to switch to directory %d\n", page));
      i_push_errorf(0, "could not switch to page %d", page);
      goto fail;
    }
  }

  i_img *im = read_one_tiff(tif, allow_incomplete);

  if (TIFFLastDirectory(tif))
    mm_log((1, "Last directory of tiff file\n"));

  do_tiff_close(&ts);

  return im;

 fail:
  do_tiff_close(&ts);
  return NULL;
}

/*
=item i_readtiff_multi_wiol(ig, *count)

Reads multiple images from a TIFF.

=cut
*/
i_img**
i_readtiff_multi_wiol(io_glue *ig, int *count) {
  i_img **results = NULL;
  int result_alloc = 0;


  i_clear_error();

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */

  mm_log((1, "i_readtiff_wiol(ig %p)\n", ig));

  tiff_state ts;
  TIFF *tif = do_tiff_open(&ts, ig, "rm");
  if (!tif) {
    mm_log((1, "i_readtiff_wiol: Unable to open tif file\n"));
    i_push_error(0, "Error opening file");
    return NULL;
  }

  *count = 0;
  do {
    i_img *im = read_one_tiff(tif, 0);
    if (!im)
      break;
    if (++*count > result_alloc) {
      if (result_alloc == 0) {
        result_alloc = 5;
        results = mymalloc(result_alloc * sizeof(i_img *));
      }
      else {
        i_img **newresults;
        result_alloc *= 2;
        newresults = myrealloc(results, result_alloc * sizeof(i_img *));
	if (!newresults) {
	  i_img_destroy(im); /* don't leak it */
	  break;
	}
	results = newresults;
      }
    }
    results[*count-1] = im;
  } while (TIFFReadDirectory(tif));

  do_tiff_close(&ts);

  return results;

 fail:

  do_tiff_close(&ts);
  return NULL;
}

undef_int
i_writetiff_low_faxable(TIFF *tif, i_img *im, int fine) {
  tf_uint32 width, height;
  tf_uint32 y;
  int rc;
  tf_uint32 x;
  tf_uint32 rowsperstrip;
  float vres = fine ? 196 : 98;
  int luma_chan;

  width    = im->xsize;
  height   = im->ysize;

  if (width != im->xsize || height != im->ysize) {
    i_push_error(0, "image too large for TIFF");
    return 0;
  }

  switch (im->channels) {
  case 1:
  case 2:
    luma_chan = 0;
    break;
  case 3:
  case 4:
    luma_chan = 1;
    break;
  default:
    /* This means a colorspace we don't handle yet */
    mm_log((1, "i_writetiff_wiol_faxable: don't handle %d channel images.\n", im->channels));
    return 0;
  }

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */


  mm_log((1, "i_writetiff_wiol_faxable: width=%d, height=%d, channels=%d\n", width, height, im->channels));
  
  if (!TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,      width)   )
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField width=%d failed\n", width)); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_IMAGELENGTH,     height)  )
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField length=%d failed\n", height)); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, (tf_uint16)1U))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField samplesperpixel=1 failed\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_ORIENTATION,  (tf_uint16)ORIENTATION_TOPLEFT))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField Orientation=topleft\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,   (tf_uint16)1U)        )
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField bitpersample=1\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_PLANARCONFIG, (tf_uint16)PLANARCONFIG_CONTIG))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField planarconfig\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, (tf_uint16)PHOTOMETRIC_MINISWHITE))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField photometric=%d\n", PHOTOMETRIC_MINISBLACK)); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_COMPRESSION, (tf_uint16)COMPRESSION_CCITTFAX3))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField compression=COMPRESSION_CCITTFAX3\n")); return 0; }

  size_t scan_line_size = TIFFScanlineSize(tif);
  rowsperstrip = TIFFDefaultStripSize(tif, -1);
  if (!TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rowsperstrip)) {
    mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField rowsperstrip=%u\n", rowsperstrip)); return 0; }

  TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rc);

  mm_log((1, "i_writetiff_wiol_faxable: TIFFGetField rowsperstrip=%u\n", rowsperstrip));
  mm_log((1, "i_writetiff_wiol_faxable: TIFFGetField scanlinesize=%zu\n", scan_line_size ));
  mm_log((1, "i_writetiff_wiol_faxable: TIFFGetField planarconfig=%d\n", PLANARCONFIG_CONTIG));

  if (!TIFFSetField(tif, TIFFTAG_XRESOLUTION, (float)204))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField Xresolution=204\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_YRESOLUTION, vres))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField Yresolution=196\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, (tf_uint16)RESUNIT_INCH)) {
    mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField ResolutionUnit=%d\n", RESUNIT_INCH)); return 0; 
  }

  if (!save_tiff_tags(tif, im)) {
    return 0;
  }

  unsigned char *linebuf = (unsigned char *)_TIFFmalloc( scan_line_size );
  
  for (y=0; y<height; y++) {
    int linebufpos=0;
    for(x=0; x<width; x+=8) { 
      int bits;
      int bitpos;
      i_sample_t luma[8];
      tf_uint8 bitval = 128;
      linebuf[linebufpos]=0;
      bits = width-x; if(bits>8) bits=8;
      i_gsamp(im, x, x+bits, y, luma, &luma_chan, 1);
      for(bitpos=0;bitpos<bits;bitpos++) {
	linebuf[linebufpos] |= ((luma[bitpos] < 128) ? bitval : 0);
	bitval >>= 1;
      }
      linebufpos++;
    }
    if (TIFFWriteScanline(tif, linebuf, y, 0) < 0) {
      mm_log((1, "i_writetiff_wiol_faxable: TIFFWriteScanline failed.\n"));
      goto fail;
    }
  }
  _TIFFfree(linebuf);

  return 1;

 fail:
  _TIFFfree(linebuf);
  return 0;
}

static tf_uint16
find_compression(char const *name, tf_uint16 *compress) {
  int i;

  for (i = 0; i < compress_value_count; ++i) {
    if (strcmp(compress_values[i].name, name) == 0) {
      *compress = (tf_uint16)compress_values[i].tag;
      return 1;
    }
  }
  *compress = COMPRESSION_NONE;

  return 0;
}

static tf_uint16
get_compression(i_img *im, tf_uint16 def_compress) {
  int entry;
  int value;

  if (i_tags_find(&im->tags, "tiff_compression", 0, &entry)
      && im->tags.tags[entry].data) {
    const char *name = im->tags.tags[entry].data;
    tf_uint16 compress;
    if (find_compression(name, &compress)
	&& TIFFIsCODECConfigured(compress))
      return compress;

    TIFFCodec *codecs = TIFFGetConfiguredCODECs();
    for (TIFFCodec *c = codecs; c->name; ++c) {
      if (strcmp(c->name, name) == 0) {
        tf_uint16 scheme = c->scheme;
        _TIFFfree(codecs);
        return scheme;
      }
    }
    _TIFFfree(codecs);
  }
  if (i_tags_get_int(&im->tags, "tiff_compression", 0, &value)) {
    if ((tf_uint16)value == value
	&& TIFFIsCODECConfigured((tf_uint16)value))
      return (tf_uint16)value;
  }

  return def_compress;
}

int
i_tiff_has_compression(const char *name) {
  tf_uint16 compress;

  if (!find_compression(name, &compress))
    return 0;

  return TIFFIsCODECConfigured(compress);
}

static int
set_base_tags(TIFF *tif, i_img *im, tf_uint16 compress, tf_uint16 photometric, 
	      tf_uint16 bits_per_sample, tf_uint16 samples_per_pixel) {
  double xres, yres;
  int resunit;
  int got_xres, got_yres;
  int aspect_only;

  if ((tf_uint32)im->xsize != im->xsize ||
      (tf_uint32)im->ysize != im->ysize) {
    i_push_error(0, "image too large for TIFF");
    return 0;
  }

  if (!TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (tf_uint32)im->xsize)) {
    i_push_error(0, "write TIFF: setting width tag");
    return 0;
  }
  if (!TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (tf_uint32)im->ysize)) {
    i_push_error(0, "write TIFF: setting length tag");
    return 0;
  }
  if (!TIFFSetField(tif, TIFFTAG_ORIENTATION, (tf_uint16)ORIENTATION_TOPLEFT)) {
    i_push_error(0, "write TIFF: setting orientation tag");
    return 0;
  }
  if (!TIFFSetField(tif, TIFFTAG_PLANARCONFIG, (tf_uint16)PLANARCONFIG_CONTIG)) {
    i_push_error(0, "write TIFF: setting planar configuration tag");
    return 0;
  }
  if (!TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, photometric)) {
    i_push_error(0, "write TIFF: setting photometric tag");
    return 0;
  }
  if (!TIFFSetField(tif, TIFFTAG_COMPRESSION, compress)) {
    i_push_error(0, "write TIFF: setting compression tag");
    return 0;
  }
  if (!TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample)) {
    i_push_error(0, "write TIFF: setting bits per sample tag");
    return 0;
  }
  if (!TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel)) {
    i_push_error(0, "write TIFF: setting samples per pixel tag");
    return 0;
  }

  got_xres = i_tags_get_float(&im->tags, "i_xres", 0, &xres);
  got_yres = i_tags_get_float(&im->tags, "i_yres", 0, &yres);
  if (!i_tags_get_int(&im->tags, "i_aspect_only", 0,&aspect_only))
    aspect_only = 0;
  if (!i_tags_get_int(&im->tags, "tiff_resolutionunit", 0, &resunit))
    resunit = RESUNIT_INCH;
  if (got_xres || got_yres) {
    if (!got_xres)
      xres = yres;
    else if (!got_yres)
      yres = xres;
    if (aspect_only) {
      resunit = RESUNIT_NONE;
    }
    else {
      if (resunit == RESUNIT_CENTIMETER) {
	xres /= 2.54;
	yres /= 2.54;
      }
      else {
	resunit  = RESUNIT_INCH;
      }
    }
    if (!TIFFSetField(tif, TIFFTAG_XRESOLUTION, (float)xres)) {
      i_push_error(0, "write TIFF: setting xresolution tag");
      return 0;
    }
    if (!TIFFSetField(tif, TIFFTAG_YRESOLUTION, (float)yres)) {
      i_push_error(0, "write TIFF: setting yresolution tag");
      return 0;
    }
    if (!TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, (tf_uint16)resunit)) {
      i_push_error(0, "write TIFF: setting resolutionunit tag");
      return 0;
    }
  }

  return 1;
}

static int 
write_one_bilevel(TIFF *tif, i_img *im, int zero_is_white) {
  tf_uint16 compress = get_compression(im, COMPRESSION_PACKBITS);
  tf_uint16 photometric;
  unsigned char *in_row;
  unsigned char *out_row;
  unsigned out_size;
  i_img_dim x, y;
  int invert;

  mm_log((1, "tiff - write_one_bilevel(tif %p, im %p, zero_is_white %d)\n", 
	  tif, im, zero_is_white));

  /* ignore a silly choice */
  if (compress == COMPRESSION_JPEG)
    compress = COMPRESSION_PACKBITS;

  switch (compress) {
  case COMPRESSION_CCITTRLE:
  case COMPRESSION_CCITTFAX3:
  case COMPRESSION_CCITTFAX4:
    /* natural fax photometric */
    photometric = PHOTOMETRIC_MINISWHITE;
    break;

  default:
    /* natural for most computer images */
    photometric = PHOTOMETRIC_MINISBLACK;
    break;
  }

  if (!set_base_tags(tif, im, compress, photometric, 1, 1))
    return 0;

  if (!TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, -1))) {
    i_push_error(0, "write TIFF: setting rows per strip tag");
    return 0; 
  }

  out_size = TIFFScanlineSize(tif);
  out_row = (unsigned char *)_TIFFmalloc( out_size );
  in_row = mymalloc(im->xsize);

  invert = (photometric == PHOTOMETRIC_MINISWHITE) != (zero_is_white != 0);

  for (y = 0; y < im->ysize; ++y) {
    int mask = 0x80;
    unsigned char *outp = out_row;
    memset(out_row, 0, out_size);
    i_gpal(im, 0, im->xsize, y, in_row);
    for (x = 0; x < im->xsize; ++x) {
      if (invert ? !in_row[x] : in_row[x]) {
	*outp |= mask;
      }
      mask >>= 1;
      if (!mask) {
	++outp;
	mask = 0x80;
      }
    }
    if (TIFFWriteScanline(tif, out_row, y, 0) < 0) {
      _TIFFfree(out_row);
      myfree(in_row);
      i_push_error(0, "write TIFF: write scan line failed");
      return 0;
    }
  }

  _TIFFfree(out_row);
  myfree(in_row);

  return 1;
}

static int
set_palette(TIFF *tif, i_img *im, int size) {
  int count;
  tf_uint16 *colors;
  tf_uint16 *out[3];
  i_color c;
  int i, ch;
  
  colors = (tf_uint16 *)_TIFFmalloc(sizeof(tf_uint16) * 3 * size);
  out[0] = colors;
  out[1] = colors + size;
  out[2] = colors + 2 * size;
    
  count = i_colorcount(im);
  for (i = 0; i < count; ++i) {
    i_getcolors(im, i, &c, 1);
    for (ch = 0; ch < 3; ++ch)
      out[ch][i] = c.channel[ch] * 257;
  }
  for (; i < size; ++i) {
    for (ch = 0; ch < 3; ++ch)
      out[ch][i] = 0;
  }
  if (!TIFFSetField(tif, TIFFTAG_COLORMAP, out[0], out[1], out[2])) {
    _TIFFfree(colors);
    i_push_error(0, "write TIFF: setting color map");
    return 0;
  }
  _TIFFfree(colors);
  
  return 1;
}

static int
write_one_paletted8(TIFF *tif, i_img *im) {
  tf_uint16 compress = get_compression(im, COMPRESSION_PACKBITS);
  unsigned char *out_row;
  unsigned out_size;
  i_img_dim y;

  mm_log((1, "tiff - write_one_paletted8(tif %p, im %p)\n", tif, im));

  /* ignore a silly choice */
  if (compress == COMPRESSION_JPEG ||
      compress == COMPRESSION_CCITTRLE ||
      compress == COMPRESSION_CCITTFAX3 ||
      compress == COMPRESSION_CCITTFAX4)
    compress = COMPRESSION_PACKBITS;

  if (!TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, -1))) {
    i_push_error(0, "write TIFF: setting rows per strip tag");
    return 0; 
  }

  if (!set_base_tags(tif, im, compress, PHOTOMETRIC_PALETTE, 8, 1))
    return 0;

  if (!set_palette(tif, im, 256))
    return 0;

  out_size = TIFFScanlineSize(tif);
  out_row = (unsigned char *)_TIFFmalloc( out_size );

  for (y = 0; y < im->ysize; ++y) {
    i_gpal(im, 0, im->xsize, y, out_row);
    if (TIFFWriteScanline(tif, out_row, y, 0) < 0) {
      _TIFFfree(out_row);
      i_push_error(0, "write TIFF: write scan line failed");
      return 0;
    }
  }

  _TIFFfree(out_row);

  return 1;
}

static int
write_one_paletted4(TIFF *tif, i_img *im) {
  tf_uint16 compress = get_compression(im, COMPRESSION_PACKBITS);
  unsigned char *in_row;
  unsigned char *out_row;
  size_t out_size;
  i_img_dim y;

  mm_log((1, "tiff - write_one_paletted4(tif %p, im %p)\n", tif, im));

  /* ignore a silly choice */
  if (compress == COMPRESSION_JPEG ||
      compress == COMPRESSION_CCITTRLE ||
      compress == COMPRESSION_CCITTFAX3 ||
      compress == COMPRESSION_CCITTFAX4)
    compress = COMPRESSION_PACKBITS;

  if (!set_base_tags(tif, im, compress, PHOTOMETRIC_PALETTE, 4, 1))
    return 0;

  if (!set_palette(tif, im, 16))
    return 0;

  if (!TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, -1))) {
    i_push_error(0, "write TIFF: setting rows per strip tag");
    return 0; 
  }

  in_row = mymalloc(im->xsize);
  out_size = TIFFScanlineSize(tif);
  out_row = (unsigned char *)_TIFFmalloc( out_size );

  for (y = 0; y < im->ysize; ++y) {
    i_gpal(im, 0, im->xsize, y, in_row);
    memset(out_row, 0, out_size);
    pack_4bit_to(out_row, in_row, im->xsize);
    if (TIFFWriteScanline(tif, out_row, y, 0) < 0) {
      _TIFFfree(out_row);
      i_push_error(0, "write TIFF: write scan line failed");
      return 0;
    }
  }

  myfree(in_row);
  _TIFFfree(out_row);

  return 1;
}

static int
set_direct_tags(TIFF *tif, i_img *im, tf_uint16 compress, 
		tf_uint16 bits_per_sample) {
  tf_uint16 extras = EXTRASAMPLE_ASSOCALPHA;
  tf_uint16 extra_count = im->channels == 2 || im->channels == 4;
  tf_uint16 photometric = im->channels >= 3 ? 
    PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK;

  if (!set_base_tags(tif, im, compress, photometric, bits_per_sample, 
		     im->channels)) {
    return 0;
  }
  
  if (extra_count) {
    if (!TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, extra_count, &extras)) {
      i_push_error(0, "write TIFF: setting extra samples tag");
      return 0;
    }
  }

  if (compress == COMPRESSION_JPEG) {
    int jpeg_quality;
    if (i_tags_get_int(&im->tags, "tiff_jpegquality", 0, &jpeg_quality)
	&& jpeg_quality >= 0 && jpeg_quality <= 100) {
      if (!TIFFSetField(tif, TIFFTAG_JPEGQUALITY, jpeg_quality)) {
	i_push_error(0, "write TIFF: setting jpeg quality pseudo-tag");
	return 0;
      }
    }
  }

  return 1;
}

static int 
write_one_32(TIFF *tif, i_img *im) {
  tf_uint16 compress = get_compression(im, COMPRESSION_PACKBITS);
  unsigned *in_row;
  size_t out_size;
  tf_uint32 *out_row;
  i_img_dim y;
  size_t sample_count = im->xsize * im->channels;
  size_t sample_index;
    
  mm_log((1, "tiff - write_one_32(tif %p, im %p)\n", tif, im));

  /* only 8 and 12 bit samples are supported by jpeg compression */
  if (compress == COMPRESSION_JPEG)
    compress = COMPRESSION_PACKBITS;

  if (!set_direct_tags(tif, im, compress, 32))
    return 0;

  in_row = mymalloc(sample_count * sizeof(unsigned));
  out_size = TIFFScanlineSize(tif);
  out_row = (tf_uint32 *)_TIFFmalloc( out_size );

  for (y = 0; y < im->ysize; ++y) {
    if (i_gsamp_bits(im, 0, im->xsize, y, in_row, NULL, im->channels, 32) <= 0) {
      i_push_error(0, "Cannot read 32-bit samples");
      return 0;
    }
    for (sample_index = 0; sample_index < sample_count; ++sample_index)
      out_row[sample_index] = in_row[sample_index];
    if (TIFFWriteScanline(tif, out_row, y, 0) < 0) {
      myfree(in_row);
      _TIFFfree(out_row);
      i_push_error(0, "write TIFF: write scan line failed");
      return 0;
    }
  }

  myfree(in_row);
  _TIFFfree(out_row);
  
  return 1;
}

static int 
write_one_16(TIFF *tif, i_img *im) {
  tf_uint16 compress = get_compression(im, COMPRESSION_PACKBITS);
  unsigned *in_row;
  size_t out_size;
  tf_uint16 *out_row;
  i_img_dim y;
  size_t sample_count = im->xsize * im->channels;
  size_t sample_index;
    
  mm_log((1, "tiff - write_one_16(tif %p, im %p)\n", tif, im));

  /* only 8 and 12 bit samples are supported by jpeg compression */
  if (compress == COMPRESSION_JPEG)
    compress = COMPRESSION_PACKBITS;

  if (!set_direct_tags(tif, im, compress, 16))
    return 0;

  in_row = mymalloc(sample_count * sizeof(unsigned));
  out_size = TIFFScanlineSize(tif);
  out_row = (tf_uint16 *)_TIFFmalloc( out_size );

  for (y = 0; y < im->ysize; ++y) {
    if (i_gsamp_bits(im, 0, im->xsize, y, in_row, NULL, im->channels, 16) <= 0) {
      i_push_error(0, "Cannot read 16-bit samples");
      return 0;
    }
    for (sample_index = 0; sample_index < sample_count; ++sample_index)
      out_row[sample_index] = in_row[sample_index];
    if (TIFFWriteScanline(tif, out_row, y, 0) < 0) {
      myfree(in_row);
      _TIFFfree(out_row);
      i_push_error(0, "write TIFF: write scan line failed");
      return 0;
    }
  }

  myfree(in_row);
  _TIFFfree(out_row);
  
  return 1;
}

static int 
write_one_8(TIFF *tif, i_img *im) {
  tf_uint16 compress = get_compression(im, COMPRESSION_PACKBITS);
  size_t out_size;
  unsigned char *out_row;
  i_img_dim y;
  size_t sample_count = im->xsize * im->channels;
    
  mm_log((1, "tiff - write_one_8(tif %p, im %p)\n", tif, im));

  if (!set_direct_tags(tif, im, compress, 8))
    return 0;

  out_size = TIFFScanlineSize(tif);
  if (out_size < sample_count)
    out_size = sample_count;
  out_row = (unsigned char *)_TIFFmalloc( out_size );

  for (y = 0; y < im->ysize; ++y) {
    if (i_gsamp(im, 0, im->xsize, y, out_row, NULL, im->channels) <= 0) {
      i_push_error(0, "Cannot read 8-bit samples");
      return 0;
    }
    if (TIFFWriteScanline(tif, out_row, y, 0) < 0) {
      _TIFFfree(out_row);
      i_push_error(0, "write TIFF: write scan line failed");
      return 0;
    }
  }
  _TIFFfree(out_row);
  
  return 1;
}

static int
i_writetiff_low(TIFF *tif, i_img *im) {
  tf_uint32 width, height;
  tf_uint16 channels;
  int zero_is_white;

  width    = im->xsize;
  height   = im->ysize;
  channels = im->channels;

  if (width != im->xsize || height != im->ysize) {
    i_push_error(0, "image too large for TIFF");
    return 0;
  }

  mm_log((1, "i_writetiff_low: width=%d, height=%d, channels=%d, bits=%d\n", width, height, channels, im->bits));
  if (im->type == i_palette_type) {
    mm_log((1, "i_writetiff_low: paletted, colors=%d\n", i_colorcount(im)));
  }
  
  if (i_img_is_monochrome(im, &zero_is_white)) {
    if (!write_one_bilevel(tif, im, zero_is_white))
      return 0;
  }
  else if (im->type == i_palette_type) {
    if (i_colorcount(im) <= 16) {
      if (!write_one_paletted4(tif, im))
	return 0;
    }
    else {
      if (!write_one_paletted8(tif, im))
	return 0;
    }
  }
  else if (im->bits > 16) {
    if (!write_one_32(tif, im))
      return 0;
  }
  else if (im->bits > 8) {
    if (!write_one_16(tif, im))
      return 0;
  }
  else {
    if (!write_one_8(tif, im))
      return 0;
  }

  if (!save_tiff_tags(tif, im))
    return 0;

  return 1;
}

/*
=item i_writetiff_multi_wiol(ig, imgs, count, fine_mode)

Stores an image in the iolayer object.

   ig - io_object that defines source to write to 
   imgs,count - the images to write

=cut 
*/

undef_int
i_writetiff_multi_wiol(io_glue *ig, i_img **imgs, int count) {
  int i;

  i_clear_error();
  mm_log((1, "i_writetiff_multi_wiol(ig %p, imgs %p, count %d)\n", 
          ig, imgs, count));

  
  tiff_state ts;
  TIFF *tif = do_tiff_open(&ts, ig, "wm");
    

  if (!tif) {
    mm_log((1, "i_writetiff_multi_wiol: Unable to open tif file for writing\n"));
    i_push_error(0, "Could not create TIFF object");
    return 0;
  }

  for (i = 0; i < count; ++i) {
    if (!i_writetiff_low(tif, imgs[i])) {
      do_tiff_close(&ts);
      return 0;
    }

    if (!TIFFWriteDirectory(tif)) {
      i_push_error(0, "Cannot write TIFF directory");
      do_tiff_close(&ts);
      return 0;
    }
  }

  do_tiff_close(&ts);

  if (i_io_close(ig))
    return 0;

  return 1;
}

/*
=item i_writetiff_multi_wiol_faxable(ig, imgs, count, fine_mode)

Stores an image in the iolayer object.

   ig - io_object that defines source to write to 
   imgs,count - the images to write
   fine_mode - select fine or normal mode fax images

=cut 
*/


undef_int
i_writetiff_multi_wiol_faxable(io_glue *ig, i_img **imgs, int count, int fine) {
  int i;

  i_clear_error();
  mm_log((1, "i_writetiff_multi_wiol(ig %p, imgs %p, count %d)\n", 
          ig, imgs, count));

  tiff_state ts;
  TIFF *tif = do_tiff_open(&ts, ig, "wm");
  
  if (!tif) {
    mm_log((1, "i_writetiff_mulit_wiol: Unable to open tif file for writing\n"));
    i_push_error(0, "Could not create TIFF object");
    return 0;
  }

  for (i = 0; i < count; ++i) {
    if (!i_writetiff_low_faxable(tif, imgs[i], fine)) {
      do_tiff_close(&ts);
      return 0;
    }

    if (!TIFFWriteDirectory(tif)) {
      i_push_error(0, "Cannot write TIFF directory");
      do_tiff_close(&ts);
      return 0;
    }
  }

  do_tiff_close(&ts);

  if (i_io_close(ig))
    return 0;

  return 1;
}

/*
=item i_writetiff_wiol(im, ig)

Stores an image in the iolayer object.

   im - image object to write out
   ig - io_object that defines source to write to 

=cut 
*/
undef_int
i_writetiff_wiol(i_img *img, io_glue *ig) {
  i_clear_error();
  mm_log((1, "i_writetiff_wiol(img %p, ig %p)\n", img, ig));

  tiff_state ts;
  TIFF *tif = do_tiff_open(&ts, ig, "wm");


  if (!tif) {
    mm_log((1, "i_writetiff_wiol: Unable to open tif file for writing\n"));
    i_push_error(0, "Could not create TIFF object");
    return 0;
  }

  if (!i_writetiff_low(tif, img)) {
    do_tiff_close(&ts);
    return 0;
  }

  do_tiff_close(&ts);

  if (i_io_close(ig))
    return 0;

  return 1;
}



/*
=item i_writetiff_wiol_faxable(i_img *, io_glue *)

Stores an image in the iolayer object in faxable tiff format.

   im - image object to write out
   ig - io_object that defines source to write to 

Note, this may be rewritten to use to simply be a call to a
lower-level function that gives more options for writing tiff at some
point.

=cut
*/

undef_int
i_writetiff_wiol_faxable(i_img *im, io_glue *ig, int fine) {
  i_clear_error();
  mm_log((1, "i_writetiff_wiol(img %p, ig %p)\n", im, ig));

  tiff_state ts;
  TIFF* tif = do_tiff_open(&ts, ig, "wm");

  if (!tif) {
    mm_log((1, "i_writetiff_wiol: Unable to open tif file for writing\n"));
    i_push_error(0, "Could not create TIFF object");
    return 0;
  }

  if (!i_writetiff_low_faxable(tif, im, fine)) {
    do_tiff_close(&ts);
    return 0;
  }

  do_tiff_close(&ts);

  if (i_io_close(ig))
    return 0;

  return 1;
}

static int save_tiff_tags(TIFF *tif, i_img *im) {
  int i;
 
  for (i = 0; i < text_tag_count; ++i) {
    int entry;
    if (i_tags_find(&im->tags, text_tag_names[i].name, 0, &entry)) {
      if (!TIFFSetField(tif, text_tag_names[i].tag, 
                       im->tags.tags[entry].data)) {
       i_push_errorf(0, "cannot save %s to TIFF", text_tag_names[i].name);
       return 0;
      }
    }
  }

  return 1;
}


static void
unpack_4bit_to(unsigned char *dest, const unsigned char *src, 
	       size_t src_byte_count) {
  while (src_byte_count > 0) {
    *dest++ = *src >> 4;
    *dest++ = *src++ & 0xf;
    --src_byte_count;
  }
}

static void pack_4bit_to(unsigned char *dest, const unsigned char *src, 
			 i_img_dim pixel_count) {
  int i = 0;
  while (i < pixel_count) {
    if ((i & 1) == 0) {
      *dest = *src++ << 4;
    }
    else {
      *dest++ |= *src++;
    }
    ++i;
  }
}

/*
=item fallback_rgb_channels

Calculate the number of output channels when we fallback to the RGBA
family of functions.

=cut
*/

static void
fallback_rgb_channels(TIFF *tif, int *channels, int *alpha_chan) {
  tf_uint16 photometric;
  tf_uint16 in_channels;
  tf_uint16 extra_count;
  tf_uint16 *extras;

  TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &in_channels);
  TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);

  switch (photometric) {
  case PHOTOMETRIC_SEPARATED:
    *channels = 3;
    break;
  
  case PHOTOMETRIC_MINISWHITE:
  case PHOTOMETRIC_MINISBLACK:
    /* the TIFF RGBA functions expand single channel grey into RGB,
       so reduce it, we move the alpha channel into the right place 
       if needed */
    *channels = 1;
    break;

  default:
    *channels = 3;
    break;
  }
  /* TIFF images can have more than one alpha channel, but Imager can't
     this ignores the possibility of 2 channel images with 2 alpha,
     but there's not much I can do about that */
  *alpha_chan = 0;
  if (TIFFGetField(tif, TIFFTAG_EXTRASAMPLES, &extra_count, &extras)
      && extra_count) {
    *alpha_chan = (*channels)++;
  }
}

static i_img *
make_rgb(TIFF *tif, i_img_dim width, i_img_dim height, int *alpha_chan) {
  int channels = 0;

  fallback_rgb_channels(tif, &channels, alpha_chan);

  return i_img_8_new(width, height, channels);
}

static i_img *
read_one_rgb_lines(TIFF *tif, i_img_dim width, i_img_dim height, int allow_incomplete) {
  i_img *im;
  tf_uint32* raster = NULL;
  tf_uint32 rowsperstrip, row;
  i_color *line_buf;
  int alpha_chan;
  int rc;

  im = make_rgb(tif, width, height, &alpha_chan);
  if (!im)
    return NULL;

  rc = TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
  mm_log((1, "i_readtiff_wiol: rowsperstrip=%u rc = %d\n",
          (unsigned)rowsperstrip, rc));
  
  if (rc != 1 || rowsperstrip == (tf_uint32)-1) {
    rowsperstrip = height;
  }
  
  raster = (tf_uint32*)_TIFFmalloc(width * rowsperstrip * sizeof(tf_uint32));
  if (!raster) {
    i_img_destroy(im);
    i_push_error(0, "No space for raster buffer");
    return NULL;
  }

  line_buf = mymalloc(sizeof(i_color) * width);
  
  for( row = 0; row < height; row += rowsperstrip ) {
    tf_uint32 newrows, i_row;
    
    if (!TIFFReadRGBAStrip(tif, row, raster)) {
      if (allow_incomplete) {
	i_tags_setn(&im->tags, "i_lines_read", row);
	i_tags_setn(&im->tags, "i_incomplete", 1);
	break;
      }
      else {
	i_push_error(0, "could not read TIFF image strip");
	_TIFFfree(raster);
	i_img_destroy(im);
	return NULL;
      }
    }
    
    newrows = (row+rowsperstrip > height) ? height-row : rowsperstrip;
    mm_log((1, "newrows=%d\n", newrows));
    
    for( i_row = 0; i_row < newrows; i_row++ ) { 
      tf_uint32 x;
      i_color *outp = line_buf;

      for(x = 0; x<width; x++) {
	tf_uint32 temp = raster[x+width*(newrows-i_row-1)];
	outp->rgba.r = TIFFGetR(temp);
	outp->rgba.g = TIFFGetG(temp);
	outp->rgba.b = TIFFGetB(temp);

	if (alpha_chan) {
	  /* the libtiff RGBA code expands greyscale into RGBA, so put the
	     alpha in the right place and scale it */
	  int ch;
	  outp->channel[alpha_chan] = TIFFGetA(temp);
	  if (outp->channel[alpha_chan]) {
	    for (ch = 0; ch < alpha_chan; ++ch) {
	      outp->channel[ch] = outp->channel[ch] * 255 / outp->channel[alpha_chan];
	    }
	  }
	}

	outp++;
      }
      i_plin(im, 0, width, i_row+row, line_buf);
    }
  }

  myfree(line_buf);
  _TIFFfree(raster);
  
  return im;
}

static i_img *
read_one_rgb_tiled(TIFF *tif, i_img_dim width, i_img_dim height, int allow_incomplete) {
  i_img *im;
  tf_uint32* raster = NULL;
  int ok = 1;
  tf_uint32 row, col;
  tf_uint32 tile_width, tile_height;
  unsigned long pixels = 0;
  i_color *line;
  int alpha_chan;
  
  im = make_rgb(tif, width, height, &alpha_chan);
  if (!im)
    return NULL;
  
  TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tile_width);
  TIFFGetField(tif, TIFFTAG_TILELENGTH, &tile_height);
  mm_log((1, "i_readtiff_wiol: tile_width=%d, tile_height=%d\n", tile_width, tile_height));
  
  raster = (tf_uint32*)_TIFFmalloc(sizeof(tf_uint32) * tile_width * tile_height);
  if (!raster) {
    i_img_destroy(im);
    i_push_error(0, "No space for raster buffer");
    return NULL;
  }
  line = mymalloc(tile_width * sizeof(i_color));
  
  for( row = 0; row < height; row += tile_height ) {
    for( col = 0; col < width; col += tile_width ) {
      
      /* Read the tile into an RGBA array */
      if (TIFFReadRGBATileExt(tif, col, row, raster, 1)) {
	tf_uint32 i_row, x;
	tf_uint32 newrows = (row+tile_height > height) ? height-row : tile_height;
	tf_uint32 newcols = (col+tile_width  > width ) ? width-col  : tile_width;

	mm_log((1, "i_readtiff_wiol: tile(%d, %d) newcols=%d newrows=%d\n", col, row, newcols, newrows));
	for( i_row = 0; i_row < newrows; i_row++ ) {
	  i_color *outp = line;
	  for(x = 0; x < newcols; x++) {
	    tf_uint32 temp = raster[x+tile_width*(tile_height-i_row-1)];
	    outp->rgba.r = TIFFGetR(temp);
	    outp->rgba.g = TIFFGetG(temp);
	    outp->rgba.b = TIFFGetB(temp);
	    outp->rgba.a = TIFFGetA(temp);

	    if (alpha_chan) {
	      /* the libtiff RGBA code expands greyscale into RGBA, so put the
		 alpha in the right place and scale it */
	      int ch;
	      outp->channel[alpha_chan] = TIFFGetA(temp);
	      
	      if (outp->channel[alpha_chan]) {
		for (ch = 0; ch < alpha_chan; ++ch) {
		  outp->channel[ch] = outp->channel[ch] * 255 / outp->channel[alpha_chan];
		}
	      }
	    }

	    ++outp;
	  }
	  i_plin(im, col, col+newcols, row+i_row, line);
	}
	pixels += (unsigned long)newrows * newcols;
      }
      else {
	if (allow_incomplete) {
	  ok = 0;
	}
	else {
	  goto error;
	}
      }
    }
  }

  if (!ok) {
    if (pixels == 0) {
      i_push_error(0, "TIFF: No image data could be read from the image");
      goto error;
    }

    /* incomplete image */
    i_tags_setn(&im->tags, "i_incomplete", 1);
    i_tags_setn(&im->tags, "i_lines_read", pixels / width);
  }

  myfree(line);
  _TIFFfree(raster);
  
  return im;

 error:
  myfree(line);
  _TIFFfree(raster);
  i_img_destroy(im);
  return NULL;
}

char const *
i_tiff_libversion(void) {
  return TIFFGetVersion();
}

#define Q(x) Q_(x)
#define Q_(x) #x

char const *
i_tiff_builddate(void) {
  return Q(TIFFLIB_VERSION);
}

char const *
i_tiff_buildversion(void) {
#ifdef TIFFLIB_MAJOR_VERSION
  return Q(TIFFLIB_MAJOR_VERSION) "." Q(TIFFLIB_MINOR_VERSION) "." Q(TIFFLIB_MICRO_VERSION);
#else
  return "";
#endif
}

static int 
setup_paletted(read_state_t *state) {
  tf_uint16 *maps[3];
  int i, ch;
  int color_count = 1 << state->bits_per_sample;

  state->img = i_img_pal_new(state->width, state->height, 3, 256);
  if (!state->img)
    return 0;

  /* setup the color map */
  if (!TIFFGetField(state->tif, TIFFTAG_COLORMAP, maps+0, maps+1, maps+2)) {
    i_push_error(0, "Cannot get colormap for paletted image");
    i_img_destroy(state->img);
    return 0;
  }
  for (i = 0; i < color_count; ++i) {
    i_color c;
    for (ch = 0; ch < 3; ++ch) {
      c.channel[ch] = Sample16To8(maps[ch][i]);
    }
    i_addcolors(state->img, &c, 1);
  }

  return 1;
}

static int 
tile_contig_getter(read_state_t *state, read_putter_t putter) {
  tf_uint32 tile_width, tile_height;
  tf_uint32 this_tile_height, this_tile_width;
  tf_uint32 rows_left, cols_left;
  tf_uint32 x, y;

  state->raster = _TIFFmalloc(TIFFTileSize(state->tif));
  if (!state->raster) {
    i_push_error(0, "tiff: Out of memory allocating tile buffer");
    return 0;
  }

  TIFFGetField(state->tif, TIFFTAG_TILEWIDTH, &tile_width);
  TIFFGetField(state->tif, TIFFTAG_TILELENGTH, &tile_height);
  rows_left = state->height;
  for (y = 0; y < state->height; y += this_tile_height) {
    this_tile_height = rows_left > tile_height ? tile_height : rows_left;

    cols_left = state->width;
    for (x = 0; x < state->width; x += this_tile_width) {
      this_tile_width = cols_left > tile_width ? tile_width : cols_left;

      if (TIFFReadTile(state->tif,
		       state->raster,
		       x, y, 0, 0) < 0) {
	if (!state->allow_incomplete) {
	  return 0;
	}
      }
      else {
	putter(state, x, y, this_tile_width, this_tile_height, tile_width - this_tile_width);
      }

      cols_left -= this_tile_width;
    }

    rows_left -= this_tile_height;
  }

  return 1;
}

static int 
strip_contig_getter(read_state_t *state, read_putter_t putter) {
  tf_uint32 rows_per_strip;
  tsize_t strip_size = TIFFStripSize(state->tif);
  tf_uint32 y, strip_rows, rows_left;

  state->raster = _TIFFmalloc(strip_size);
  if (!state->raster) {
    i_push_error(0, "tiff: Out of memory allocating strip buffer");
    return 0;
  }
  
  TIFFGetFieldDefaulted(state->tif, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
  rows_left = state->height;
  for (y = 0; y < state->height; y += strip_rows) {
    strip_rows = rows_left > rows_per_strip ? rows_per_strip : rows_left;
    if (TIFFReadEncodedStrip(state->tif,
			     TIFFComputeStrip(state->tif, y, 0),
			     state->raster,
			     strip_size) < 0) {
      if (!state->allow_incomplete)
	return 0;
    }
    else {
      putter(state, 0, y, state->width, strip_rows, 0);
    }
    rows_left -= strip_rows;
  }

  return 1;
}

static int 
paletted_putter8(read_state_t *state, i_img_dim x, i_img_dim y, i_img_dim width, i_img_dim height, int extras) {
  unsigned char *p = state->raster;

  state->pixels_read += width * height;
  while (height > 0) {
    i_ppal(state->img, x, x + width, y, p);
    p += width + extras;
    --height;
    ++y;
  }

  return 1;
}

static int 
paletted_putter4(read_state_t *state, i_img_dim x, i_img_dim y, i_img_dim width, i_img_dim height, int extras) {
  tf_uint32 img_line_size = (width + 1) / 2;
  tf_uint32 skip_line_size = (width + extras + 1) / 2;
  unsigned char *p = state->raster;

  if (!state->line_buf)
    state->line_buf = mymalloc(state->width);

  state->pixels_read += width * height;
  while (height > 0) {
    unpack_4bit_to(state->line_buf, p, img_line_size);
    i_ppal(state->img, x, x + width, y, state->line_buf);
    p += skip_line_size;
    --height;
    ++y;
  }

  return 1;
}

static void
rgb_channels(read_state_t *state, int *out_channels) {
  tf_uint16 extra_count;
  tf_uint16 *extras;
  
  /* safe defaults */
  *out_channels = 3;
  state->alpha_chan = 0;
  state->scale_alpha = 0;
  state->color_channels = 3;

  /* plain RGB */
  if (state->samples_per_pixel == 3)
    return;
 
  if (!TIFFGetField(state->tif, TIFFTAG_EXTRASAMPLES, &extra_count, &extras)) {
    mm_log((1, "tiff: samples != 3 but no extra samples tag\n"));
    return;
  }

  if (!extra_count) {
    mm_log((1, "tiff: samples != 3 but no extra samples listed"));
    return;
  }

  ++*out_channels;
  state->alpha_chan = 3;
  switch (*extras) {
  case EXTRASAMPLE_UNSPECIFIED:
  case EXTRASAMPLE_ASSOCALPHA:
    state->scale_alpha = 1;
    break;

  case EXTRASAMPLE_UNASSALPHA:
    state->scale_alpha = 0;
    break;

  default:
    mm_log((1, "tiff: unknown extra sample type %d, treating as assoc alpha\n",
	    *extras));
    state->scale_alpha = 1;
    break;
  }
  mm_log((1, "tiff alpha channel %d scale %d\n", state->alpha_chan, state->scale_alpha));
}

static void
grey_channels(read_state_t *state, int *out_channels) {
  tf_uint16 extra_count;
  tf_uint16 *extras;
  
  /* safe defaults */
  *out_channels = 1;
  state->alpha_chan = 0;
  state->scale_alpha = 0;
  state->color_channels = 1;

  /* plain grey */
  if (state->samples_per_pixel == 1)
    return;
 
  if (!TIFFGetField(state->tif, TIFFTAG_EXTRASAMPLES, &extra_count, &extras)) {
    mm_log((1, "tiff: samples != 1 but no extra samples tag\n"));
    return;
  }

  if (!extra_count) {
    mm_log((1, "tiff: samples != 1 but no extra samples listed"));
    return;
  }

  ++*out_channels;
  state->alpha_chan = 1;
  switch (*extras) {
  case EXTRASAMPLE_UNSPECIFIED:
  case EXTRASAMPLE_ASSOCALPHA:
    state->scale_alpha = 1;
    break;

  case EXTRASAMPLE_UNASSALPHA:
    state->scale_alpha = 0;
    break;

  default:
    mm_log((1, "tiff: unknown extra sample type %d, treating as assoc alpha\n",
	    *extras));
    state->scale_alpha = 1;
    break;
  }
}

static int
setup_16_rgb(read_state_t *state) {
  int out_channels;

  rgb_channels(state, &out_channels);

  state->img = i_img_16_new(state->width, state->height, out_channels);
  if (!state->img)
    return 0;
  state->line_buf = mymalloc(sizeof(unsigned) * state->width * out_channels);

  return 1;
}

static int
setup_16_grey(read_state_t *state) {
  int out_channels;

  grey_channels(state, &out_channels);

  state->img = i_img_16_new(state->width, state->height, out_channels);
  if (!state->img)
    return 0;
  state->line_buf = mymalloc(sizeof(unsigned) * state->width * out_channels);

  return 1;
}

static int 
putter_16(read_state_t *state, i_img_dim x, i_img_dim y, i_img_dim width, i_img_dim height, 
	  int row_extras) {
  tf_uint16 *p = state->raster;
  int out_chan = state->img->channels;

  state->pixels_read += width * height;
  while (height > 0) {
    i_img_dim i;
    int ch;
    unsigned *outp = state->line_buf;

    for (i = 0; i < width; ++i) {
      for (ch = 0; ch < out_chan; ++ch) {
	outp[ch] = p[ch];
      }
      if (state->sample_signed) {
	for (ch = 0; ch < state->color_channels; ++ch)
	  outp[ch] ^= 0x8000;
      }
      if (state->alpha_chan && state->scale_alpha && outp[state->alpha_chan]) {
	for (ch = 0; ch < state->alpha_chan; ++ch) {
	  int result = 0.5 + (outp[ch] * 65535.0 / outp[state->alpha_chan]);
	  outp[ch] = CLAMP16(result);
	}
      }
      p += state->samples_per_pixel;
      outp += out_chan;
    }

    i_psamp_bits(state->img, x, x + width, y, state->line_buf, NULL, out_chan, 16);

    p += row_extras * state->samples_per_pixel;
    --height;
    ++y;
  }

  return 1;
}

static int
setup_8_rgb(read_state_t *state) {
  int out_channels;

  rgb_channels(state, &out_channels);

  state->img = i_img_8_new(state->width, state->height, out_channels);
  if (!state->img)
    return 0;
  state->line_buf = mymalloc(sizeof(unsigned) * state->width * out_channels);

  return 1;
}

static int
setup_8_grey(read_state_t *state) {
  int out_channels;

  grey_channels(state, &out_channels);

  state->img = i_img_8_new(state->width, state->height, out_channels);
  if (!state->img)
    return 0;
  state->line_buf = mymalloc(sizeof(i_color) * state->width * out_channels);

  return 1;
}

static int 
putter_8(read_state_t *state, i_img_dim x, i_img_dim y, i_img_dim width, i_img_dim height, 
	  int row_extras) {
  unsigned char *p = state->raster;
  int out_chan = state->img->channels;

  state->pixels_read += width * height;
  while (height > 0) {
    i_img_dim i;
    int ch;
    i_color *outp = state->line_buf;

    for (i = 0; i < width; ++i) {
      for (ch = 0; ch < out_chan; ++ch) {
	outp->channel[ch] = p[ch];
      }
      if (state->sample_signed) {
	for (ch = 0; ch < state->color_channels; ++ch)
	  outp->channel[ch] ^= 0x80;
      }
      if (state->alpha_chan && state->scale_alpha 
	  && outp->channel[state->alpha_chan]) {
	for (ch = 0; ch < state->alpha_chan; ++ch) {
	  int result = (outp->channel[ch] * 255 + 127) / outp->channel[state->alpha_chan];
	
	  outp->channel[ch] = CLAMP8(result);
	}
      }
      p += state->samples_per_pixel;
      outp++;
    }

    i_plin(state->img, x, x + width, y, state->line_buf);

    p += row_extras * state->samples_per_pixel;
    --height;
    ++y;
  }

  return 1;
}

static int
setup_32_rgb(read_state_t *state) {
  int out_channels;

  rgb_channels(state, &out_channels);

  state->img = i_img_double_new(state->width, state->height, out_channels);
  if (!state->img)
    return 0;
  state->line_buf = mymalloc(sizeof(i_fcolor) * state->width);

  return 1;
}

static int
setup_32_grey(read_state_t *state) {
  int out_channels;

  grey_channels(state, &out_channels);

  state->img = i_img_double_new(state->width, state->height, out_channels);
  if (!state->img)
    return 0;
  state->line_buf = mymalloc(sizeof(i_fcolor) * state->width);

  return 1;
}

static int 
putter_32(read_state_t *state, i_img_dim x, i_img_dim y, i_img_dim width, i_img_dim height, 
	  int row_extras) {
  tf_uint32 *p = state->raster;
  int out_chan = state->img->channels;

  state->pixels_read += width * height;
  while (height > 0) {
    i_img_dim i;
    int ch;
    i_fcolor *outp = state->line_buf;

    for (i = 0; i < width; ++i) {
#ifdef IEEEFP_TYPES
      if (state->sample_format == SAMPLEFORMAT_IEEEFP) {
	const float *pv = (const float *)p;
	for (ch = 0; ch < out_chan; ++ch) {
	  outp->channel[ch] = pv[ch];
	}
      }
      else {
#endif
	for (ch = 0; ch < out_chan; ++ch) {
	  if (state->sample_signed && ch < state->color_channels)
	    outp->channel[ch] = (p[ch] ^ 0x80000000UL) / 4294967295.0;
	  else
	    outp->channel[ch] = p[ch] / 4294967295.0;
	}
#ifdef IEEEFP_TYPES
      }
#endif

      if (state->alpha_chan && state->scale_alpha && outp->channel[state->alpha_chan]) {
	for (ch = 0; ch < state->alpha_chan; ++ch)
	  outp->channel[ch] /= outp->channel[state->alpha_chan];
      }
      p += state->samples_per_pixel;
      outp++;
    }

    i_plinf(state->img, x, x + width, y, state->line_buf);

    p += row_extras * state->samples_per_pixel;
    --height;
    ++y;
  }

  return 1;
}

static int
setup_bilevel(read_state_t *state) {
  i_color black, white;
  state->img = i_img_pal_new(state->width, state->height, 1, 256);
  if (!state->img)
    return 0;
  black.channel[0] = black.channel[1] = black.channel[2] = 
    black.channel[3] = 0;
  white.channel[0] = white.channel[1] = white.channel[2] = 
    white.channel[3] = 255;
  if (state->photometric == PHOTOMETRIC_MINISBLACK) {
    i_addcolors(state->img, &black, 1);
    i_addcolors(state->img, &white, 1);
  }
  else {
    i_addcolors(state->img, &white, 1);
    i_addcolors(state->img, &black, 1);
  }
  state->line_buf = mymalloc(state->width);

  return 1;
}

static int 
putter_bilevel(read_state_t *state, i_img_dim x, i_img_dim y, i_img_dim width, i_img_dim height, 
	       int row_extras) {
  unsigned char *line_in = state->raster;
  size_t line_size = (width + row_extras + 7) / 8;
  
  /* tifflib returns the bits in MSB2LSB order even when the file is
     in LSB2MSB, so we only need to handle MSB2LSB */
  state->pixels_read += width * height;
  while (height > 0) {
    i_img_dim i;
    unsigned char *outp = state->line_buf;
    unsigned char *inp = line_in;
    unsigned mask = 0x80;

    for (i = 0; i < width; ++i) {
      *outp++ = *inp & mask ? 1 : 0;
      mask >>= 1;
      if (!mask) {
	++inp;
	mask = 0x80;
      }
    }

    i_ppal(state->img, x, x + width, y, state->line_buf);

    line_in += line_size;
    --height;
    ++y;
  }

  return 1;
}

static void
cmyk_channels(read_state_t *state, int *out_channels) {
  tf_uint16 extra_count;
  tf_uint16 *extras;
  
  /* safe defaults */
  *out_channels = 3;
  state->alpha_chan = 0;
  state->scale_alpha = 0;
  state->color_channels = 3;

  /* plain CMYK */
  if (state->samples_per_pixel == 4)
    return;
 
  if (!TIFFGetField(state->tif, TIFFTAG_EXTRASAMPLES, &extra_count, &extras)) {
    mm_log((1, "tiff: CMYK samples != 4 but no extra samples tag\n"));
    return;
  }

  if (!extra_count) {
    mm_log((1, "tiff: CMYK samples != 4 but no extra samples listed"));
    return;
  }

  ++*out_channels;
  state->alpha_chan = 4;
  switch (*extras) {
  case EXTRASAMPLE_UNSPECIFIED:
  case EXTRASAMPLE_ASSOCALPHA:
    state->scale_alpha = 1;
    break;

  case EXTRASAMPLE_UNASSALPHA:
    state->scale_alpha = 0;
    break;

  default:
    mm_log((1, "tiff: unknown extra sample type %d, treating as assoc alpha\n",
	    *extras));
    state->scale_alpha = 1;
    break;
  }
}

static int
setup_cmyk8(read_state_t *state) {
  int channels;

  cmyk_channels(state, &channels);
  state->img = i_img_8_new(state->width, state->height, channels);

  state->line_buf = mymalloc(sizeof(i_color) * state->width);

  return 1;
}

static int 
putter_cmyk8(read_state_t *state, i_img_dim x, i_img_dim y, i_img_dim width, i_img_dim height, 
	       int row_extras) {
  unsigned char *p = state->raster;

  state->pixels_read += width * height;
  while (height > 0) {
    i_img_dim i;
    int ch;
    i_color *outp = state->line_buf;

    for (i = 0; i < width; ++i) {
      unsigned char c, m, y, k;
      c = p[0];
      m = p[1];
      y = p[2];
      k = 255 - p[3];
      if (state->sample_signed) {
	c ^= 0x80;
	m ^= 0x80;
	y ^= 0x80;
	k ^= 0x80;
      }
      outp->rgba.r = (k * (255 - c)) / 255;
      outp->rgba.g = (k * (255 - m)) / 255;
      outp->rgba.b = (k * (255 - y)) / 255;
      if (state->alpha_chan) {
	outp->rgba.a = p[state->alpha_chan];
	if (state->scale_alpha 
	    && outp->rgba.a) {
	  for (ch = 0; ch < 3; ++ch) {
	    int result = (outp->channel[ch] * 255 + 127) / outp->rgba.a;
	    outp->channel[ch] = CLAMP8(result);
	  }
	}
      }
      p += state->samples_per_pixel;
      outp++;
    }

    i_plin(state->img, x, x + width, y, state->line_buf);

    p += row_extras * state->samples_per_pixel;
    --height;
    ++y;
  }

  return 1;
}

static int
setup_cmyk16(read_state_t *state) {
  int channels;

  cmyk_channels(state, &channels);
  state->img = i_img_16_new(state->width, state->height, channels);

  state->line_buf = mymalloc(sizeof(unsigned) * state->width * channels);

  return 1;
}

static int 
putter_cmyk16(read_state_t *state, i_img_dim x, i_img_dim y, i_img_dim width, i_img_dim height, 
	       int row_extras) {
  tf_uint16 *p = state->raster;
  int out_chan = state->img->channels;

  mm_log((4, "putter_cmyk16(%p, %" i_DF ", %" i_DF ", %" i_DF
	  ", %" i_DF ", %d)\n", state, i_DFcp(x, y), i_DFcp(width, height),
	  row_extras));

  state->pixels_read += width * height;
  while (height > 0) {
    i_img_dim i;
    int ch;
    unsigned *outp = state->line_buf;

    for (i = 0; i < width; ++i) {
      unsigned c, m, y, k;
      c = p[0];
      m = p[1];
      y = p[2];
      k = 65535 - p[3];
      if (state->sample_signed) {
	c ^= 0x8000;
	m ^= 0x8000;
	y ^= 0x8000;
	k ^= 0x8000;
      }
      outp[0] = (k * (65535U - c)) / 65535U;
      outp[1] = (k * (65535U - m)) / 65535U;
      outp[2] = (k * (65535U - y)) / 65535U;
      if (state->alpha_chan) {
	outp[3] = p[state->alpha_chan];
	if (state->scale_alpha 
	    && outp[3]) {
	  for (ch = 0; ch < 3; ++ch) {
	    int result = (outp[ch] * 65535 + 32767) / outp[3];
	    outp[3] = CLAMP16(result);
	  }
	}
      }
      p += state->samples_per_pixel;
      outp += out_chan;
    }

    i_psamp_bits(state->img, x, x + width, y, state->line_buf, NULL, out_chan, 16);

    p += row_extras * state->samples_per_pixel;
    --height;
    ++y;
  }

  return 1;
}

static void
tiffio_context_init(tiffio_context_t *c, io_glue *ig) {
  c->magic = TIFFIO_MAGIC;
  c->ig = ig;
  c->warn_buf = NULL;
}

static void
tiffio_context_final(tiffio_context_t *c) {
  c->magic = TIFFIO_MAGIC;
  if (c->warn_buf)
    io_glue_destroy(c->warn_buf);
}

i_tiff_codec *
i_tiff_get_codecs(size_t *pcount) {
  TIFFCodec *codecs = TIFFGetConfiguredCODECs();

  if (!codecs)
    return NULL;

  int count;
  for (count = 0; codecs[count].name; ++count) {
    /* just counting */
  }
  i_tiff_codec *mycodecs = mymalloc(sizeof(i_tiff_codec) * count);
  for (int i = 0; i < count; ++i) {
    mycodecs[i].description = codecs[i].name;
    mycodecs[i].name = "";
    for (int j = 0; j < compress_value_count; ++j) {
      if (codecs[i].scheme == compress_values[j].tag) {
        mycodecs[i].name = compress_values[j].name;
        break;
      }
    }
    mycodecs[i].code = codecs[i].scheme;
  }
  _TIFFfree(codecs);
  
  *pcount = count;
  return mycodecs;
}

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>, Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

Imager(3)

=cut
*/
