#include "image.h"
#include <gif_lib.h>

/*
=head1 NAME

gif.c - read and write gif files for Imager

=head1 SYNOPSIS

  i_img *img;
  i_img *imgs[count];
  int fd;
  int *colour_table,
  int colours;
  int max_colours; // number of bits per colour
  int pixdev;  // how much noise to add 
  i_color fixed[N]; // fixed palette entries 
  int fixedlen; // number of fixed colours 
  int success; // non-zero on success
  char *data; // a GIF file in memory
  int length; // how big data is 
  int reader(char *, char *, int, int);
  int writer(char *, char *, int);
  char *userdata; // user's data, whatever it is
  i_quantize quant;
  i_gif_opts opts;

  img = i_readgif(fd, &colour_table, &colours);
  success = i_writegif(img, fd, max_colours, pixdev, fixedlen, fixed);
  success = i_writegifmc(img, fd, max_colours);
  img = i_readgif_scalar(data, length, &colour_table, &colours);
  img = i_readgif_callback(cb, userdata, &colour_table, &colours);
  success = i_writegif_gen(&quant, fd, imgs, count, &opts);
  success = i_writegif_callback(&quant, writer, userdata, maxlength, 
                                imgs, count, &opts);

=head1 DESCRIPTION

This source file provides the C level interface to reading and writing
GIF files for Imager.

This has been tested with giflib 3 and 4, though you lose the callback
functionality with giflib3.

=head1 REFERENCE

=over

=cut
*/

static char const *gif_error_msg(int code);
static void gif_push_error();

#if IM_GIFMAJOR >= 4

/*
=item gif_scalar_info

Internal.  A structure passed to the reader function used for reading
GIFs from scalars.

Used with giflib 4 and later.

=cut
*/

struct gif_scalar_info {
  char *data;
  int length;
  int cpos;
};

/*
=item my_gif_inputfunc(GifFileType *gft, GifByteType *buf, int length)

Internal.  The reader callback passed to giflib.

Used with giflib 4 and later.

=cut
*/

int
my_gif_inputfunc(GifFileType* gft, GifByteType *buf,int length) {
  struct gif_scalar_info *gsi=(struct gif_scalar_info *)gft->UserData;
  /*   fprintf(stderr,"my_gif_inputfunc: length=%d cpos=%d tlength=%d\n",length,gsi->cpos,gsi->length); */

  if (gsi->cpos == gsi->length) return 0;
  if (gsi->cpos+length > gsi->length) length=gsi->length-gsi->cpos; /* Don't read too much */
  memcpy(buf,gsi->data+gsi->cpos,length);
  gsi->cpos+=length;
  return length;
}

#endif

/*
  This file needs a complete rewrite 

  This file needs a complete rewrite 

  Maybe not anymore, though reading still needs to support reading
  all those gif properties.
*/

/* Make some variables global, so we could access them faster: */

static int
  InterlacedOffset[] = { 0, 4, 2, 1 }, /* The way Interlaced image should. */
  InterlacedJumps[] = { 8, 8, 4, 2 };    /* be read - offsets and jumps... */

/*  static ColorMapObject *ColorMap; */


static
void
i_colortable_copy(int **colour_table, int *colours, ColorMapObject *colourmap) {
  GifColorType *mapentry;
  int q;
  int colourmapsize = colourmap->ColorCount;

  if(colours) *colours = colourmapsize;
  if(!colour_table) return;
  
  *colour_table = mymalloc(sizeof(int) * colourmapsize * 3);
  memset(*colour_table, 0, sizeof(int) * colourmapsize * 3);

  for(q=0; q<colourmapsize; q++) {
    mapentry = &colourmap->Colors[q];
    (*colour_table)[q*3 + 0] = mapentry->Red;
    (*colour_table)[q*3 + 1] = mapentry->Green;
    (*colour_table)[q*3 + 2] = mapentry->Blue;
  }
}


/*
=item i_readgif_low(GifFileType *GifFile, int **colour_table, int *colours)

Internal.  Low-level function for reading a GIF file.  The caller must
create the appropriate GifFileType object and pass it in.

=cut
*/

