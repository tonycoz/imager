#include "imager.h"
#include "tiffio.h"
#include "iolayer.h"
#include "imageri.h"

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

#define byteswap_macro(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |     \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

struct tag_name {
  char *name;
  uint32 tag;
};

static i_img *read_one_rgb_tiled(TIFF *tif, int width, int height, int allow_incomplete);
static i_img *read_one_rgb_lines(TIFF *tif, int width, int height, int allow_incomplete);

static struct tag_name text_tag_names[] =
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

typedef struct read_state_tag read_state_t;
/* the setup function creates the image object, allocates the line buffer */
typedef int (*read_setup_t)(read_state_t *state);

/* the putter writes the image data provided by the getter to the
   image, x, y, width, height describe the target area of the image,
   extras is the extra number of pixels stored for each scanline in
   the raster buffer, (for tiles against the right side of the
   image) */

typedef int (*read_putter_t)(read_state_t *state, int x, int y, int width, 
			     int height, int extras);

/* reads from a tiled or strip image and calls the putter.
   This may need a second type for handling non-contiguous images
   at some point */
typedef int (*read_getter_t)(read_state_t *state, read_putter_t putter);

struct read_state_tag {
  TIFF *tif;
  i_img *img;
  void *raster;
  unsigned long pixels_read;
  int allow_incomplete;
  void *line_buf;
  uint32 width, height;
  uint16 bits_per_sample;

  /* the total number of channels (samples per pixel) */
  int samples_per_pixel;

  /* if non-zero, which channel is the alpha channel, typically 3 for rgb */
  int alpha_chan;

  /* whether or not to scale the color channels based on the alpha
     channel.  TIFF has 2 types of alpha channel, if the alpha channel
     we use is EXTRASAMPLE_ASSOCALPHA then the color data will need to
     be scaled to match Imager's conventions */
  int scale_alpha;

  int chan_count;
  int *chanp;
  int channels[MAXCHANNELS];
};

static int tile_contig_getter(read_state_t *state, read_putter_t putter);
static int strip_contig_getter(read_state_t *state, read_putter_t putter);

static int setup_paletted(read_state_t *state);
static int paletted_putter8(read_state_t *, int, int, int, int, int);
static int paletted_putter4(read_state_t *, int, int, int, int, int);

static int setup_16_rgb(read_state_t *state);
static int setup_16_grey(read_state_t *state);
static int putter_16(read_state_t *, int, int, int, int, int);

static const int text_tag_count = 
  sizeof(text_tag_names) / sizeof(*text_tag_names);

static void error_handler(char const *module, char const *fmt, va_list ap) {
  mm_log((1, "tiff error fmt %s\n", fmt));
  i_push_errorvf(0, fmt, ap);
}

#define WARN_BUFFER_LIMIT 10000
static char *warn_buffer = NULL;
static int warn_buffer_size = 0;

static void warn_handler(char const *module, char const *fmt, va_list ap) {
  char buf[1000];

  buf[0] = '\0';
#ifdef HAVE_SNPRINTF
  vsnprintf(buf, sizeof(buf), fmt, ap);
#else
  vsprintf(buf, fmt, ap);
#endif
  mm_log((1, "tiff warning %s\n", buf));

  if (!warn_buffer || strlen(warn_buffer)+strlen(buf)+2 > warn_buffer_size) {
    int new_size = warn_buffer_size + strlen(buf) + 2;
    char *old_buffer = warn_buffer;
    if (new_size > WARN_BUFFER_LIMIT) {
      new_size = WARN_BUFFER_LIMIT;
    }
    warn_buffer = myrealloc(warn_buffer, new_size);
    if (!old_buffer) *warn_buffer = '\0';
    warn_buffer_size = new_size;
  }
  if (strlen(warn_buffer)+strlen(buf)+2 <= warn_buffer_size) {
    strcat(warn_buffer, buf);
    strcat(warn_buffer, "\n");
  }
}

static int save_tiff_tags(TIFF *tif, i_img *im);

static void pack_4bit_hl(unsigned char *buf, int count);


static toff_t sizeproc(thandle_t x) {
	return 0;
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
  io_glue *ig = (io_glue*)h;
  return (toff_t) ig->seekcb(ig, o, w);
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
  /* do nothing */
}

