#include "image.h"
#include "tiffio.h"
#include "iolayer.h"
#include "imagei.h"

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

static const int text_tag_count = 
  sizeof(text_tag_names) / sizeof(*text_tag_names);

static void error_handler(char const *module, char const *fmt, va_list ap) {
  i_push_errorvf(0, fmt, ap);
}

static void warn_handler(char const *module, char const *fmt, va_list ap) {
  /* for now do nothing, perhaps we could warn(), though that should be
     done in the XS code, not in the code which isn't mean to know perl 
     exists ;) */
}

static int save_tiff_tags(TIFF *tif, i_img *im);

static void expand_4bit_hl(unsigned char *buf, int count);

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

static i_img *read_one_tiff(TIFF *tif) {
  i_img *im;
  uint32 width, height;
  uint16 channels;
  uint32* raster = NULL;
  int tiled, error;
  float xres, yres;
  uint16 resunit;
  int gotXres, gotYres;
  uint16 photometric;
  uint16 bits_per_sample;
  int i;
  int ch;

  error = 0;

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
  tiled = TIFFIsTiled(tif);
  TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);
  TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);

  mm_log((1, "i_readtiff_wiol: width=%d, height=%d, channels=%d\n", width, height, channels));
  mm_log((1, "i_readtiff_wiol: %stiled\n", tiled?"":"not "));
  mm_log((1, "i_readtiff_wiol: %sbyte swapped\n", TIFFIsByteSwapped(tif)?"":"not "));

  if (photometric == PHOTOMETRIC_PALETTE && bits_per_sample <= 8) {
    channels = 3;
    im = i_img_pal_new(width, height, channels, 256);
  }
  else {
    im = i_img_empty_ch(NULL, width, height, channels);
  }

  if (!im)
    return NULL;
    
  /* resolution tags */
  TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
  gotXres = TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres);
  gotYres = TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres);
  if (gotXres || gotYres) {
    if (!gotXres)
      xres = yres;
    else if (!gotYres)
      yres = xres;
    if (resunit == RESUNIT_CENTIMETER) {
      /* from dots per cm to dpi */
      xres *= 2.54;
      yres *= 2.54;
    }
    i_tags_addn(&im->tags, "tiff_resolutionunit", 0, resunit);
    if (resunit == RESUNIT_NONE)
      i_tags_addn(&im->tags, "i_aspect_only", 0, 1);
    i_tags_set_float(&im->tags, "i_xres", 0, xres);
    i_tags_set_float(&im->tags, "i_yres", 0, yres);
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
  
  /*   TIFFPrintDirectory(tif, stdout, 0); good for debugging */

  if (photometric == PHOTOMETRIC_PALETTE &&
      (bits_per_sample == 4 || bits_per_sample == 8)) {
    uint16 *maps[3];
    char used[256];
    int maxused;
    uint32 row, col;
    unsigned char *buffer;

    if (!TIFFGetField(tif, TIFFTAG_COLORMAP, maps+0, maps+1, maps+2)) {
      i_push_error(0, "Cannot get colormap for paletted image");
      i_img_destroy(im);
      return NULL;
    }
    buffer = (unsigned char *)_TIFFmalloc(width+2);
    if (!buffer) {
      i_push_error(0, "out of memory");
      i_img_destroy(im);
      return NULL;
    }
    row = 0;
    memset(used, 0, sizeof(used));
    while (row < height && TIFFReadScanline(tif, buffer, row, 0) > 0) {
      if (bits_per_sample == 4)
        expand_4bit_hl(buffer, (width+1)/2);
      for (col = 0; col < width; ++col) {
        used[buffer[col]] = 1;
      }
      i_ppal(im, 0, width, row, buffer);
      ++row;
    }
    if (row < height) {
      error = 1;
    }
    /* Ideally we'd optimize the palette, but that could be expensive
       since we'd have to re-index every pixel.

       Optimizing the palette (even at this level) might not 
       be what the user wants, so I don't do it.

       We'll add a function to optimize a paletted image instead.
    */
    maxused = (1 << bits_per_sample)-1;
    if (!error) {
      while (maxused >= 0 && !used[maxused])
        --maxused;
    }
    for (i = 0; i < 1 << bits_per_sample; ++i) {
      i_color c;
      for (ch = 0; ch < 3; ++ch) {
        c.channel[ch] = Sample16To8(maps[ch][i]);
      }
      i_addcolors(im, &c, 1);
    }
    _TIFFfree(buffer);
  }
  else {
    if (tiled) {
      int ok = 1;
      uint32 row, col;
      uint32 tile_width, tile_height;
      
      TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tile_width);
      TIFFGetField(tif, TIFFTAG_TILELENGTH, &tile_height);
      mm_log((1, "i_readtiff_wiol: tile_width=%d, tile_height=%d\n", tile_width, tile_height));
      
      raster = (uint32*)_TIFFmalloc(tile_width * tile_height * sizeof (uint32));
      if (!raster) {
        i_img_destroy(im);
        i_push_error(0, "No space for raster buffer");
        return NULL;
      }
      
      for( row = 0; row < height; row += tile_height ) {
        for( col = 0; ok && col < width; col += tile_width ) {
          uint32 i_row, x, newrows, newcols;
          
          /* Read the tile into an RGBA array */
          if (!TIFFReadRGBATile(tif, col, row, raster)) {
            ok = 0;
            break;
          }
          newrows = (row+tile_height > height) ? height-row : tile_height;
          mm_log((1, "i_readtiff_wiol: newrows=%d\n", newrows));
          newcols = (col+tile_width  > width ) ? width-row  : tile_width;
          for( i_row = 0; i_row < tile_height; i_row++ ) {
            for(x = 0; x < newcols; x++) {
              i_color val;
              uint32 temp = raster[x+tile_width*(tile_height-i_row-1)];
              val.rgba.r = TIFFGetR(temp);
              val.rgba.g = TIFFGetG(temp);
              val.rgba.b = TIFFGetB(temp);
              val.rgba.a = TIFFGetA(temp);
              i_ppix(im, col+x, row+i_row, &val);
            }
          }
        }
      }
    } else {
      uint32 rowsperstrip, row;
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
          error++;
          break;
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
    }
  }
  if (error) {
    mm_log((1, "i_readtiff_wiol: error during reading\n"));
    i_tags_addn(&im->tags, "i_incomplete", 0, 1);
  }
  if (raster)
    _TIFFfree( raster );

  return im;
}