i_img *
i_readgif_low(GifFileType *GifFile, int **colour_table, int *colours) {
  i_img *im;
  int i, j, Size, Row, Col, Width, Height, ExtCode, Count, x;
  int cmapcnt = 0, ImageNum = 0, BackGround = 0, ColorMapSize = 0;
  ColorMapObject *ColorMap;
 
  GifRecordType RecordType;
  GifByteType *Extension;
  
  GifRowType GifRow;
  static GifColorType *ColorMapEntry;
  i_color col;

  mm_log((1,"i_readgif_low(GifFile %p, colour_table %p, colours %p)\n", GifFile, colour_table, colours));

  /* it's possible that the caller has called us with *colour_table being
     non-NULL, but we check that to see if we need to free an allocated
     colour table on error.
  */
  if (colour_table)
    *colour_table = NULL;

  BackGround = GifFile->SBackGroundColor;
  ColorMap = (GifFile->Image.ColorMap ? GifFile->Image.ColorMap : GifFile->SColorMap);

  if (ColorMap) {
    ColorMapSize = ColorMap->ColorCount;
    i_colortable_copy(colour_table, colours, ColorMap);
    cmapcnt++;
  }
  

  im = i_img_empty_ch(NULL,GifFile->SWidth,GifFile->SHeight,3);

  Size = GifFile->SWidth * sizeof(GifPixelType); 
  
  if ((GifRow = (GifRowType) mymalloc(Size)) == NULL)
    m_fatal(0,"Failed to allocate memory required, aborted."); /* First row. */

  for (i = 0; i < GifFile->SWidth; i++) GifRow[i] = GifFile->SBackGroundColor;
  
  /* Scan the content of the GIF file and load the image(s) in: */
  do {
    if (DGifGetRecordType(GifFile, &RecordType) == GIF_ERROR) {
      gif_push_error();
      i_push_error(0, "Unable to get record type");
      if (colour_table && *colour_table) {
	myfree(*colour_table);
	*colour_table = NULL;
      }
      i_img_destroy(im);
      DGifCloseFile(GifFile);
      return NULL;
    }
    
    switch (RecordType) {
    case IMAGE_DESC_RECORD_TYPE:
      if (DGifGetImageDesc(GifFile) == GIF_ERROR) {
	gif_push_error();
	i_push_error(0, "Unable to get image descriptor");
	if (colour_table && *colour_table) {
	  myfree(*colour_table);
	  *colour_table = NULL;
	}
	i_img_destroy(im);
	DGifCloseFile(GifFile);
	return NULL;
      }

      if (( ColorMap = (GifFile->Image.ColorMap ? GifFile->Image.ColorMap : GifFile->SColorMap) )) {
	mm_log((1, "Adding local colormap\n"));
	ColorMapSize = ColorMap->ColorCount;
	if ( cmapcnt == 0) {
	  i_colortable_copy(colour_table, colours, ColorMap);
	  cmapcnt++;
	}
      } else {
	/* No colormap and we are about to read in the image - abandon for now */
	mm_log((1, "Going in with no colormap\n"));
	i_push_error(0, "Image does not have a local or a global color map");
	/* we can't have allocated a colour table here */
	i_img_destroy(im);
	DGifCloseFile(GifFile);
	return NULL;
      }
      
      Row = GifFile->Image.Top; /* Image Position relative to Screen. */
      Col = GifFile->Image.Left;
      Width = GifFile->Image.Width;
      Height = GifFile->Image.Height;
      ImageNum++;
      mm_log((1,"i_readgif: Image %d at (%d, %d) [%dx%d]: \n",ImageNum, Col, Row, Width, Height));

      if (GifFile->Image.Left + GifFile->Image.Width > GifFile->SWidth ||
	  GifFile->Image.Top + GifFile->Image.Height > GifFile->SHeight) {
	i_push_errorf(0, "Image %d is not confined to screen dimension, aborted.\n",ImageNum);
	if (colour_table && *colour_table) {
	  myfree(*colour_table);
	  *colour_table = NULL;
	}
	i_img_destroy(im);
	DGifCloseFile(GifFile);
	return(0);
      }
      if (GifFile->Image.Interlace) {

	for (Count = i = 0; i < 4; i++) for (j = Row + InterlacedOffset[i]; j < Row + Height; j += InterlacedJumps[i]) {
	  Count++;
	  if (DGifGetLine(GifFile, GifRow, Width) == GIF_ERROR) {
	    gif_push_error();
	    i_push_error(0, "Reading GIF line");
	    if (colour_table && *colour_table) {
	      myfree(*colour_table);
	      *colour_table = NULL;
	    }
	    i_img_destroy(im);
	    DGifCloseFile(GifFile);
	    return NULL;
	  }
	  
	  for (x = 0; x < Width; x++) {
	    ColorMapEntry = &ColorMap->Colors[GifRow[x]];
	    col.rgb.r = ColorMapEntry->Red;
	    col.rgb.g = ColorMapEntry->Green;
	    col.rgb.b = ColorMapEntry->Blue;
	    i_ppix(im,Col+x,j,&col);
	  }
	  
	}
      }
      else {
	for (i = 0; i < Height; i++) {
	  if (DGifGetLine(GifFile, GifRow, Width) == GIF_ERROR) {
	    gif_push_error();
	    i_push_error(0, "Reading GIF line");
	    if (colour_table && *colour_table) {
	      myfree(*colour_table);
	      *colour_table = NULL;
	    }
	    i_img_destroy(im);
	    DGifCloseFile(GifFile);
	    return NULL;
	  }

	  for (x = 0; x < Width; x++) {
	    ColorMapEntry = &ColorMap->Colors[GifRow[x]];
	    col.rgb.r = ColorMapEntry->Red;
	    col.rgb.g = ColorMapEntry->Green;
	    col.rgb.b = ColorMapEntry->Blue;
	    i_ppix(im, Col+x, Row, &col);
	  }
	  Row++;
	}
      }
      break;
    case EXTENSION_RECORD_TYPE:
      /* Skip any extension blocks in file: */
      if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR) {
	gif_push_error();
	i_push_error(0, "Reading extension record");
	if (colour_table && *colour_table) {
	  myfree(*colour_table);
	  *colour_table = NULL;
	}
	i_img_destroy(im);
	DGifCloseFile(GifFile);
	return NULL;
      }
      while (Extension != NULL) {
	if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR) {
	  gif_push_error();
	  i_push_error(0, "reading next block of extension");
	  if (colour_table && *colour_table) {
	    myfree(*colour_table);
	    *colour_table = NULL;
	  }
	  i_img_destroy(im);
	  DGifCloseFile(GifFile);
	  return NULL;
	}
      }
      break;
    case TERMINATE_RECORD_TYPE:
      break;
    default:		    /* Should be traps by DGifGetRecordType. */
      break;
    }
  } while (RecordType != TERMINATE_RECORD_TYPE);
  
  myfree(GifRow);
  
  if (DGifCloseFile(GifFile) == GIF_ERROR) {
    gif_push_error();
    i_push_error(0, "Closing GIF file object");
    if (colour_table && *colour_table) {
      myfree(*colour_table);
      *colour_table = NULL;
    }
    i_img_destroy(im);
    return NULL;
  }
  return im;
}