static i_img *read_one_tiff(TIFF *tif, int allow_incomplete) {
  i_img *im;
  uint32 width, height;
  uint16 samples_per_pixel;
  int tiled, error;
  float xres, yres;
  uint16 resunit;
  int gotXres, gotYres;
  uint16 photometric;
  uint16 bits_per_sample;
  uint16 planar_config;
  int i;
  read_state_t state;
  read_setup_t setupf = NULL;
  read_getter_t getterf = NULL;
  read_putter_t putterf = NULL;

  error = 0;

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
  tiled = TIFFIsTiled(tif);
  TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);
  TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
  TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar_config);

  mm_log((1, "i_readtiff_wiol: width=%d, height=%d, channels=%d\n", width, height, samples_per_pixel));
  mm_log((1, "i_readtiff_wiol: %stiled\n", tiled?"":"not "));
  mm_log((1, "i_readtiff_wiol: %sbyte swapped\n", TIFFIsByteSwapped(tif)?"":"not "));

  if (photometric == PHOTOMETRIC_PALETTE && bits_per_sample <= 8) {
    setupf = setup_paletted;
    if (bits_per_sample == 8)
      putterf = paletted_putter8;
    else if (bits_per_sample == 4)
      putterf = paletted_putter4;
    else
      mm_log((1, "unsupported paletted bits_per_sample %d\n", bits_per_sample));
  }
  else if (bits_per_sample == 16 
	   && photometric == PHOTOMETRIC_RGB) {
    setupf = setup_16_rgb;
    putterf = putter_16;
  }
  else if (bits_per_sample == 16
	   && photometric == PHOTOMETRIC_MINISBLACK) {
    setupf = setup_16_grey;
    putterf = putter_16;
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
    unsigned long total_pixels = (unsigned long)width * height;
    memset(&state, 0, sizeof(state));
    state.tif = tif;
    state.allow_incomplete = allow_incomplete;
    state.width = width;
    state.height = height;
    state.bits_per_sample = bits_per_sample;
    state.samples_per_pixel = samples_per_pixel;

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
  i_tags_addn(&im->tags, "tiff_bitspersample", 0, bits_per_sample);
  i_tags_addn(&im->tags, "tiff_photometric", 0, photometric);
    
  /* resolution tags */
  TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
  gotXres = TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres);
  gotYres = TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres);
  if (gotXres || gotYres) {
    if (!gotXres)
      xres = yres;
    else if (!gotYres)
      yres = xres;
    i_tags_addn(&im->tags, "tiff_resolutionunit", 0, resunit);
    if (resunit == RESUNIT_CENTIMETER) {
      /* from dots per cm to dpi */
      xres *= 2.54;
      yres *= 2.54;
      i_tags_add(&im->tags, "tiff_resolutionunit_name", 0, "centimeter", -1, 0);
    }
    else if (resunit == RESUNIT_NONE) {
      i_tags_addn(&im->tags, "i_aspect_only", 0, 1);
      i_tags_add(&im->tags, "tiff_resolutionunit_name", 0, "none", -1, 0);
    }
    else if (resunit == RESUNIT_INCH) {
      i_tags_add(&im->tags, "tiff_resolutionunit_name", 0, "inch", -1, 0);
    }
    else {
      i_tags_add(&im->tags, "tiff_resolutionunit_name", 0, "unknown", -1, 0);
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
      i_tags_add(&im->tags, text_tag_names[i].name, 0, data, 
		 strlen(data), 0);
    }
  }

  i_tags_add(&im->tags, "i_format", 0, "tiff", -1, 0);
  if (warn_buffer && *warn_buffer) {
    i_tags_add(&im->tags, "i_warning", 0, warn_buffer, -1, 0);
    *warn_buffer = '\0';
  }

  /* general metadata */
  i_tags_addn(&im->tags, "tiff_bitspersample", 0, bits_per_sample);
  i_tags_addn(&im->tags, "tiff_photometric", 0, photometric);
    
  /* resolution tags */
  TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
  gotXres = TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres);
  gotYres = TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres);
  if (gotXres || gotYres) {
    if (!gotXres)
      xres = yres;
    else if (!gotYres)
      yres = xres;
    i_tags_addn(&im->tags, "tiff_resolutionunit", 0, resunit);
    if (resunit == RESUNIT_CENTIMETER) {
      /* from dots per cm to dpi */
      xres *= 2.54;
      yres *= 2.54;
      i_tags_add(&im->tags, "tiff_resolutionunit_name", 0, "centimeter", -1, 0);
    }
    else if (resunit == RESUNIT_NONE) {
      i_tags_addn(&im->tags, "i_aspect_only", 0, 1);
      i_tags_add(&im->tags, "tiff_resolutionunit_name", 0, "none", -1, 0);
    }
    else if (resunit == RESUNIT_INCH) {
      i_tags_add(&im->tags, "tiff_resolutionunit_name", 0, "inch", -1, 0);
    }
    else {
      i_tags_add(&im->tags, "tiff_resolutionunit_name", 0, "unknown", -1, 0);
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
      i_tags_add(&im->tags, text_tag_names[i].name, 0, data, 
		 strlen(data), 0);
    }
  }

  i_tags_add(&im->tags, "i_format", 0, "tiff", -1, 0);
  if (warn_buffer && *warn_buffer) {
    i_tags_add(&im->tags, "i_warning", 0, warn_buffer, -1, 0);
    *warn_buffer = '\0';
  }
  
  return im;
}

