#include "image.h"
#include "tiffio.h"
#include "iolayer.h"


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
=item i_readtiff_wiol(ig, length)

Retrieve an image and stores in the iolayer object. Returns NULL on fatal error.

   ig     - io_glue object
   length - maximum length to read from data source, before closing it

=cut
*/

i_img*
i_readtiff_wiol(io_glue *ig, int length) {
  i_img *im;
  uint32 width, height;
  uint16 channels;
  uint32* raster;
  int tiled, error;
  TIFF* tif;
  float xres, yres;
  uint16 resunit;
  int gotXres, gotYres;

  error = 0;

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */

  io_glue_commit_types(ig);
  mm_log((1, "i_readtiff_wiol(ig %p, length %d)\n", ig, length));
  
  tif = TIFFClientOpen("Iolayer: FIXME", 
		       "rm", 
		       (thandle_t) ig,
		       (TIFFReadWriteProc) ig->readcb,
		       (TIFFReadWriteProc) ig->writecb,
		       (TIFFSeekProc) comp_seek,
		       (TIFFCloseProc) ig->closecb,
		       (TIFFSizeProc) ig->sizecb,
		       (TIFFMapFileProc) NULL,
		       (TIFFUnmapFileProc) NULL);
  
  if (!tif) {
    mm_log((1, "i_readtiff_wiol: Unable to open tif file\n"));
    return NULL;
  }

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
  tiled = TIFFIsTiled(tif);

  mm_log((1, "i_readtiff_wiol: width=%d, height=%d, channels=%d\n", width, height, channels));
  mm_log((1, "i_readtiff_wiol: %stiled\n", tiled?"":"not "));
  mm_log((1, "i_readtiff_wiol: %sbyte swapped\n", TIFFIsByteSwapped(tif)?"":"not "));
  
  im = i_img_empty_ch(NULL, width, height, channels);

  if (!TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &resunit))
    resunit = RESUNIT_INCH;
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
  
  /*   TIFFPrintDirectory(tif, stdout, 0); good for debugging */
  
  if (tiled) {
    int ok = 1;
    uint32 row, col;
    uint32 tile_width, tile_height;

    TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tile_width);
    TIFFGetField(tif, TIFFTAG_TILELENGTH, &tile_height);
    mm_log((1, "i_readtiff_wiol: tile_width=%d, tile_height=%d\n", tile_width, tile_height));

    raster = (uint32*)_TIFFmalloc(tile_width * tile_height * sizeof (uint32));
    if (!raster) {
      TIFFError(TIFFFileName(tif), "No space for raster buffer");
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
    TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
    mm_log((1, "i_readtiff_wiol: rowsperstrip=%d\n", rowsperstrip));
    
    raster = (uint32*)_TIFFmalloc(width * rowsperstrip * sizeof (uint32));
    if (!raster) {
      TIFFError(TIFFFileName(tif), "No space for raster buffer");
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
  if (error) {
    mm_log((1, "i_readtiff_wiol: error during reading\n"));
  }
  _TIFFfree( raster );
  if (TIFFLastDirectory(tif)) mm_log((1, "Last directory of tiff file\n"));
  return im;
}



/*
=item i_writetif_wiol(im, ig)

Stores an image in the iolayer object.

   im - image object to write out
   ig - io_object that defines source to write to 

=cut 
*/


/* FIXME: Add an options array in here soonish */

undef_int
i_writetiff_wiol(i_img *im, io_glue *ig) {
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
  TIFF* tif;
  int got_xres, got_yres, got_aspectonly, aspect_only, resunit;
  double xres, yres;

  char *cc = mymalloc( 123 );
  myfree(cc);


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
    break;
  default:
    /* This means a colorspace we don't handle yet */
    mm_log((1, "i_writetiff_wiol: don't handle %d channel images.\n", channels));
    return 0;
  }

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */

  io_glue_commit_types(ig);
  mm_log((1, "i_writetiff_wiol(im 0x%p, ig 0x%p)\n", im, ig));

  /* FIXME: Enable the mmap interface */
  
  tif = TIFFClientOpen("No name", 
		       "wm",
		       (thandle_t) ig, 
		       (TIFFReadWriteProc) ig->readcb,
		       (TIFFReadWriteProc) ig->writecb,
		       (TIFFSeekProc)      comp_seek,
		       (TIFFCloseProc)     ig->closecb, 
		       (TIFFSizeProc)      ig->sizecb,
		       (TIFFMapFileProc)   NULL,
		       (TIFFUnmapFileProc) NULL);
  


  if (!tif) {
    mm_log((1, "i_writetiff_wiol: Unable to open tif file for writing\n"));
    return 0;
  }

  mm_log((1, "i_writetiff_wiol: width=%d, height=%d, channels=%d\n", width, height, channels));
  
  if (!TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,      width)   ) { mm_log((1, "i_writetiff_wiol: TIFFSetField width=%d failed\n", width)); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_IMAGELENGTH,     height)  ) { mm_log((1, "i_writetiff_wiol: TIFFSetField length=%d failed\n", height)); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, channels)) { mm_log((1, "i_writetiff_wiol: TIFFSetField samplesperpixel=%d failed\n", channels)); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_ORIENTATION,  ORIENTATION_TOPLEFT)) { mm_log((1, "i_writetiff_wiol: TIFFSetField Orientation=topleft\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,   8)        ) { mm_log((1, "i_writetiff_wiol: TIFFSetField bitpersample=8\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG)) { mm_log((1, "i_writetiff_wiol: TIFFSetField planarconfig\n")); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,   photometric)) { mm_log((1, "i_writetiff_wiol: TIFFSetField photometric=%d\n", photometric)); return 0; }
  if (!TIFFSetField(tif, TIFFTAG_COMPRESSION,   compression)) { mm_log((1, "i_writetiff_wiol: TIFFSetField compression=%d\n", compression)); return 0; }

  switch (compression) {
  case COMPRESSION_JPEG:
    mm_log((1, "i_writetiff_wiol: jpeg compression\n"));
    if (!TIFFSetField(tif, TIFFTAG_JPEGQUALITY, quality)        ) { mm_log((1, "i_writetiff_wiol: TIFFSetField jpegquality=%d\n", quality)); return 0; }
    if (!TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, jpegcolormode)) { mm_log((1, "i_writetiff_wiol: TIFFSetField jpegcolormode=%d\n", jpegcolormode)); return 0; }
    break;
  case COMPRESSION_LZW:
    mm_log((1, "i_writetiff_wiol: lzw compression\n"));
    break;
  case COMPRESSION_DEFLATE:
    mm_log((1, "i_writetiff_wiol: deflate compression\n"));
    if (predictor != 0) 
      if (!TIFFSetField(tif, TIFFTAG_PREDICTOR, predictor)) { mm_log((1, "i_writetiff_wiol: TIFFSetField predictor=%d\n", predictor)); return 0; }
    break;
  case COMPRESSION_PACKBITS:
    mm_log((1, "i_writetiff_wiol: packbits compression\n"));
    break;
  default:
    mm_log((1, "i_writetiff_wiol: unknown compression %d\n", compression));
    return 0;
  }
  
  linebytes = channels * width;
  linebuf = (unsigned char *)_TIFFmalloc( TIFFScanlineSize(tif) > linebytes ?
					  linebytes : TIFFScanlineSize(tif) );
  
  if (!TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, rowsperstrip))) {
    mm_log((1, "i_writetiff_wiol: TIFFSetField rowsperstrip=%d\n", rowsperstrip)); return 0; }

  TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
  TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rc);

  mm_log((1, "i_writetiff_wiol: TIFFGetField rowsperstrip=%d\n", rowsperstrip));
  mm_log((1, "i_writetiff_wiol: TIFFGetField scanlinesize=%d\n", TIFFScanlineSize(tif) ));
  mm_log((1, "i_writetiff_wiol: TIFFGetField planarconfig=%d == %d\n", rc, PLANARCONFIG_CONTIG));

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
      TIFFClose(tif);
      i_img_destroy(im);
      i_push_error(0, "cannot set TIFFTAG_XRESOLUTION tag");
      return 0;
    }
    if (!TIFFSetField(tif, TIFFTAG_YRESOLUTION, (float)yres)) {
      TIFFClose(tif);
      i_img_destroy(im);
      i_push_error(0, "cannot set TIFFTAG_YRESOLUTION tag");
      return 0;
    }
    if (!TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, (uint16)resunit)) {
      TIFFClose(tif);
      i_img_destroy(im);
      i_push_error(0, "cannot set TIFFTAG_RESOLUTIONUNIT tag");
      return 0;
    }
  }
  
  for (y=0; y<height; y++) {
    ci = 0;
    for(x=0; x<width; x++) { 
      (void) i_gpix(im, x, y,&val);
      for(ch=0; ch<channels; ch++) linebuf[ci++] = val.channel[ch];
    }
    if (TIFFWriteScanline(tif, linebuf, y, 0) < 0) {
      mm_log((1, "i_writetiff_wiol: TIFFWriteScanline failed.\n"));
      break;
    }
  }
  (void) TIFFClose(tif);
  if (linebuf) _TIFFfree(linebuf);
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
  uint32 width, height;
  unsigned char *linebuf = NULL;
  uint32 y;
  int rc;
  uint32 x;
  TIFF* tif;
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

  io_glue_commit_types(ig);
  mm_log((1, "i_writetiff_wiol_faxable(im 0x%p, ig 0x%p)\n", im, ig));

  /* FIXME: Enable the mmap interface */
  
  tif = TIFFClientOpen("No name", 
		       "wm",
		       (thandle_t) ig, 
		       (TIFFReadWriteProc) ig->readcb,
		       (TIFFReadWriteProc) ig->writecb,
		       (TIFFSeekProc)      comp_seek,
		       (TIFFCloseProc)     ig->closecb, 
		       (TIFFSizeProc)      ig->sizecb,
		       (TIFFMapFileProc)   NULL,
		       (TIFFUnmapFileProc) NULL);

  if (!tif) {
    mm_log((1, "i_writetiff_wiol_faxable: Unable to open tif file for writing\n"));
    return 0;
  }

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
  (void) TIFFClose(tif);
  if (linebuf) _TIFFfree(linebuf);
  return 1;
}


/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>

=head1 SEE ALSO

Imager(3)

=cut
*/