/*
=item i_readgif(int fd, int **colour_table, int *colours)

Reads in a GIF file from a file handle and converts it to an Imager
RGB image object.

Returns the palette for the object in colour_table for colours
colours.

Returns NULL on failure.

=cut
*/

i_img *
i_readgif(int fd, int **colour_table, int *colours) {
  GifFileType *GifFile;

  i_clear_error();
  
  mm_log((1,"i_readgif(fd %d, colour_table %p, colours %p)\n", fd, colour_table, colours));

  if ((GifFile = DGifOpenFileHandle(fd)) == NULL) {
    gif_push_error();
    i_push_error(0, "Cannot create giflib file object");
    mm_log((1,"i_readgif: Unable to open file\n"));
    return NULL;
  }

  return i_readgif_low(GifFile, colour_table, colours);
}

/*
=item i_writegif(i_img *im, int fd, int max_colors, int pixdev, int fixedlen, i_color fixed[])

Write I<img> to the file handle I<fd>.  The resulting GIF will use a
maximum of 1<<I<max_colours> colours, with the first I<fixedlen>
colours taken from I<fixed>.

Returns non-zero on success.

=cut
*/

undef_int
i_writegif(i_img *im, int fd, int max_colors, int pixdev, int fixedlen, i_color fixed[])
{
  i_color colors[256];
  i_quantize quant;
  i_gif_opts opts;
  
  memset(&quant, 0, sizeof(quant));
  memset(&opts, 0, sizeof(opts));
  quant.make_colors = mc_addi;
  quant.mc_colors = colors;
  quant.mc_size = 1<<max_colors;
  quant.mc_count = fixedlen;
  memcpy(colors, fixed, fixedlen * sizeof(i_color));
  quant.translate = pt_perturb;
  quant.perturb = pixdev;
  return i_writegif_gen(&quant, fd, &im, 1, &opts);
}

/*
=item i_writegifmc(i_img *im, int fd, int max_colors)

Write I<img> to the file handle I<fd>.  The resulting GIF will use a
maximum of 1<<I<max_colours> colours.

Returns non-zero on success.

=cut
*/

undef_int
i_writegifmc(i_img *im, int fd, int max_colors) {
  i_color colors[256];
  i_quantize quant;
  i_gif_opts opts;
  
  memset(&quant, 0, sizeof(quant));
  memset(&opts, 0, sizeof(opts));
  quant.make_colors = mc_none; /* ignored for pt_giflib */
  quant.mc_colors = colors;
  quant.mc_size = 1 << max_colors;
  quant.mc_count = 0;
  quant.translate = pt_giflib;
  return i_writegif_gen(&quant, fd, &im, 1, &opts);
}


/*
=item i_readgif_scalar(char *data, int length, int **colour_table, int *colours)

Reads a GIF file from an in memory copy of the file.  This can be used
if you get the 'file' from some source other than an actual file (or
some other file handle).

This function is only available with giflib 4 and higher.

=cut
*/
i_img*
i_readgif_scalar(char *data, int length, int **colour_table, int *colours) {
#if IM_GIFMAJOR >= 4
  GifFileType *GifFile;
  struct gif_scalar_info gsi;

  i_clear_error();

  gsi.cpos=0;
  gsi.length=length;
  gsi.data=data;

  mm_log((1,"i_readgif_scalar(char* data, int length, colour_table %p, colours %p)\n", data, length, colour_table, colours));
  if ((GifFile = DGifOpen( (void*) &gsi, my_gif_inputfunc )) == NULL) {
    gif_push_error();
    i_push_error(0, "Cannot create giflib callback object");
    mm_log((1,"i_readgif_scalar: Unable to open scalar datasource.\n"));
    return NULL;
  }

  return i_readgif_low(GifFile, colour_table, colours);
#else
  return NULL;
#endif
}