/*
=item i_readtiff_wiol(im, ig)

=cut
*/
i_img*
i_readtiff_wiol(io_glue *ig, int allow_incomplete, int page) {
  TIFF* tif;
  TIFFErrorHandler old_handler;
  TIFFErrorHandler old_warn_handler;
  i_img *im;

  i_clear_error();
  old_handler = TIFFSetErrorHandler(error_handler);
  old_warn_handler = TIFFSetWarningHandler(warn_handler);
  if (warn_buffer)
    *warn_buffer = '\0';

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */

  io_glue_commit_types(ig);
  mm_log((1, "i_readtiff_wiol(ig %p, allow_incomplete %d, page %d)\n", ig, allow_incomplete, page));
  
  tif = TIFFClientOpen("(Iolayer)", 
		       "rm", 
		       (thandle_t) ig,
		       (TIFFReadWriteProc) ig->readcb,
		       (TIFFReadWriteProc) ig->writecb,
		       (TIFFSeekProc) comp_seek,
		       (TIFFCloseProc) ig->closecb,
		       ig->sizecb ? (TIFFSizeProc) ig->sizecb : (TIFFSizeProc) sizeproc,
		       (TIFFMapFileProc) comp_mmap,
		       (TIFFUnmapFileProc) comp_munmap);
  
  if (!tif) {
    mm_log((1, "i_readtiff_wiol: Unable to open tif file\n"));
    i_push_error(0, "Error opening file");
    TIFFSetErrorHandler(old_handler);
    TIFFSetWarningHandler(old_warn_handler);
    return NULL;
  }

  if (page != 0) {
    if (!TIFFSetDirectory(tif, page)) {
      mm_log((1, "i_readtiff_wiol: Unable to switch to directory %d\n", page));
      i_push_errorf(0, "could not switch to page %d", page);
      TIFFSetErrorHandler(old_handler);
      TIFFSetWarningHandler(old_warn_handler);
      TIFFClose(tif);
      return NULL;
    }
  }

  im = read_one_tiff(tif, allow_incomplete);

  if (TIFFLastDirectory(tif)) mm_log((1, "Last directory of tiff file\n"));
  TIFFSetErrorHandler(old_handler);
  TIFFSetWarningHandler(old_warn_handler);
  TIFFClose(tif);
  return im;
}

/*
=item i_readtiff_multi_wiol(ig, length, *count)

Reads multiple images from a TIFF.

=cut
*/
i_img**
i_readtiff_multi_wiol(io_glue *ig, int length, int *count) {
  TIFF* tif;
  TIFFErrorHandler old_handler;
  TIFFErrorHandler old_warn_handler;
  i_img **results = NULL;
  int result_alloc = 0;
  int dirnum = 0;

  i_clear_error();
  old_handler = TIFFSetErrorHandler(error_handler);
  old_warn_handler = TIFFSetWarningHandler(warn_handler);
  if (warn_buffer)
    *warn_buffer = '\0';

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */

  io_glue_commit_types(ig);
  mm_log((1, "i_readtiff_wiol(ig %p, length %d)\n", ig, length));
  
  tif = TIFFClientOpen("(Iolayer)", 
		       "rm", 
		       (thandle_t) ig,
		       (TIFFReadWriteProc) ig->readcb,
		       (TIFFReadWriteProc) ig->writecb,
		       (TIFFSeekProc) comp_seek,
		       (TIFFCloseProc) ig->closecb,
		       ig->sizecb ? (TIFFSizeProc) ig->sizecb : (TIFFSizeProc) sizeproc,
		       (TIFFMapFileProc) comp_mmap,
		       (TIFFUnmapFileProc) comp_munmap);
  
  if (!tif) {
    mm_log((1, "i_readtiff_wiol: Unable to open tif file\n"));
    i_push_error(0, "Error opening file");
    TIFFSetErrorHandler(old_handler);
    TIFFSetWarningHandler(old_warn_handler);
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
  } while (TIFFSetDirectory(tif, ++dirnum));

  TIFFSetWarningHandler(old_warn_handler);
  TIFFSetErrorHandler(old_handler);
  TIFFClose(tif);
  return results;
}

undef_int
i_writetiff_low_faxable(TIFF *tif, i_img *im, int fine) {
  uint32 width, height;
  unsigned char *linebuf = NULL;
  uint32 y;
  int rc;
  uint32 x;
  uint32 rowsperstrip;
  float vres = fine ? 196 : 98;
  int luma_chan;

  width    = im->xsize;
  height   = im->ysize;

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
  if (!TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField samplesperpixel=1 failed\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_ORIENTATION,  ORIENTATION_TOPLEFT))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField Orientation=topleft\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,   1)        )
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField bitpersample=1\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField planarconfig\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField photometric=%d\n", PHOTOMETRIC_MINISBLACK)); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_COMPRESSION, 3))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField compression=3\n")); return 0; }

  linebuf = (unsigned char *)_TIFFmalloc( TIFFScanlineSize(tif) );
  
  if (!TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, -1))) {
    mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField rowsperstrip=-1\n")); return 0; }

  TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
  TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rc);

  mm_log((1, "i_writetiff_wiol_faxable: TIFFGetField rowsperstrip=%d\n", rowsperstrip));
  mm_log((1, "i_writetiff_wiol_faxable: TIFFGetField scanlinesize=%d\n", TIFFScanlineSize(tif) ));
  mm_log((1, "i_writetiff_wiol_faxable: TIFFGetField planarconfig=%d == %d\n", rc, PLANARCONFIG_CONTIG));

  if (!TIFFSetField(tif, TIFFTAG_XRESOLUTION, (float)204))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField Xresolution=204\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_YRESOLUTION, vres))
    { mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField Yresolution=196\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH)) {
    mm_log((1, "i_writetiff_wiol_faxable: TIFFSetField ResolutionUnit=%d\n", RESUNIT_INCH)); return 0; 
  }

  if (!save_tiff_tags(tif, im)) {
    return 0;
  }

  for (y=0; y<height; y++) {
    int linebufpos=0;
    for(x=0; x<width; x+=8) { 
      int bits;
      int bitpos;
      i_sample_t luma[8];
      uint8 bitval = 128;
      linebuf[linebufpos]=0;
      bits = width-x; if(bits>8) bits=8;
      i_gsamp(im, x, x+8, y, luma, &luma_chan, 1);
      for(bitpos=0;bitpos<bits;bitpos++) {
	linebuf[linebufpos] |= ((luma[bitpos] < 128) ? bitval : 0);
	bitval >>= 1;
      }
      linebufpos++;
    }
    if (TIFFWriteScanline(tif, linebuf, y, 0) < 0) {
      mm_log((1, "i_writetiff_wiol_faxable: TIFFWriteScanline failed.\n"));
      break;
    }
  }
  if (linebuf) _TIFFfree(linebuf);

  return 1;
}

