#include "image.h"
#include <gif_lib.h>


#if IM_GIFMAJOR >= 4

struct gif_scalar_info {
  char *data;
  int length;
  int cpos;
};

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
    ImageNum = 0,
    BackGround = 0,
    ColorMapSize = 0,
    InterlacedOffset[] = { 0, 4, 2, 1 }, /* The way Interlaced image should. */
    InterlacedJumps[] = { 8, 8, 4, 2 };    /* be read - offsets and jumps... */
static ColorMapObject *ColorMap;

i_img *
i_readgif_low(GifFileType *GifFile, int **colour_table, int *colours) {
  i_img *im;
  int i, j, Size, Row, Col, Width, Height, ExtCode, Count, x;
  GifRecordType RecordType;
  GifByteType *Extension;
  
  GifRowType GifRow;
  static GifColorType *ColorMapEntry;
  i_color col;

  /*  unsigned char *Buffer, *BufferP; */

  mm_log((1,"i_readgif_low(GifFile %p, colour_table %p, colours %p)\n", GifFile, colour_table, colours));

  BackGround = GifFile->SBackGroundColor;
  ColorMap = (GifFile->Image.ColorMap ? GifFile->Image.ColorMap : GifFile->SColorMap);
  ColorMapSize = ColorMap->ColorCount;

  /* **************************************** */
  if(colour_table != NULL) {
    int q;
    *colour_table=mymalloc(sizeof(int *) * ColorMapSize * 3);
    if(*colour_table == NULL) 
      m_fatal(0,"Failed to allocate memory for GIF colour table, aborted."); 
    
    memset(*colour_table, 0, sizeof(int *) * ColorMapSize * 3);

    for(q=0; q<ColorMapSize; q++) {
      ColorMapEntry = &ColorMap->Colors[q];
      (*colour_table)[q*3 + 0]=ColorMapEntry->Red;
      (*colour_table)[q*3 + 1]=ColorMapEntry->Green;
      (*colour_table)[q*3 + 2]=ColorMapEntry->Blue;
    }
  }

  if(colours != NULL) {
    *colours = ColorMapSize;
  }
  
  /* **************************************** */
  im=i_img_empty_ch(NULL,GifFile->SWidth,GifFile->SHeight,3);
  
  Size = GifFile->SWidth * sizeof(GifPixelType); 
  
  if ((GifRow = (GifRowType) mymalloc(Size)) == NULL)
    m_fatal(0,"Failed to allocate memory required, aborted."); /* First row. */

  for (i = 0; i < GifFile->SWidth; i++) GifRow[i] = GifFile->SBackGroundColor;
  
  /* Scan the content of the GIF file and load the image(s) in: */
  do {
    if (DGifGetRecordType(GifFile, &RecordType) == GIF_ERROR) {
      PrintGifError();
      exit(-1);
    }
    
    switch (RecordType) {
    case IMAGE_DESC_RECORD_TYPE:
      if (DGifGetImageDesc(GifFile) == GIF_ERROR) {
	PrintGifError();
	exit(-1);
      }
      Row = GifFile->Image.Top; /* Image Position relative to Screen. */
      Col = GifFile->Image.Left;
      Width = GifFile->Image.Width;
      Height = GifFile->Image.Height;
      ImageNum++;
      mm_log((1,"i_readgif: Image %d at (%d, %d) [%dx%d]: \n",ImageNum, Col, Row, Width, Height));

      if (GifFile->Image.Left + GifFile->Image.Width > GifFile->SWidth ||
	  GifFile->Image.Top + GifFile->Image.Height > GifFile->SHeight) {
	fprintf(stderr, "Image %d is not confined to screen dimension, aborted.\n",ImageNum);
	return(0);
      }
      if (GifFile->Image.Interlace) {

	for (Count = i = 0; i < 4; i++) for (j = Row + InterlacedOffset[i]; j < Row + Height; j += InterlacedJumps[i]) {
	  Count++;
	  if (DGifGetLine(GifFile, &GifRow[Col], Width) == GIF_ERROR) {
	    mm_log((1,"fatal"));
	    exit(-1);
	  }
	  
	  for (x = 0; x < GifFile->SWidth; x++) {
	    ColorMapEntry = &ColorMap->Colors[GifRow[x]];
	    col.rgb.r = ColorMapEntry->Red;
	    col.rgb.g = ColorMapEntry->Green;
	    col.rgb.b = ColorMapEntry->Blue;
	    i_ppix(im,x,j,&col);
	  }
	  
	}
      }
      else {
	for (i = 0; i < Height; i++) {
	  if (DGifGetLine(GifFile, &GifRow[Col], Width) == GIF_ERROR) {
	    mm_log((1,"fatal\n"));
	    exit(-1);
	  }

	  for (x = 0; x < GifFile->SWidth; x++) {
	    ColorMapEntry = &ColorMap->Colors[GifRow[x]];
	    col.rgb.r = ColorMapEntry->Red;
	    col.rgb.g = ColorMapEntry->Green;
	    col.rgb.b = ColorMapEntry->Blue;
	    i_ppix(im,x,Row,&col);
	  }
	  Row++;
	}
      }
      break;
    case EXTENSION_RECORD_TYPE:
      /* Skip any extension blocks in file: */
      if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR) {
	mm_log((1,"fatal\n"));
	exit(-1);
      }
      while (Extension != NULL) {
	if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR) {
	  mm_log((1,"fatal\n"));
	  exit(-1);
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
    PrintGifError();
    exit(-1);
  }
  return im;
}


i_img *
i_readgif(int fd, int **colour_table, int *colours) {
  GifFileType *GifFile;
  
  mm_log((1,"i_readgif(fd %d, colour_table %p, colours %p)\n", fd, colour_table, colours));

  if ((GifFile = DGifOpenFileHandle(fd)) == NULL) {
    mm_log((1,"i_readgif: Unable to open file\n"));
    return NULL;
  }

  return i_readgif_low(GifFile, colour_table, colours);
}


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

#if 1

undef_int
i_writegifex(i_img *im, int fd) {
  return 0;
}

#else

/* I don't think this works
   note that RedBuffer is never allocated - TC
*/

undef_int
i_writegifex(i_img *im,int fd) {
  int colors, xsize, ysize, channels;
  int x,y,ColorMapSize;
  unsigned long Size;

  struct octt *ct;

  GifByteType *RedBuffer = NULL, *GreenBuffer = NULL, *BlueBuffer = NULL,*OutputBuffer = NULL;
  ColorMapObject *OutputColorMap = NULL;
  GifFileType *GifFile;
  GifByteType *Ptr;
  
  i_color val;

  mm_log((1,"i_writegif(0x%x,fd %d)\n",im,fd));
  
  if (!(im->channels==1 || im->channels==3)) { fprintf(stderr,"Unable to write gif, improper colorspace.\n"); exit(3); }

  xsize=im->xsize;
  ysize=im->ysize;
  channels=im->channels;

  colors=0;
  ct=octt_new();

  colors=0;
  for(x=0;x<xsize;x++) for(y=0;y<ysize;y++) {
    i_gpix(im,x,y,&val);
    colors+=octt_add(ct,val.rgb.r,val.rgb.g,val.rgb.b);
    /*  if (colors > maxc) { octt_delete(ct); } 
	We'll just bite the bullet */
  }

  ColorMapSize = (colors > 256) ? 256 : colors;
  
  Size = ((long) im->xsize) * im->ysize * sizeof(GifByteType);
  
  if ((OutputColorMap = MakeMapObject(ColorMapSize, NULL)) == NULL)
    m_fatal(0,"Failed to allocate memory for Output colormap.");
  if ((OutputBuffer = (GifByteType *) mymalloc(im->xsize * im->ysize * sizeof(GifByteType))) == NULL)
    m_fatal(0,"Failed to allocate memory for output buffer.");
  
  if (QuantizeBuffer(im->xsize, im->ysize, &ColorMapSize, RedBuffer, GreenBuffer, BlueBuffer,
		     OutputBuffer, OutputColorMap->Colors) == GIF_ERROR) {
    mm_log((1,"Error in QuantizeBuffer, unable to write image.\n"));
    return(0);
  }


  myfree(RedBuffer);
  if (im->channels == 3) { myfree(GreenBuffer); myfree(BlueBuffer); }
  
  if ((GifFile = EGifOpenFileHandle(fd)) == NULL) {
    mm_log((1,"Error in EGifOpenFileHandle, unable to write image.\n"));
    return(0);
  }
  
  if (EGifPutScreenDesc(GifFile,im->xsize, im->ysize, colors, 0,OutputColorMap) == GIF_ERROR ||
      EGifPutImageDesc(GifFile,0, 0, im->xsize, im->ysize, FALSE, NULL) == GIF_ERROR) {
    mm_log((1,"Error in EGifOpenFileHandle, unable to write image.\n"));
    if (GifFile != NULL) EGifCloseFile(GifFile);
    return(0);
  }

  Ptr = OutputBuffer;

  for (y = 0; y < im->ysize; y++) {
    if (EGifPutLine(GifFile, Ptr, im->xsize) == GIF_ERROR) {
      mm_log((1,"Error in EGifOpenFileHandle, unable to write image.\n"));
      if (GifFile != NULL) EGifCloseFile(GifFile);
      return(0);
    }
    
    Ptr += im->xsize;
  }
  
  if (EGifCloseFile(GifFile) == GIF_ERROR) {
    mm_log((1,"Error in EGifCloseFile, unable to write image.\n"));
    return(0);
  }
  return(1);
}

#endif

i_img*
i_readgif_scalar(char *data, int length, int **colour_table, int *colours) {
#if IM_GIFMAJOR >= 4
  GifFileType *GifFile;
  
  struct gif_scalar_info gsi;

  gsi.cpos=0;
  gsi.length=length;
  gsi.data=data;

  mm_log((1,"i_readgif_scalar(char* data, int length, colour_table %p, colours %p)\n", data, length, colour_table, colours));
  if ((GifFile = DGifOpen( (void*) &gsi, my_gif_inputfunc )) == NULL) {
    mm_log((1,"i_readgif_scalar: Unable to open scalar datasource.\n"));
    return NULL;
  }

  return i_readgif_low(GifFile, colour_table, colours);
#else
  return NULL;
#endif
}

#if IM_GIFMAJOR >= 4

static int
gif_read_callback(GifFileType *gft, GifByteType *buf, int length) {
  return i_gen_reader((i_gen_read_data *)gft->UserData, buf, length);
}

#endif

i_img*
i_readgif_callback(i_read_callback_t cb, char *userdata, int **colour_table, int *colours) {
#if IM_GIFMAJOR >= 4
  GifFileType *GifFile;
  i_img *result;
  
  i_gen_read_data *gci = i_gen_read_data_new(cb, userdata);

  mm_log((1,"i_readgif_callback(callback %p, userdata %p, colour_table %p, colours %p)\n", cb, userdata, colour_table, colours));
  if ((GifFile = DGifOpen( (void*) gci, gif_read_callback )) == NULL) {
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

/* low level image write 
   writes in interlace if that's what was requested */
static undef_int 
do_write(GifFileType *gf, i_gif_opts *opts, i_img *img, i_palidx *data) {
  if (opts->interlace) {
    int i, j;
    for (i = 0; i < 4; ++i) {
      for (j = InterlacedOffset[i]; j < img->ysize; j += InterlacedJumps[i]) {
	if (EGifPutLine(gf, data+j*img->xsize, img->xsize) == GIF_ERROR) {
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
	mm_log((1, "Error in EGifPutLine\n"));
	EGifCloseFile(gf);
	return 0;
      }
      data += img->xsize;
    }
  }

  return 1;
}

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
    return EGifPutExtension(gf, 0xF9, sizeof(gce), gce) != GIF_ERROR;
  }
  return 1;
}

/* add the Netscape2.0 loop extension block, if requested */
static int do_ns_loop(GifFileType *gf, i_gif_opts *opts)
{
#if 0
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
  if (opts->loop_count) {
    unsigned char nsle[15] = "NETSCAPE2.0";
    nsle[11] = 3;
    nsle[12] = 1;
    nsle[13] = opts->loop_count % 256;
    nsle[14] = opts->loop_count / 256;
    return EGifPutExtension(gf, 0xFF, sizeof(nsle), nsle) != GIF_ERROR;
  }
#endif
  return 1;
}

static ColorMapObject *make_gif_map(i_quantize *quant, i_gif_opts *opts,
				    int want_trans) {
  GifColorType colors[256];
  int i;
  int size = quant->mc_count;
  int map_size;

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
  return MakeMapObject(map_size, colors);
}

/* we need to call EGifSetGifVersion() before opening the file - put that
   common code here
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


  if (count <= 0)
    return 0; /* what are you smoking? */

  orig_count = quant->mc_count;
  orig_size = quant->mc_size;

  if (opts->each_palette) {
    int want_trans;

    /* we always generate a global palette - this lets systems with a 
       broken giflib work */
    quant_makemap(quant, imgs, 1);
    result = quant_translate(quant, imgs[0]);

    want_trans = quant->transp != tr_none 
      && imgs[0]->channels == 4 
      && quant->mc_count < 256;
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
      quant_makemap(quant, imgs+imgn, 1);
      result = quant_translate(quant, imgs[imgn]);
      want_trans = quant->transp != tr_none 
	&& imgs[imgn]->channels == 4 
	&& quant->mc_count < 256;
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

    mm_log((1, "i_writegif_low: GOT HERE\n"));

    /* handle the first image separately - since we allow giflib
       conversion and giflib doesn't give us a separate function to build
       the colormap. */
     
    /* produce a colour map */
    quant_makemap(quant, imgs, count);
    result = quant_translate(quant, imgs[0]);

    /* get a palette entry for the transparency iff we have an image
       with an alpha channel */
    want_trans = 0;
    for (imgn = 0; imgn < count; ++imgn) {
      if (imgs[imgn]->channels == 4) {
	++want_trans;
	break;
      }
    }
    want_trans = want_trans && quant->transp != tr_none && quant->mc_count < 256;
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
    mm_log((1, "Error in EGifCloseFile\n"));
    return 0;
  }

  return 1;
}

undef_int
i_writegif_gen(i_quantize *quant, int fd, i_img **imgs, int count, 
	       i_gif_opts *opts) {
  GifFileType *gf;

  mm_log((1, "i_writegif_gen(quant %p, fd %d, imgs %p, count %d, opts %p)\n", 
	  quant, fd, imgs, count, opts));

  gif_set_version(quant, opts);

  mm_log((1, "i_writegif_gen: set ops\n"));

  if ((gf = EGifOpenFileHandle(fd)) == NULL) {
    mm_log((1, "Error in EGifOpenFileHandle, unable to write image.\n"));
    return 0;
  }

  return i_writegif_low(quant, gf, imgs, count, opts);
}

#if IM_GIFMAJOR >= 4

static int gif_writer_callback(GifFileType *gf, const GifByteType *data, int size)
{
  i_gen_write_data *gwd = (i_gen_write_data *)gf->UserData;

  return i_gen_writer(gwd, data, size) ? size : 0;
}

#endif

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

  mm_log((1, "i_writegif_callback(quant %p, i_write_callback_t %p, userdata $p, maxlength %d, imgs %p, count %d, opts %p)\n", 
	  quant, cb, userdata, maxlength, imgs, count, opts));
  
  if ((gf = EGifOpen(gwd, &gif_writer_callback)) == NULL) {
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