#if IM_GIFMAJOR >= 4

/*
=item gif_read_callback(GifFileType *gft, GifByteType *buf, int length)

Internal.  The reader callback wrapper passed to giflib.

This function is only used with giflib 4 and higher.

=cut
*/

static int
gif_read_callback(GifFileType *gft, GifByteType *buf, int length) {
  return i_gen_reader((i_gen_read_data *)gft->UserData, buf, length);
}

#endif


/*
=item i_readgif_callback(i_read_callback_t cb, char *userdata, int **colour_table, int *colours)

Read a GIF file into an Imager RGB file, the data of the GIF file is
retreived by callin the user supplied callback function.

This function is only used with giflib 4 and higher.

=cut
*/

i_img*
i_readgif_callback(i_read_callback_t cb, char *userdata, int **colour_table, int *colours) {
#if IM_GIFMAJOR >= 4
  GifFileType *GifFile;
  i_img *result;

  i_gen_read_data *gci = i_gen_read_data_new(cb, userdata);

  i_clear_error();
  
  mm_log((1,"i_readgif_callback(callback %p, userdata %p, colour_table %p, colours %p)\n", cb, userdata, colour_table, colours));
  if ((GifFile = DGifOpen( (void*) gci, gif_read_callback )) == NULL) {
    gif_push_error();
    i_push_error(0, "Cannot create giflib callback object");
    mm_log((1,"i_readgif_callback: Unable to open callback datasource.\n"));
    myfree(gci);
    return NULL;
  }

  result = i_readgif_low(GifFile, colour_table, colours);
  free_gen_read_data(gci);

  return result;
#else
  return NULL;
#endif
}

/*
=item do_write(GifFileType *gf, i_gif_opts *opts, i_img *img, i_palidx *data)

Internal.  Low level image write function.  Writes in interlace if
that was requested in the GIF options.

Returns non-zero on success.

=cut
*/
static undef_int 
do_write(GifFileType *gf, i_gif_opts *opts, i_img *img, i_palidx *data) {
  if (opts->interlace) {
    int i, j;
    for (i = 0; i < 4; ++i) {
      for (j = InterlacedOffset[i]; j < img->ysize; j += InterlacedJumps[i]) {
	if (EGifPutLine(gf, data+j*img->xsize, img->xsize) == GIF_ERROR) {
	  gif_push_error();
	  i_push_error(0, "Could not save image data:");
	  mm_log((1, "Error in EGifPutLine\n"));
	  EGifCloseFile(gf);
	  return 0;
	}
      }
    }
  }
  else {
    int y;
    for (y = 0; y < img->ysize; ++y) {
      if (EGifPutLine(gf, data, img->xsize) == GIF_ERROR) {
	gif_push_error();
	i_push_error(0, "Could not save image data:");
	mm_log((1, "Error in EGifPutLine\n"));
	EGifCloseFile(gf);
	return 0;
      }
      data += img->xsize;
    }
  }

  return 1;
}

/*
=item do_gce(GifFileType *gf, int index, i_gif_opts *opts, int want_trans, int trans_index)

Internal. Writes the GIF graphics control extension, if necessary.

Returns non-zero on success.

=cut
*/
static int do_gce(GifFileType *gf, int index, i_gif_opts *opts, int want_trans, int trans_index)
{
  unsigned char gce[4] = {0};
  int want_gce = 0;
  if (want_trans) {
    gce[0] |= 1;
    gce[3] = trans_index;
    ++want_gce;
  }
  if (index < opts->delay_count) {
    gce[1] = opts->delays[index] % 256;
    gce[2] = opts->delays[index] / 256;
    ++want_gce;
  }
  if (index < opts->user_input_count) {
    if (opts->user_input_flags[index])
      gce[0] |= 2;
    ++want_gce;
  }
  if (index < opts->disposal_count) {
    gce[0] |= (opts->disposal[index] & 3) << 2;
    ++want_gce;
  }
  if (want_gce) {
    if (EGifPutExtension(gf, 0xF9, sizeof(gce), gce) == GIF_ERROR) {
      gif_push_error();
      i_push_error(0, "Could not save GCE");
    }
  }
  return 1;
}