undef_int
i_writetiff_low(TIFF *tif, i_img *im) {
  uint32 width, height;
  uint16 channels;
  uint16 predictor = 0;
  int quality = 75;
  int jpegcolormode = JPEGCOLORMODE_RGB;
  uint16 compression = COMPRESSION_PACKBITS;
  i_color val;
  uint16 photometric;
  uint32 rowsperstrip = (uint32) -1;  /* Let library pick default */
  unsigned char *linebuf = NULL;
  uint32 y;
  tsize_t linebytes;
  int ch, ci, rc;
  uint32 x;
  int got_xres, got_yres, aspect_only, resunit;
  double xres, yres;
  uint16 bitspersample = 8;
  uint16 samplesperpixel;
  uint16 *colors = NULL;

  width    = im->xsize;
  height   = im->ysize;
  channels = im->channels;

  switch (channels) {
  case 1:
    photometric = PHOTOMETRIC_MINISBLACK;
    break;
  case 3:
    photometric = PHOTOMETRIC_RGB;
    if (compression == COMPRESSION_JPEG && jpegcolormode == JPEGCOLORMODE_RGB) photometric = PHOTOMETRIC_YCBCR;
    else if (im->type == i_palette_type) {
      photometric = PHOTOMETRIC_PALETTE;
    }
    break;
  default:
    /* This means a colorspace we don't handle yet */
    mm_log((1, "i_writetiff_wiol: don't handle %d channel images.\n", channels));
    return 0;
  }

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */

  /*io_glue_commit_types(ig);*/
  /*mm_log((1, "i_writetiff_wiol(im 0x%p, ig 0x%p)\n", im, ig));*/

  mm_log((1, "i_writetiff_low: width=%d, height=%d, channels=%d\n", width, height, channels));
  
  if (!TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,      width)   ) { 
    mm_log((1, "i_writetiff_wiol: TIFFSetField width=%d failed\n", width)); 
    return 0; 
  }
  if (!TIFFSetField(tif, TIFFTAG_IMAGELENGTH,     height)  ) { 
    mm_log((1, "i_writetiff_wiol: TIFFSetField length=%d failed\n", height)); 
    return 0; 
  }
  if (!TIFFSetField(tif, TIFFTAG_ORIENTATION,  ORIENTATION_TOPLEFT)) {
    mm_log((1, "i_writetiff_wiol: TIFFSetField Orientation=topleft\n")); 
    return 0; 
  }
  if (!TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG)) { 
    mm_log((1, "i_writetiff_wiol: TIFFSetField planarconfig\n")); 
    return 0; 
  }
  if (!TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,   photometric)) { 
    mm_log((1, "i_writetiff_wiol: TIFFSetField photometric=%d\n", photometric)); 
    return 0; 
  }
  if (!TIFFSetField(tif, TIFFTAG_COMPRESSION,   compression)) { 
    mm_log((1, "i_writetiff_wiol: TIFFSetField compression=%d\n", compression)); 
    return 0; 
  }
  samplesperpixel = channels;
  if (photometric == PHOTOMETRIC_PALETTE) {
    uint16 *out[3];
    i_color c;
    int count = i_colorcount(im);
    int size;
    int ch, i;
    
    samplesperpixel = 1;
    if (count > 16)
      bitspersample = 8;
    else
      bitspersample = 4;
    size = 1 << bitspersample;
    colors = (uint16 *)_TIFFmalloc(sizeof(uint16) * 3 * size);
    out[0] = colors;
    out[1] = colors + size;
    out[2] = colors + 2 * size;
    
    for (i = 0; i < count; ++i) {
      i_getcolors(im, i, &c, 1);
      for (ch = 0; ch < 3; ++ch)
        out[ch][i] = c.channel[ch] * 257;
    }
    for (; i < size; ++i) {
      for (ch = 0; ch < 3; ++ch)
        out[ch][i] = 0;
    }
    if (!TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bitspersample)) { 
      mm_log((1, "i_writetiff_wiol: TIFFSetField bitpersample=%d\n", 
              bitspersample)); 
      return 0;
    }
    if (!TIFFSetField(tif, TIFFTAG_COLORMAP, out[0], out[1], out[2])) {
      mm_log((1, "i_writetiff_wiol: TIFFSetField colormap\n")); 
      return 0;
    }
  }
  else {
    if (!TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bitspersample)) { 
      mm_log((1, "i_writetiff_wiol: TIFFSetField bitpersample=%d\n", 
              bitspersample)); 
      return 0;
    }
  }
  if (!TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samplesperpixel)) { 
    mm_log((1, "i_writetiff_wiol: TIFFSetField samplesperpixel=%d failed\n", samplesperpixel)); 
    return 0;
  }

  switch (compression) {
  case COMPRESSION_JPEG:
    mm_log((1, "i_writetiff_wiol: jpeg compression\n"));
    if (!TIFFSetField(tif, TIFFTAG_JPEGQUALITY, quality)        ) { 
      mm_log((1, "i_writetiff_wiol: TIFFSetField jpegquality=%d\n", quality));
      return 0; 
    }
    if (!TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, jpegcolormode)) { 
      mm_log((1, "i_writetiff_wiol: TIFFSetField jpegcolormode=%d\n", jpegcolormode)); 
      return 0; 
    }
    break;
  case COMPRESSION_LZW:
    mm_log((1, "i_writetiff_wiol: lzw compression\n"));
    break;
  case COMPRESSION_DEFLATE:
    mm_log((1, "i_writetiff_wiol: deflate compression\n"));
    if (predictor != 0) 
      if (!TIFFSetField(tif, TIFFTAG_PREDICTOR, predictor)) { 
        mm_log((1, "i_writetiff_wiol: TIFFSetField predictor=%d\n", predictor)); 
        return 0; 
      }
    break;
  case COMPRESSION_PACKBITS:
    mm_log((1, "i_writetiff_wiol: packbits compression\n"));
    break;
  default:
    mm_log((1, "i_writetiff_wiol: unknown compression %d\n", compression));
    return 0;
  }
  
  linebytes = channels * width;
  linebytes = TIFFScanlineSize(tif) > linebytes ? linebytes 
    : TIFFScanlineSize(tif);
  /* working space for the scanlines - we go from 8-bit/pixel to 4 */
  if (photometric == PHOTOMETRIC_PALETTE && bitspersample == 4)
    linebytes += linebytes + 1;
  linebuf = (unsigned char *)_TIFFmalloc(linebytes);
  
  if (!TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, rowsperstrip))) {
    mm_log((1, "i_writetiff_wiol: TIFFSetField rowsperstrip=%d\n", rowsperstrip)); return 0; }

  TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
  TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rc);

  mm_log((1, "i_writetiff_wiol: TIFFGetField rowsperstrip=%d\n", rowsperstrip));
  mm_log((1, "i_writetiff_wiol: TIFFGetField scanlinesize=%d\n", TIFFScanlineSize(tif) ));
  mm_log((1, "i_writetiff_wiol: TIFFGetField planarconfig=%d == %d\n", rc, PLANARCONFIG_CONTIG));
  mm_log((1, "i_writetiff_wiol: bitspersample = %d\n", bitspersample));

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
      i_push_error(0, "cannot set TIFFTAG_XRESOLUTION tag");
      return 0;
    }
    if (!TIFFSetField(tif, TIFFTAG_YRESOLUTION, (float)yres)) {
      i_push_error(0, "cannot set TIFFTAG_YRESOLUTION tag");
      return 0;
    }
    if (!TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, (uint16)resunit)) {
      i_push_error(0, "cannot set TIFFTAG_RESOLUTIONUNIT tag");
      return 0;
    }
  }

  if (!save_tiff_tags(tif, im)) {
    return 0;
  }

  if (photometric == PHOTOMETRIC_PALETTE) {
    for (y = 0; y < height; ++y) {
      i_gpal(im, 0, width, y, linebuf);
      if (bitspersample == 4)
        pack_4bit_hl(linebuf, width);
      if (TIFFWriteScanline(tif, linebuf, y, 0) < 0) {
        mm_log((1, "i_writetiff_wiol: TIFFWriteScanline failed.\n"));
        if (linebuf) _TIFFfree(linebuf);
        if (colors) _TIFFfree(colors);
        return 0;
      }
    }
  }
  else {
    for (y=0; y<height; y++) {
      ci = 0;
      for(x=0; x<width; x++) { 
        (void) i_gpix(im, x, y,&val);
        for(ch=0; ch<channels; ch++) 
          linebuf[ci++] = val.channel[ch];
      }
      if (TIFFWriteScanline(tif, linebuf, y, 0) < 0) {
        mm_log((1, "i_writetiff_wiol: TIFFWriteScanline failed.\n"));
        if (linebuf) _TIFFfree(linebuf);
        if (colors) _TIFFfree(colors);
        return 0;
      }
    }
  }
  if (linebuf) _TIFFfree(linebuf);
  if (colors) _TIFFfree(colors);
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
  TIFF* tif;
  TIFFErrorHandler old_handler;
  int i;

  old_handler = TIFFSetErrorHandler(error_handler);

  io_glue_commit_types(ig);
  i_clear_error();
  mm_log((1, "i_writetiff_multi_wiol(ig 0x%p, imgs 0x%p, count %d)\n", 
          ig, imgs, count));

  /* FIXME: Enable the mmap interface */
  
  tif = TIFFClientOpen("No name", 
		       "wm",
		       (thandle_t) ig, 
		       (TIFFReadWriteProc) ig->readcb,
		       (TIFFReadWriteProc) ig->writecb,
		       (TIFFSeekProc)      comp_seek,
		       (TIFFCloseProc)     ig->closecb, 
		       ig->sizecb ? (TIFFSizeProc) ig->sizecb : (TIFFSizeProc) sizeproc,
		       (TIFFMapFileProc)   comp_mmap,
		       (TIFFUnmapFileProc) comp_munmap);
  


  if (!tif) {
    mm_log((1, "i_writetiff_multi_wiol: Unable to open tif file for writing\n"));
    i_push_error(0, "Could not create TIFF object");
    TIFFSetErrorHandler(old_handler);
    return 0;
  }

  for (i = 0; i < count; ++i) {
    if (!i_writetiff_low(tif, imgs[i])) {
      TIFFClose(tif);
      TIFFSetErrorHandler(old_handler);
      return 0;
    }

    if (!TIFFWriteDirectory(tif)) {
      i_push_error(0, "Cannot write TIFF directory");
      TIFFClose(tif);
      TIFFSetErrorHandler(old_handler);
      return 0;
    }
  }

  TIFFSetErrorHandler(old_handler);
  (void) TIFFClose(tif);

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
  TIFF* tif;
  int i;
  TIFFErrorHandler old_handler;

  old_handler = TIFFSetErrorHandler(error_handler);

  io_glue_commit_types(ig);
  i_clear_error();
  mm_log((1, "i_writetiff_multi_wiol(ig 0x%p, imgs 0x%p, count %d)\n", 
          ig, imgs, count));

  /* FIXME: Enable the mmap interface */
  
  tif = TIFFClientOpen("No name", 
		       "wm",
		       (thandle_t) ig, 
		       (TIFFReadWriteProc) ig->readcb,
		       (TIFFReadWriteProc) ig->writecb,
		       (TIFFSeekProc)      comp_seek,
		       (TIFFCloseProc)     ig->closecb, 
		       ig->sizecb ? (TIFFSizeProc) ig->sizecb : (TIFFSizeProc) sizeproc,
		       (TIFFMapFileProc)   comp_mmap,
		       (TIFFUnmapFileProc) comp_munmap);
  


  if (!tif) {
    mm_log((1, "i_writetiff_mulit_wiol: Unable to open tif file for writing\n"));
    i_push_error(0, "Could not create TIFF object");
    TIFFSetErrorHandler(old_handler);
    return 0;
  }

  for (i = 0; i < count; ++i) {
    if (!i_writetiff_low_faxable(tif, imgs[i], fine)) {
      TIFFClose(tif);
      TIFFSetErrorHandler(old_handler);
      return 0;
    }

    if (!TIFFWriteDirectory(tif)) {
      i_push_error(0, "Cannot write TIFF directory");
      TIFFClose(tif);
      TIFFSetErrorHandler(old_handler);
      return 0;
    }
  }

  (void) TIFFClose(tif);
  TIFFSetErrorHandler(old_handler);

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
  TIFF* tif;
  TIFFErrorHandler old_handler;

  old_handler = TIFFSetErrorHandler(error_handler);

  io_glue_commit_types(ig);
  i_clear_error();
  mm_log((1, "i_writetiff_wiol(img %p, ig 0x%p)\n", img, ig));

  /* FIXME: Enable the mmap interface */

  tif = TIFFClientOpen("No name", 
		       "wm",
		       (thandle_t) ig, 
		       (TIFFReadWriteProc) ig->readcb,
		       (TIFFReadWriteProc) ig->writecb,
		       (TIFFSeekProc)      comp_seek,
		       (TIFFCloseProc)     ig->closecb, 
		       ig->sizecb ? (TIFFSizeProc) ig->sizecb : (TIFFSizeProc) sizeproc,
		       (TIFFMapFileProc)   comp_mmap,
		       (TIFFUnmapFileProc) comp_munmap);
  


  if (!tif) {
    mm_log((1, "i_writetiff_wiol: Unable to open tif file for writing\n"));
    i_push_error(0, "Could not create TIFF object");
    TIFFSetErrorHandler(old_handler);
    return 0;
  }

  if (!i_writetiff_low(tif, img)) {
    TIFFClose(tif);
    TIFFSetErrorHandler(old_handler);
    return 0;
  }

  (void) TIFFClose(tif);
  TIFFSetErrorHandler(old_handler);

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
  TIFF* tif;
  TIFFErrorHandler old_handler;

  old_handler = TIFFSetErrorHandler(error_handler);

  io_glue_commit_types(ig);
  i_clear_error();
  mm_log((1, "i_writetiff_wiol(img %p, ig 0x%p)\n", im, ig));

  /* FIXME: Enable the mmap interface */
  
  tif = TIFFClientOpen("No name", 
		       "wm",
		       (thandle_t) ig, 
		       (TIFFReadWriteProc) ig->readcb,
		       (TIFFReadWriteProc) ig->writecb,
		       (TIFFSeekProc)      comp_seek,
		       (TIFFCloseProc)     ig->closecb, 
		       ig->sizecb ? (TIFFSizeProc) ig->sizecb : (TIFFSizeProc) sizeproc,
		       (TIFFMapFileProc)   comp_mmap,
		       (TIFFUnmapFileProc) comp_munmap);
  


  if (!tif) {
    mm_log((1, "i_writetiff_wiol: Unable to open tif file for writing\n"));
    i_push_error(0, "Could not create TIFF object");
    TIFFSetErrorHandler(old_handler);
    return 0;
  }

  if (!i_writetiff_low_faxable(tif, im, fine)) {
    TIFFClose(tif);
    TIFFSetErrorHandler(old_handler);
    return 0;
  }

  (void) TIFFClose(tif);
  TIFFSetErrorHandler(old_handler);

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
unpack_4bit_to(unsigned char *dest, unsigned char *src, int src_byte_count) {
  while (src_byte_count > 0) {
    *dest++ = *src >> 4;
    *dest++ = *src++ & 0xf;
    --src_byte_count;
  }
}