/*
=item i_readtiff_wiol(im, ig)

=cut
*/
i_img*
i_readtiff_wiol(io_glue *ig, int length) {
  TIFF* tif;
  TIFFErrorHandler old_handler;
  TIFFErrorHandler old_warn_handler;
  i_img *im;

  i_clear_error();
  old_handler = TIFFSetErrorHandler(error_handler);
  old_warn_handler = TIFFSetWarningHandler(warn_handler);

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
    i_push_error(0, "opening file");
    TIFFSetErrorHandler(old_handler);
    TIFFSetWarningHandler(old_warn_handler);
    return NULL;
  }

  im = read_one_tiff(tif);

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
    i_push_error(0, "opening file");
    TIFFSetErrorHandler(old_handler);
    TIFFSetWarningHandler(old_warn_handler);
    return NULL;
  }

  *count = 0;
  do {
    i_img *im = read_one_tiff(tif);
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
  int luma_mask;
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
  if (!TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK))
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
	linebuf[linebufpos] |= ((luma[bitpos]>=128)?bitval:0);
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
  int got_xres, got_yres, got_aspectonly, aspect_only, resunit;
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
    int bits;
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
  int i;

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
    return 0;
  }

  for (i = 0; i < count; ++i) {
    if (!i_writetiff_low(tif, imgs[i])) {
      TIFFClose(tif);
      return 0;
    }

    if (!TIFFWriteDirectory(tif)) {
      i_push_error(0, "Cannot write TIFF directory");
      TIFFClose(tif);
      return 0;
    }
  }

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
    return 0;
  }

  for (i = 0; i < count; ++i) {
    if (!i_writetiff_low_faxable(tif, imgs[i], fine)) {
      TIFFClose(tif);
      return 0;
    }

    if (!TIFFWriteDirectory(tif)) {
      i_push_error(0, "Cannot write TIFF directory");
      TIFFClose(tif);
      return 0;
    }
  }

  (void) TIFFClose(tif);
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
  int i;

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
    return 0;
  }

  if (!i_writetiff_low(tif, img)) {
    TIFFClose(tif);
    return 0;
  }

  (void) TIFFClose(tif);
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
  int i;

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
    return 0;
  }

  if (!i_writetiff_low_faxable(tif, im, fine)) {
    TIFFClose(tif);
    return 0;
  }

  (void) TIFFClose(tif);
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


/*
=item expand_4bit_hl(buf, count)

Expands 4-bit/entry packed data into 1 byte/entry.

buf must contain count bytes to be expanded and have 2*count bytes total 
space.

The data is expanded in place.

=cut
*/

static void expand_4bit_hl(unsigned char *buf, int count) {
  while (--count >= 0) {
    buf[count*2+1] = buf[count] & 0xF;
    buf[count*2] = buf[count] >> 4;
  }
}

static void pack_4bit_hl(unsigned char *buf, int count) {
  int i = 0;
  while (i < count) {
    buf[i/2] = (buf[i] << 4) + buf[i+1];
    i += 2;
  }
}

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>

=head1 SEE ALSO

Imager(3)

=cut
*/