/*
=item do_ns_loop(GifFileType *gf, i_gif_opts *opts)

Internal.  Add the Netscape2.0 loop extension block, if requested.

The code for this function is currently "#if 0"ed out since the giflib
extension writing code currently doesn't seem to support writing
application extension blocks.

=cut
*/
static int do_ns_loop(GifFileType *gf, i_gif_opts *opts)
{
  /* EGifPutExtension() doesn't appear to handle application 
     extension blocks in any way
     Since giflib wraps the fd with a FILE * (and puts that in its
     private data), we can't do an end-run and write the data 
     directly to the fd.
     There's no open interface that takes a FILE * either, so we 
     can't workaround it that way either.
     If giflib's callback interface wasn't broken by default, I'd 
     force file writes to use callbacks, but it is broken by default.
  */
#if 0
  /* yes this was another attempt at supporting the loop extension */
  if (opts->loop_count) {
    unsigned char nsle[12] = "NETSCAPE2.0";
    unsigned char subblock[3];
    if (EGifPutExtension(gf, 0xFF, 11, nsle) == GIF_ERROR) {
      gif_push_error();
      i_push_error(0, "writing loop extension");
      return 0;
    }
    subblock[0] = 1;
    subblock[1] = opts->loop_count % 256;
    subblock[2] = opts->loop_count / 256;
    if (EGifPutExtension(gf, 0, 3, subblock) == GIF_ERROR) {
      gif_push_error();
      i_push_error(0, "writing loop extention sub-block");
      return 0;
    }
    if (EGifPutExtension(gf, 0, 0, subblock) == GIF_ERROR) {
      gif_push_error();
      i_push_error(0, "writing loop extension terminator");
      return 0;
    }
  }
#endif
  return 1;
}

/*
=item make_gif_map(i_quantize *quant, i_gif_opts *opts, int want_trans)

Create a giflib color map object from an Imager color map.

=cut
*/

static ColorMapObject *make_gif_map(i_quantize *quant, i_gif_opts *opts,
				    int want_trans) {
  GifColorType colors[256];
  int i;
  int size = quant->mc_count;
  int map_size;
  ColorMapObject *map;

  for (i = 0; i < quant->mc_count; ++i) {
    colors[i].Red = quant->mc_colors[i].rgb.r;
    colors[i].Green = quant->mc_colors[i].rgb.g;
    colors[i].Blue = quant->mc_colors[i].rgb.b;
  }
  if (want_trans) {
    colors[size].Red = opts->tran_color.rgb.r;
    colors[size].Green = opts->tran_color.rgb.g;
    colors[size].Blue = opts->tran_color.rgb.b;
    ++size;
  }
  map_size = 1;
  while (map_size < size)
    map_size <<= 1;
  /* giflib spews for 1 colour maps, reasonable, I suppose */
  if (map_size == 1)
    map_size = 2;
  
  map = MakeMapObject(map_size, colors);
  mm_log((1, "XXX map is at %p and colors at %p\n", map, map->Colors));
  if (!map) {
    gif_push_error();
    i_push_error(0, "Could not create color map object");
    return NULL;
  }
  return map;
}

/*
=item gif_set_version(i_quantize *quant, i_gif_opts *opts)

We need to call EGifSetGifVersion() before opening the file - put that
common code here.

Unfortunately giflib 4.1.0 crashes when we use this.  Internally
giflib 4.1.0 has code:

  static char *GifVersionPrefix = GIF87_STAMP;

and the code that sets the version internally does:

  strncpy(&GifVersionPrefix[3], Version, 3);

which is very broken.

Failing to set the correct GIF version doesn't seem to cause a problem
with readers.

=cut
*/

static void gif_set_version(i_quantize *quant, i_gif_opts *opts) {
  /* the following crashed giflib
     the EGifSetGifVersion() is seriously borked in giflib
     it's less borked in the ungiflib beta, but we don't have a mechanism
     to distinguish them
     if (opts->delay_count
     || opts->user_input_count
     || opts->disposal_count
     || opts->loop_count
     || quant->transp != tr_none)
     EGifSetGifVersion("89a");
     else
     EGifSetGifVersion("87a");
  */
}

/*
=item i_writegif_low(i_quantize *quant, GifFileType *gf, i_img **imgs, int count, i_gif_opts *opts)

Internal.  Low-level function that does the high-level GIF processing
:)

Returns non-zero on success.

=cut
*/