static void pack_4bit_hl(unsigned char *buf, int count) {
  int i = 0;
  while (i < count) {
    buf[i/2] = (buf[i] << 4) + buf[i+1];
    i += 2;
  }
}

static i_img *
make_rgb(TIFF *tif, int width, int height) {
  uint16 photometric;
  uint16 channels;

  TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
  TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);

  if (photometric == PHOTOMETRIC_SEPARATED && channels >= 4) {
    /* TIFF can have more than one alpha channel on an image,
       but Imager can't, only store the first one */
    
    channels = channels == 4 ? 3 : 4;

    /* unfortunately the RGBA functions don't try to deal with the alpha
       channel on CMYK images, at some point I'm planning on expanding
       TIFF support to handle 16-bit/sample images and I'll deal with
       it then */
  }

  /* TIFF images can have more than one alpha channel, but Imager can't
     this ignores the possibility of 2 channel images with 2 alpha,
     but there's not much I can do about that */
  if (channels > 4)
    channels = 4;

  return i_img_8_new(width, height, channels);
}

static i_img *
read_one_rgb_lines(TIFF *tif, int width, int height, int allow_incomplete) {
  i_img *im;
  uint32* raster = NULL;
  uint32 rowsperstrip, row;

  im = make_rgb(tif, width, height);
  if (!im)
    return NULL;

  int rc = TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
  mm_log((1, "i_readtiff_wiol: rowsperstrip=%d rc = %d\n", rowsperstrip, rc));
  
  if (rc != 1 || rowsperstrip==-1) {
    rowsperstrip = height;
  }
  
  raster = (uint32*)_TIFFmalloc(width * rowsperstrip * sizeof (uint32));
  if (!raster) {
    i_img_destroy(im);
    i_push_error(0, "No space for raster buffer");
    return NULL;
  }
  
  for( row = 0; row < height; row += rowsperstrip ) {
    uint32 newrows, i_row;
    
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
      uint32 x;
      for(x = 0; x<width; x++) {
	i_color val;
	uint32 temp = raster[x+width*(newrows-i_row-1)];
	val.rgba.r = TIFFGetR(temp);
	val.rgba.g = TIFFGetG(temp);
	val.rgba.b = TIFFGetB(temp);
	val.rgba.a = TIFFGetA(temp);
	i_ppix(im, x, i_row+row, &val);
      }
    }
  }

  _TIFFfree(raster);
  
  return im;
}