static undef_int
i_writegif_low(i_quantize *quant, GifFileType *gf, i_img **imgs, int count,
	       i_gif_opts *opts) {
  unsigned char *result;
  int color_bits;
  ColorMapObject *map;
  int scrw = 0, scrh = 0;
  int imgn, orig_count, orig_size;
  int posx, posy;

  mm_log((1, "i_writegif_low(quant %p, gf  %p, imgs %p, count %d, opts %p)\n", 
	  quant, gf, imgs, count, opts));

  /**((char *)0) = 1;*/
  /* sanity is nice */
  if (quant->mc_size > 256) 
    quant->mc_size = 256;
  if (quant->mc_count > quant->mc_size)
    quant->mc_count = quant->mc_size;

  for (imgn = 0; imgn < count; ++imgn) {
    if (imgn < opts->position_count) {
      if (imgs[imgn]->xsize + opts->positions[imgn].x > scrw)
	scrw = imgs[imgn]->xsize + opts->positions[imgn].x;
      if (imgs[imgn]->ysize + opts->positions[imgn].y > scrw)
	scrh = imgs[imgn]->ysize + opts->positions[imgn].y;
    }
    else {
      if (imgs[imgn]->xsize > scrw)
	scrw = imgs[imgn]->xsize;
      if (imgs[imgn]->ysize > scrh)
	scrh = imgs[imgn]->ysize;
    }
  }

  if (count <= 0) {
    i_push_error(0, "No images provided to write");
    return 0; /* what are you smoking? */
  }

  orig_count = quant->mc_count;
  orig_size = quant->mc_size;

  if (opts->each_palette) {
    int want_trans = quant->transp != tr_none 
      && imgs[0]->channels == 4;

    /* if the caller gives us too many colours we can't do transparency */
    if (want_trans && quant->mc_count == 256)
      want_trans = 0;
    /* if they want transparency but give us a big size, make it smaller
       to give room for a transparency colour */
    if (want_trans && quant->mc_size == 256)
      --quant->mc_size;

    /* we always generate a global palette - this lets systems with a 
       broken giflib work */
    quant_makemap(quant, imgs, 1);
    result = quant_translate(quant, imgs[0]);

    if (want_trans)
      quant_transparent(quant, result, imgs[0], quant->mc_count);
    
    if ((map = make_gif_map(quant, opts, want_trans)) == NULL) {
      myfree(result);
      EGifCloseFile(gf);
      mm_log((1, "Error in MakeMapObject."));
      return 0;
    }

    color_bits = 1;
    while (quant->mc_size > (1 << color_bits))
      ++color_bits;
  
    if (EGifPutScreenDesc(gf, scrw, scrh, color_bits, 0, map) == GIF_ERROR) {
      gif_push_error();
      i_push_error(0, "Could not save screen descriptor");
      FreeMapObject(map);
      myfree(result);
      EGifCloseFile(gf);
      mm_log((1, "Error in EGifPutScreenDesc."));
      return 0;
    }
    FreeMapObject(map);

    if (!do_ns_loop(gf, opts))
      return 0;

    if (!do_gce(gf, 0, opts, want_trans, quant->mc_count)) {
      myfree(result);
      EGifCloseFile(gf);
      return 0;
    }
    if (opts->position_count) {
      posx = opts->positions[0].x;
      posy = opts->positions[0].y;
    }
    else
      posx = posy = 0;
    if (EGifPutImageDesc(gf, posx, posy, imgs[0]->xsize, imgs[0]->ysize, 
			 opts->interlace, NULL) == GIF_ERROR) {
      gif_push_error();
      i_push_error(0, "Could not save image descriptor");
      EGifCloseFile(gf);
      mm_log((1, "Error in EGifPutImageDesc."));
      return 0;
    }
    if (!do_write(gf, opts, imgs[0], result)) {
      EGifCloseFile(gf);
      myfree(result);
      return 0;
    }
    for (imgn = 1; imgn < count; ++imgn) {
      quant->mc_count = orig_count;
      quant->mc_size = orig_size;
      want_trans = quant->transp != tr_none 
	&& imgs[0]->channels == 4;
      /* if the caller gives us too many colours we can't do transparency */
      if (want_trans && quant->mc_count == 256)
	want_trans = 0;
      /* if they want transparency but give us a big size, make it smaller
	 to give room for a transparency colour */
      if (want_trans && quant->mc_size == 256)
	--quant->mc_size;

      quant_makemap(quant, imgs+imgn, 1);
      result = quant_translate(quant, imgs[imgn]);
      if (want_trans)
	quant_transparent(quant, result, imgs[imgn], quant->mc_count);
      
      if (!do_gce(gf, imgn, opts, want_trans, quant->mc_count)) {
	myfree(result);
	EGifCloseFile(gf);
	return 0;
      }
      if ((map = make_gif_map(quant, opts, want_trans)) == NULL) {
	myfree(result);
	EGifCloseFile(gf);
	mm_log((1, "Error in MakeMapObject."));
	return 0;
      }
      if (imgn < opts->position_count) {
	posx = opts->positions[imgn].x;
	posy = opts->positions[imgn].y;
      }
      else
	posx = posy = 0;
      if (EGifPutImageDesc(gf, posx, posy, imgs[imgn]->xsize, 
			   imgs[imgn]->ysize, opts->interlace, 
			   map) == GIF_ERROR) {
	gif_push_error();
	i_push_error(0, "Could not save image descriptor");
	myfree(result);
	FreeMapObject(map);
	EGifCloseFile(gf);
	mm_log((1, "Error in EGifPutImageDesc."));
	return 0;
      }
      FreeMapObject(map);
      
      if (!do_write(gf, opts, imgs[imgn], result)) {
	EGifCloseFile(gf);
	myfree(result);
	return 0;
      }
      myfree(result);
    }
  }
  else {
    int want_trans;

    /* get a palette entry for the transparency iff we have an image
       with an alpha channel */
    want_trans = 0;
    for (imgn = 0; imgn < count; ++imgn) {
      if (imgs[imgn]->channels == 4) {
	++want_trans;
	break;
      }
    }
    want_trans = want_trans && quant->transp != tr_none 
      && quant->mc_count < 256;
    if (want_trans && quant->mc_size == 256)
      --quant->mc_size;

    /* handle the first image separately - since we allow giflib
       conversion and giflib doesn't give us a separate function to build
       the colormap. */
     
    /* produce a colour map */
    quant_makemap(quant, imgs, count);
    result = quant_translate(quant, imgs[0]);

    if ((map = make_gif_map(quant, opts, want_trans)) == NULL) {
      myfree(result);
      EGifCloseFile(gf);
      mm_log((1, "Error in MakeMapObject"));
      return 0;
    }
    color_bits = 1;
    while (quant->mc_count > (1 << color_bits))
      ++color_bits;

    if (EGifPutScreenDesc(gf, scrw, scrh, color_bits, 0, map) == GIF_ERROR) {
      gif_push_error();
      i_push_error(0, "Could not save screen descriptor");
      FreeMapObject(map);
      myfree(result);
      EGifCloseFile(gf);
      mm_log((1, "Error in EGifPutScreenDesc."));
      return 0;
    }
    FreeMapObject(map);

    if (!do_ns_loop(gf, opts))
      return 0;

    if (!do_gce(gf, 0, opts, want_trans, quant->mc_count)) {
      myfree(result);
      EGifCloseFile(gf);
      return 0;
    }
    if (opts->position_count) {
      posx = opts->positions[0].x;
      posy = opts->positions[0].y;
    }
    else
      posx = posy = 0;
    if (EGifPutImageDesc(gf, posx, posy, imgs[0]->xsize, imgs[0]->ysize, 
			 opts->interlace, NULL) == GIF_ERROR) {
      gif_push_error();
      i_push_error(0, "Could not save image descriptor");
      EGifCloseFile(gf);
      mm_log((1, "Error in EGifPutImageDesc."));
      return 0;
    }
    if (want_trans && imgs[0]->channels == 4) 
      quant_transparent(quant, result, imgs[0], quant->mc_count);

    if (!do_write(gf, opts, imgs[0], result)) {
      EGifCloseFile(gf);
      myfree(result);
      return 0;
    }
    myfree(result);

    for (imgn = 1; imgn < count; ++imgn) {
      int local_trans;
      result = quant_translate(quant, imgs[imgn]);
      local_trans = want_trans && imgs[imgn]->channels == 4;
      if (local_trans)
	quant_transparent(quant, result, imgs[imgn], quant->mc_count);
      if (!do_gce(gf, imgn, opts, local_trans, quant->mc_count)) {
	myfree(result);
	EGifCloseFile(gf);
	return 0;
      }
      if (imgn < opts->position_count) {
	posx = opts->positions[imgn].x;
	posy = opts->positions[imgn].y;
      }
      else
	posx = posy = 0;
      if (EGifPutImageDesc(gf, posx, posy, 
			   imgs[imgn]->xsize, imgs[imgn]->ysize, 
			   opts->interlace, NULL) == GIF_ERROR) {
	gif_push_error();
	i_push_error(0, "Could not save image descriptor");
	myfree(result);
	EGifCloseFile(gf);
	mm_log((1, "Error in EGifPutImageDesc."));
	return 0;
      }
      if (!do_write(gf, opts, imgs[imgn], result)) {
	EGifCloseFile(gf);
	myfree(result);
	return 0;
      }
      myfree(result);
    }
  }
  if (EGifCloseFile(gf) == GIF_ERROR) {
    gif_push_error();
    i_push_error(0, "Could not close GIF file");
    mm_log((1, "Error in EGifCloseFile\n"));
    return 0;
  }

  return 1;
}