/* adapted from libtiff 

  libtiff's TIFFReadRGBATile succeeds even when asked to read an
  invalid tile, which means we have no way of knowing whether the data
  we received from it is valid or not.

  So the caller here has set stoponerror to 1 so that
  TIFFRGBAImageGet() will fail.

  read_one_rgb_tiled() then takes that into account for i_incomplete
  or failure.
 */
static int
myTIFFReadRGBATile(TIFFRGBAImage *img, uint32 col, uint32 row, uint32 * raster)

{
    int 	ok;
    uint32	tile_xsize, tile_ysize;
    uint32	read_xsize, read_ysize;
    uint32	i_row;

    /*
     * Verify that our request is legal - on a tile file, and on a
     * tile boundary.
     */
    
    TIFFGetFieldDefaulted(img->tif, TIFFTAG_TILEWIDTH, &tile_xsize);
    TIFFGetFieldDefaulted(img->tif, TIFFTAG_TILELENGTH, &tile_ysize);
    if( (col % tile_xsize) != 0 || (row % tile_ysize) != 0 )
    {
      i_push_errorf(0, "Row/col passed to myTIFFReadRGBATile() must be top"
		    "left corner of a tile.");
      return 0;
    }

    /*
     * The TIFFRGBAImageGet() function doesn't allow us to get off the
     * edge of the image, even to fill an otherwise valid tile.  So we
     * figure out how much we can read, and fix up the tile buffer to
     * a full tile configuration afterwards.
     */

    if( row + tile_ysize > img->height )
        read_ysize = img->height - row;
    else
        read_ysize = tile_ysize;
    
    if( col + tile_xsize > img->width )
        read_xsize = img->width - col;
    else
        read_xsize = tile_xsize;

    /*
     * Read the chunk of imagery.
     */
    
    img->row_offset = row;
    img->col_offset = col;

    ok = TIFFRGBAImageGet(img, raster, read_xsize, read_ysize );
        
    /*
     * If our read was incomplete we will need to fix up the tile by
     * shifting the data around as if a full tile of data is being returned.
     *
     * This is all the more complicated because the image is organized in
     * bottom to top format. 
     */

    if( read_xsize == tile_xsize && read_ysize == tile_ysize )
        return( ok );

    for( i_row = 0; i_row < read_ysize; i_row++ ) {
        memmove( raster + (tile_ysize - i_row - 1) * tile_xsize,
                 raster + (read_ysize - i_row - 1) * read_xsize,
                 read_xsize * sizeof(uint32) );
        _TIFFmemset( raster + (tile_ysize - i_row - 1) * tile_xsize+read_xsize,
                     0, sizeof(uint32) * (tile_xsize - read_xsize) );
    }

    for( i_row = read_ysize; i_row < tile_ysize; i_row++ ) {
        _TIFFmemset( raster + (tile_ysize - i_row - 1) * tile_xsize,
                     0, sizeof(uint32) * tile_xsize );
    }

    return (ok);
}