/*
=item i_writegif_gen(i_quantize *quant, int fd, i_img **imgs, int count, i_gif_opts *opts)

General high-level function to write a GIF to a file.

Writes the GIF images to the specified file handle using the options
in quant and opts.  See L<image.h/i_quantize> and
L<image.h/i_gif_opts>.

Returns non-zero on success.

=cut
*/

undef_int
i_writegif_gen(i_quantize *quant, int fd, i_img **imgs, int count, 
	       i_gif_opts *opts) {
  GifFileType *gf;

  i_clear_error();
  mm_log((1, "i_writegif_gen(quant %p, fd %d, imgs %p, count %d, opts %p)\n", 
	  quant, fd, imgs, count, opts));

  gif_set_version(quant, opts);

  if ((gf = EGifOpenFileHandle(fd)) == NULL) {
    gif_push_error();
    i_push_error(0, "Cannot create GIF file object");
    mm_log((1, "Error in EGifOpenFileHandle, unable to write image.\n"));
    return 0;
  }

  return i_writegif_low(quant, gf, imgs, count, opts);
}

#if IM_GIFMAJOR >= 4

/*
=item gif_writer_callback(GifFileType *gf, const GifByteType *data, int size)

Internal.  Wrapper for the user write callback function.

=cut
*/

static int gif_writer_callback(GifFileType *gf, const GifByteType *data, int size)
{
  i_gen_write_data *gwd = (i_gen_write_data *)gf->UserData;

  return i_gen_writer(gwd, data, size) ? size : 0;
}

#endif

/*
=item i_writegif_callback(i_quantize *quant, i_write_callback_t cb, char *userdata, int maxlength, i_img **imgs, int count, i_gif_opts *opts)

General high-level function to write a GIF using callbacks to send
back the data.

Returns non-zero on success.

=cut
*/