static i_img *
read_one_rgb_tiled(TIFF *tif, int width, int height, int allow_incomplete) {
  i_img *im;
  uint32* raster = NULL;
  int ok = 1;
  uint32 row, col;
  uint32 tile_width, tile_height;
  unsigned long pixels = 0;
  char 	emsg[1024] = "";
  TIFFRGBAImage img;
  i_color *line;
  
  im = make_rgb(tif, width, height);
  if (!im)
    return NULL;
  
  if (!TIFFRGBAImageOK(tif, emsg) 
      || !TIFFRGBAImageBegin(&img, tif, 1, emsg)) {
    i_push_error(0, emsg);
    i_img_destroy(im);
    return( 0 );
  }

  TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tile_width);
  TIFFGetField(tif, TIFFTAG_TILELENGTH, &tile_height);
  mm_log((1, "i_readtiff_wiol: tile_width=%d, tile_height=%d\n", tile_width, tile_height));
  
  raster = (uint32*)_TIFFmalloc(tile_width * tile_height * sizeof (uint32));
  if (!raster) {
    i_img_destroy(im);
    i_push_error(0, "No space for raster buffer");
    TIFFRGBAImageEnd(&img);
    return NULL;
  }
  line = mymalloc(tile_width * sizeof(i_color));
  
  for( row = 0; row < height; row += tile_height ) {
    for( col = 0; col < width; col += tile_width ) {
      
      /* Read the tile into an RGBA array */
      if (myTIFFReadRGBATile(&img, col, row, raster)) {
	uint32 i_row, x;
	uint32 newrows = (row+tile_height > height) ? height-row : tile_height;
	uint32 newcols = (col+tile_width  > width ) ? width-col  : tile_width;

	mm_log((1, "i_readtiff_wiol: tile(%d, %d) newcols=%d newrows=%d\n", col, row, newcols, newrows));
	for( i_row = 0; i_row < newrows; i_row++ ) {
	  i_color *outp = line;
	  for(x = 0; x < newcols; x++) {
	    uint32 temp = raster[x+tile_width*(tile_height-i_row-1)];
	    outp->rgba.r = TIFFGetR(temp);
	    outp->rgba.g = TIFFGetG(temp);
	    outp->rgba.b = TIFFGetB(temp);
	    outp->rgba.a = TIFFGetA(temp);
	    ++outp;
	  }
	  i_plin(im, col, col+newcols, row+i_row, line);
	}
	pixels += newrows * newcols;
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
  TIFFRGBAImageEnd(&img);
  _TIFFfree(raster);
  
  return im;

 error:
  myfree(line);
  _TIFFfree(raster);
  TIFFRGBAImageEnd(&img);
  i_img_destroy(im);
  return NULL;
}

char const *
i_tiff_libversion(void) {
  return TIFFGetVersion();
}

static int 
setup_paletted(read_state_t *state) {
  uint16 *maps[3];
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
  uint32 tile_width, tile_height;
  uint32 this_tile_height, this_tile_width;
  uint32 rows_left, cols_left;
  uint32 x, y;

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
  uint32 rows_per_strip;
  tsize_t strip_size = TIFFStripSize(state->tif);
  uint32 y, strip_rows, rows_left;

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
paletted_putter8(read_state_t *state, int x, int y, int width, int height, int extras) {
  unsigned char *p = state->raster;

  state->pixels_read += (unsigned long) width * height;
  while (height > 0) {
    i_ppal(state->img, x, x + width, y, p);
    p += width + extras;
    --height;
    ++y;
  }

  return 1;
}

static int 
paletted_putter4(read_state_t *state, int x, int y, int width, int height, int extras) {
  uint32 img_line_size = (width + 1) / 2;
  uint32 skip_line_size = (width + extras + 1) / 2;
  unsigned char *p = state->raster;

  if (!state->line_buf)
    state->line_buf = mymalloc(state->width);

  state->pixels_read += (unsigned long) width * height;
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
  uint16 extra_count;
  uint16 *extras;
  
  /* safe defaults */
  *out_channels = 3;
  state->alpha_chan = 0;
  state->scale_alpha = 0;

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

static void
grey_channels(read_state_t *state, int *out_channels) {
  uint16 extra_count;
  uint16 *extras;
  
  /* safe defaults */
  *out_channels = 1;
  state->alpha_chan = 0;
  state->scale_alpha = 0;

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
putter_16(read_state_t *state, int x, int y, int width, int height, 
	  int row_extras) {
  uint16 *p = state->raster;
  int out_chan = state->img->channels;

  state->pixels_read += (unsigned long) width * height;
  while (height > 0) {
    int i;
    int ch;
    unsigned *outp = state->line_buf;

    for (i = 0; i < width; ++i) {
      for (ch = 0; ch < out_chan; ++ch) {
	outp[ch] = p[ch];
      }
      if (state->alpha_chan && state->scale_alpha && outp[ch]) {
	for (ch = 0; ch < state->alpha_chan; ++ch)
	  outp[ch] = outp[ch] * (double)outp[state->alpha_chan] / 65535;
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

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>, Tony Cook <tony@imager.perl.org>

=head1 SEE ALSO

Imager(3)

=cut
*/