undef_int
i_writegif_callback(i_quantize *quant, i_write_callback_t cb, char *userdata,
		    int maxlength, i_img **imgs, int count, i_gif_opts *opts)
{
#if IM_GIFMAJOR >= 4
  GifFileType *gf;
  i_gen_write_data *gwd = i_gen_write_data_new(cb, userdata, maxlength);
  /* giflib declares this incorrectly as EgifOpen */
  extern GifFileType *EGifOpen(void *userData, OutputFunc writeFunc);
  int result;

  i_clear_error();

  mm_log((1, "i_writegif_callback(quant %p, i_write_callback_t %p, userdata $p, maxlength %d, imgs %p, count %d, opts %p)\n", 
	  quant, cb, userdata, maxlength, imgs, count, opts));
  
  if ((gf = EGifOpen(gwd, &gif_writer_callback)) == NULL) {
    gif_push_error();
    i_push_error(0, "Cannot create GIF file object");
    mm_log((1, "Error in EGifOpenFileHandle, unable to write image.\n"));
    free_gen_write_data(gwd, 0);
    return 0;
  }

  result = i_writegif_low(quant, gf, imgs, count, opts);
  return free_gen_write_data(gwd, result);
#else
  return 0;
#endif
}

/*
=item gif_error_msg(int code)

Grabs the most recent giflib error code from GifLastError() and 
returns a string that describes that error.

The returned pointer points to a static buffer, either from a literal
C string or a static buffer.

=cut */

static char const *gif_error_msg(int code) {
  static char msg[80];

  switch (code) {
  case E_GIF_ERR_OPEN_FAILED: /* should not see this */
    return "Failed to open given file";
    
  case E_GIF_ERR_WRITE_FAILED:
    return "Write failed";

  case E_GIF_ERR_HAS_SCRN_DSCR: /* should not see this */
    return "Screen descriptor already passed to giflib";

  case E_GIF_ERR_HAS_IMAG_DSCR: /* should not see this */
    return "Image descriptor already passed to giflib";
    
  case E_GIF_ERR_NO_COLOR_MAP: /* should not see this */
    return "Neither global nor local color map set";

  case E_GIF_ERR_DATA_TOO_BIG: /* should not see this */
    return "Too much pixel data passed to giflib";

  case E_GIF_ERR_NOT_ENOUGH_MEM:
    return "Out of memory";
    
  case E_GIF_ERR_DISK_IS_FULL:
    return "Disk is full";
    
  case E_GIF_ERR_CLOSE_FAILED: /* should not see this */
    return "File close failed";
 
  case E_GIF_ERR_NOT_WRITEABLE: /* should not see this */
    return "File not writable";

  case D_GIF_ERR_OPEN_FAILED:
    return "Failed to open file";
    
  case D_GIF_ERR_READ_FAILED:
    return "Failed to read from file";

  case D_GIF_ERR_NOT_GIF_FILE:
    return "File is not a GIF file";

  case D_GIF_ERR_NO_SCRN_DSCR:
    return "No screen descriptor detected - invalid file";

  case D_GIF_ERR_NO_IMAG_DSCR:
    return "No image descriptor detected - invalid file";

  case D_GIF_ERR_NO_COLOR_MAP:
    return "No global or local color map found";

  case D_GIF_ERR_WRONG_RECORD:
    return "Wrong record type detected - invalid file?";

  case D_GIF_ERR_DATA_TOO_BIG:
    return "Data in file too big for image";

  case D_GIF_ERR_NOT_ENOUGH_MEM:
    return "Out of memory";

  case D_GIF_ERR_CLOSE_FAILED:
    return "Close failed";

  case D_GIF_ERR_NOT_READABLE:
    return "File not opened for read";

  case D_GIF_ERR_IMAGE_DEFECT:
    return "Defective image";

  case D_GIF_ERR_EOF_TOO_SOON:
    return "Unexpected EOF - invalid file";

  default:
    sprintf(msg, "Unknown giflib error code %d", code);
    return msg;
  }
}

/*
=item gif_push_error()

Utility function that takes the current GIF error code, converts it to
an error message and pushes it on the error stack.

=cut
*/

static void gif_push_error() {
  int code = GifLastError(); /* clears saved error */

  i_push_error(code, gif_error_msg(code));
}

/*
=head1 BUGS

The Netscape loop extension isn't implemented.  Giflib's extension
writing code doesn't seem to support writing named extensions in this 
form.

A bug in giflib is tickled by the i_writegif_callback().  This isn't a
problem on ungiflib, but causes a SEGV on giflib.  A patch is provided
in t/t10formats.t

The GIF file tag (GIF87a vs GIF89a) currently isn't set.  Using the
supplied interface in giflib 4.1.0 causes a SEGV in
EGifSetGifVersion().  See L<gif_set_version> for an explanation.

=head1 AUTHOR

Arnar M. Hrafnkelsson, addi@umich.edu

=head1 SEE ALSO

perl(1), Imager(3)

=cut

*/
