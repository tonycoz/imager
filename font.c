#include "image.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>






/*
=head1 NAME

font.c - implements font handling functions for t1 and truetype fonts

=head1 SYNOPSIS

  i_init_fonts();

  #ifdef HAVE_LIBT1
  fontnum = i_t1_new(path_to_pfb, path_to_afm);
  i_t1_bbox(fontnum, points, "foo", 3, int cords[6]);
  rc = i_t1_destroy(fontnum);
  #endif

  #ifdef HAVE_LIBTT
  handle = i_tt_new(path_to_ttf);
  rc = i_tt_bbox(handle, points, "foo", 3, int cords[6]);
  i_tt_destroy(handle);

  // and much more

=head1 DESCRIPTION

font.c implements font creation, rendering, bounding box functions and
more for Imager.

=head1 FUNCTION REFERENCE

Some of these functions are internal.

=over

=cut

*/









/* 
=item i_init_fonts()

Initialize font rendering libraries if they are avaliable.

=cut
*/

undef_int 
i_init_fonts() {
  mm_log((1,"Initializing fonts\n"));

#ifdef HAVE_LIBT1
  init_t1();
#endif
  
#ifdef HAVE_LIBTT
  init_tt();
#endif

#ifdef HAVE_FT2
  if (!i_ft2_init())
    return 0;
#endif

  return(1); /* FIXME: Always true - check the return values of the init_t1 and init_tt functions */
}




#ifdef HAVE_LIBT1



/* 
=item i_init_t1()

Initializes the t1lib font rendering engine.

=cut
*/

undef_int
init_t1() {
  mm_log((1,"init_t1()\n"));
  if ((T1_InitLib(LOGFILE|IGNORE_CONFIGFILE|IGNORE_FONTDATABASE) == NULL)){
    mm_log((1,"Initialization of t1lib failed\n"));
    return(1);
  }
  T1_SetLogLevel(T1LOG_DEBUG);
  i_t1_set_aa(1); /* Default Antialias value */
  return(0);
}


/* 
=item i_close_t1()

Shuts the t1lib font rendering engine down.

  This it seems that this function is never used.

=cut
*/

void
i_close_t1(void) {
  T1_CloseLib();
}


/*
=item i_t1_new(pfb, afm)

Loads the fonts with the given filenames, returns its font id

 pfb -  path to pfb file for font
 afm -  path to afm file for font

=cut
*/

int
i_t1_new(char *pfb,char *afm) {
  int font_id;
  mm_log((1,"i_t1_new(pfb %s,afm %s)\n",pfb,(afm?afm:"NULL")));
  font_id = T1_AddFont(pfb);
  if (font_id<0) {
    mm_log((1,"i_t1_new: Failed to load pfb file '%s' - return code %d.\n",pfb,font_id));
    return font_id;
  }
  
  if (afm != NULL) {
    mm_log((1,"i_t1_new: requesting afm file '%s'.\n",afm));
    if (T1_SetAfmFileName(font_id,afm)<0) mm_log((1,"i_t1_new: afm loading of '%s' failed.\n",afm));
  }
  return font_id;
}

/*
=item i_t1_destroy(font_id)

Frees resources for a t1 font with given font id.

   font_id - number of the font to free

=cut
*/

int
i_t1_destroy(int font_id) {
  mm_log((1,"i_t1_destroy(font_id %d)\n",font_id));
  return T1_DeleteFont(font_id);
}


/*
=item i_t1_set_aa(st)

Sets the antialiasing level of the t1 library.

   st - 0 =  NONE, 1 = LOW, 2 =  HIGH.

=cut
*/

void
i_t1_set_aa(int st) {
  int i;
  unsigned long cst[17];
  switch(st) {
  case 0:
    T1_AASetBitsPerPixel( 8 );
    T1_AASetLevel( T1_AA_NONE );
    T1_AANSetGrayValues( 0, 255 );
    mm_log((1,"setting T1 antialias to none\n"));
    break;
  case 1:
    T1_AASetBitsPerPixel( 8 );
    T1_AASetLevel( T1_AA_LOW );
    T1_AASetGrayValues( 0,65,127,191,255 );
    mm_log((1,"setting T1 antialias to low\n"));
    break;
  case 2:
    T1_AASetBitsPerPixel(8);
    T1_AASetLevel(T1_AA_HIGH);
    for(i=0;i<17;i++) cst[i]=(i*255)/16;
    T1_AAHSetGrayValues( cst );
    mm_log((1,"setting T1 antialias to high\n"));
  }
}


/* 
=item i_t1_cp(im, xb, yb, channel, fontnum, points, str, len, align)

Interface to text rendering into a single channel in an image

   im        pointer to image structure
   xb        x coordinate of start of string
   yb        y coordinate of start of string ( see align )
   channel - destination channel
   fontnum - t1 library font id
   points  - number of points in fontheight
   str     - string to render
   len     - string length
   align   - (0 - top of font glyph | 1 - baseline )

=cut
*/

undef_int
i_t1_cp(i_img *im,int xb,int yb,int channel,int fontnum,float points,char* str,int len,int align) {
  GLYPH *glyph;
  int xsize,ysize,x,y;
  i_color val;

  unsigned int ch_mask_store;
  
  if (im == NULL) { mm_log((1,"i_t1_cp: Null image in input\n")); return(0); }

  glyph=T1_AASetString( fontnum, str, len, 0, T1_KERNING, points, NULL);
  if (glyph == NULL)
    return 0;

  mm_log((1,"metrics: ascent: %d descent: %d\n",glyph->metrics.ascent,glyph->metrics.descent));
  mm_log((1," leftSideBearing: %d rightSideBearing: %d\n",glyph->metrics.leftSideBearing,glyph->metrics.rightSideBearing));
  mm_log((1," advanceX: %d  advanceY: %d\n",glyph->metrics.advanceX,glyph->metrics.advanceY));
  mm_log((1,"bpp: %d\n",glyph->bpp));
  
  xsize=glyph->metrics.rightSideBearing-glyph->metrics.leftSideBearing;
  ysize=glyph->metrics.ascent-glyph->metrics.descent;
  
  mm_log((1,"width: %d height: %d\n",xsize,ysize));

  ch_mask_store=im->ch_mask;
  im->ch_mask=1<<channel;

  if (align==1) { xb+=glyph->metrics.leftSideBearing; yb-=glyph->metrics.ascent; }
  
  for(y=0;y<ysize;y++) for(x=0;x<xsize;x++) {
    val.channel[channel]=glyph->bits[y*xsize+x];
    i_ppix(im,x+xb,y+yb,&val);
  }
  
  im->ch_mask=ch_mask_store;
  return 1;
}


/*
=item i_t1_bbox(handle, fontnum, points, str, len, cords)

function to get a strings bounding box given the font id and sizes

   handle  - pointer to font handle   
   fontnum - t1 library font id
   points  - number of points in fontheight
   str     - string to measure
   len     - string length
   cords   - the bounding box (modified in place)

=cut
*/

void
i_t1_bbox(int fontnum,float points,char *str,int len,int cords[6]) {
  BBox bbox;
  BBox gbbox;
  
  mm_log((1,"i_t1_bbox(fontnum %d,points %.2f,str '%.*s', len %d)\n",fontnum,points,len,str,len));
  T1_LoadFont(fontnum);  /* FIXME: Here a return code is ignored - haw haw haw */ 
  bbox = T1_GetStringBBox(fontnum,str,len,0,T1_KERNING);
  gbbox = T1_GetFontBBox(fontnum);
  
  mm_log((1,"bbox: (%d,%d,%d,%d)\n",
	  (int)(bbox.llx*points/1000),
	  (int)(gbbox.lly*points/1000),
	  (int)(bbox.urx*points/1000),
	  (int)(gbbox.ury*points/1000),
	  (int)(bbox.lly*points/1000),
	  (int)(bbox.ury*points/1000) ));


  cords[0]=((float)bbox.llx*points)/1000;
  cords[2]=((float)bbox.urx*points)/1000;

  cords[1]=((float)gbbox.lly*points)/1000;
  cords[3]=((float)gbbox.ury*points)/1000;

  cords[4]=((float)bbox.lly*points)/1000;
  cords[5]=((float)bbox.ury*points)/1000;
}


/*
=item i_t1_text(im, xb, yb, cl, fontnum, points, str, len, align)

Interface to text rendering in a single color onto an image

   im      - pointer to image structure
   xb      - x coordinate of start of string
   yb      - y coordinate of start of string ( see align )
   cl      - color to draw the text in
   fontnum - t1 library font id
   points  - number of points in fontheight
   str     - char pointer to string to render
   len     - string length
   align   - (0 - top of font glyph | 1 - baseline )

=cut
*/

undef_int
i_t1_text(i_img *im,int xb,int yb,i_color *cl,int fontnum,float points,char* str,int len,int align) {
  GLYPH *glyph;
  int xsize,ysize,x,y,ch;
  i_color val;
  unsigned char c,i;

  if (im == NULL) { mm_log((1,"i_t1_cp: Null image in input\n")); return(0); }

  glyph=T1_AASetString( fontnum, str, len, 0, T1_KERNING, points, NULL);
  if (glyph == NULL)
    return 0;

  mm_log((1,"metrics:  ascent: %d descent: %d\n",glyph->metrics.ascent,glyph->metrics.descent));
  mm_log((1," leftSideBearing: %d rightSideBearing: %d\n",glyph->metrics.leftSideBearing,glyph->metrics.rightSideBearing));
  mm_log((1," advanceX: %d advanceY: %d\n",glyph->metrics.advanceX,glyph->metrics.advanceY));
  mm_log((1,"bpp: %d\n",glyph->bpp));
  
  xsize=glyph->metrics.rightSideBearing-glyph->metrics.leftSideBearing;
  ysize=glyph->metrics.ascent-glyph->metrics.descent;
  
  mm_log((1,"width: %d height: %d\n",xsize,ysize));

  if (align==1) { xb+=glyph->metrics.leftSideBearing; yb-=glyph->metrics.ascent; }
  
  for(y=0;y<ysize;y++) for(x=0;x<xsize;x++) {
    c=glyph->bits[y*xsize+x];
    i=255-c;
    i_gpix(im,x+xb,y+yb,&val);
    for(ch=0;ch<im->channels;ch++) val.channel[ch]=(c*cl->channel[ch]+i*val.channel[ch])/255;
    i_ppix(im,x+xb,y+yb,&val);
  }
  return 1;
}


#endif /* HAVE_LIBT1 */










/* Truetype font support */

#ifdef HAVE_LIBTT


/* Defines */

#define USTRCT(x) ((x).z)
#define TT_VALID( handle )  ( ( handle ).z != NULL )


/* Prototypes */

static  int i_tt_get_instance( TT_Fonthandle *handle, int points, int smooth );
static void i_tt_init_raster_map( TT_Raster_Map* bit, int width, int height, int smooth );
static void i_tt_done_raster_map( TT_Raster_Map *bit );
static void i_tt_clear_raster_map( TT_Raster_Map* bit );
static void i_tt_blit_or( TT_Raster_Map *dst, TT_Raster_Map *src,int x_off, int y_off );
static  int i_tt_get_glyph( TT_Fonthandle *handle, int inst, unsigned char j );
static void i_tt_render_glyph( TT_Glyph glyph, TT_Glyph_Metrics* gmetrics, TT_Raster_Map *bit, TT_Raster_Map *small_bit, int x_off, int y_off, int smooth );
static void i_tt_render_all_glyphs( TT_Fonthandle *handle, int inst, TT_Raster_Map *bit, TT_Raster_Map *small_bit, int cords[6], char* txt, int len, int smooth );
static void i_tt_dump_raster_map2( i_img* im, TT_Raster_Map* bit, int xb, int yb, i_color *cl, int smooth );
static void i_tt_dump_raster_map_channel( i_img* im, TT_Raster_Map* bit, int xb, int yb, int channel, int smooth );
static  int i_tt_rasterize( TT_Fonthandle *handle, TT_Raster_Map *bit, int cords[6], float points, char* txt, int len, int smooth );
static undef_int i_tt_bbox_inst( TT_Fonthandle *handle, int inst ,const char *txt, int len, int cords[6] );


/* static globals needed */

static TT_Engine    engine;
static int  LTT_dpi    = 72; /* FIXME: this ought to be a part of the call interface */
static int  LTT_hinted = 1;  /* FIXME: this too */


/*
 * FreeType interface
 */


/*
=item init_tt()

Initializes the freetype font rendering engine

=cut
*/

undef_int
init_tt() {
  TT_Error  error;
  mm_log((1,"init_tt()\n"));
  error = TT_Init_FreeType( &engine );
  if ( error ){
    mm_log((1,"Initialization of freetype failed, code = 0x%x\n",error));
    return(1);
  }
  return(0);
}


/* 
=item i_tt_get_instance(handle, points, smooth)

Finds a points+smooth instance or if one doesn't exist in the cache
allocates room and returns its cache entry

   fontname - path to the font to load
   handle   - handle to the font.
   points   - points of the requested font
   smooth   - boolean (True: antialias on, False: antialias is off)

=cut
*/

static
int
i_tt_get_instance( TT_Fonthandle *handle, int points, int smooth ) {
  int i,idx;
  TT_Error error;
  
  mm_log((1,"i_tt_get_instance(handle 0x%X, points %d, smooth %d)\n",handle,points,smooth));
  
  if (smooth == -1) { /* Smooth doesn't matter for this search */
    for(i=0;i<TT_CHC;i++) if (handle->instanceh[i].ptsize==points) {
      mm_log((1,"i_tt_get_instance: in cache - (non selective smoothing search) returning %d\n",i));
      return i;
    }
    smooth=1; /* We will be adding a font - add it as smooth then */
  } else { /* Smooth doesn't matter for this search */
    for(i=0;i<TT_CHC;i++) if (handle->instanceh[i].ptsize==points && handle->instanceh[i].smooth==smooth) {
      mm_log((1,"i_tt_get_instance: in cache returning %d\n",i));
      return i;
    }
  }
  
  /* Found the instance in the cache - return the cache index */
  
  for(idx=0;idx<TT_CHC;idx++) if (!(handle->instanceh[idx].order)) break; /* find the lru item */

  mm_log((1,"i_tt_get_instance: lru item is %d\n",idx));
  mm_log((1,"i_tt_get_instance: lru pointer 0x%X\n",USTRCT(handle->instanceh[idx].instance) ));
  
  if ( USTRCT(handle->instanceh[idx].instance) ) {
    mm_log((1,"i_tt_get_instance: freeing lru item from cache %d\n",idx));
    TT_Done_Instance( handle->instanceh[idx].instance ); /* Free instance if needed */
  }
  
  /* create and initialize instance */
  /* FIXME: probably a memory leak on fail */
  
  (void) (( error = TT_New_Instance( handle->face, &handle->instanceh[idx].instance ) ) || 
	  ( error = TT_Set_Instance_Resolutions( handle->instanceh[idx].instance, LTT_dpi, LTT_dpi ) ) ||
	  ( error = TT_Set_Instance_CharSize( handle->instanceh[idx].instance, points*64 ) ) );
  
  if ( error ) {
    mm_log((1, "Could not create and initialize instance: error 0x%x.\n",error ));
    return -1;
  }
  
  /* Now that the instance should the inplace we need to lower all of the
     ru counts and put `this' one with the highest entry */
  
  for(i=0;i<TT_CHC;i++) handle->instanceh[i].order--;

  handle->instanceh[idx].order=TT_CHC-1;
  handle->instanceh[idx].ptsize=points;
  handle->instanceh[idx].smooth=smooth;
  TT_Get_Instance_Metrics( handle->instanceh[idx].instance, &(handle->instanceh[idx].imetrics) );

  /* Zero the memory for the glyph storage so they are not thought as cached if they haven't been cached
     since this new font was loaded */

  for(i=0;i<256;i++) USTRCT(handle->instanceh[idx].glyphs[i])=NULL;
  
  return idx;
}


/*
=item i_tt_new(fontname)

Creates a new font handle object, finds a character map and initialise the
the font handle's cache

   fontname - path to the font to load

=cut
*/

TT_Fonthandle*
i_tt_new(char *fontname) {
  TT_Error error;
  TT_Fonthandle *handle;
  unsigned short i,n;
  unsigned short platform,encoding;
  
  mm_log((1,"i_tt_new(fontname '%s')\n",fontname));
  
  /* allocate memory for the structure */
  
  handle = mymalloc( sizeof(TT_Fonthandle) );

  /* load the typeface */
  error = TT_Open_Face( engine, fontname, &handle->face );
  if ( error ) {
    if ( error == TT_Err_Could_Not_Open_File ) { mm_log((1, "Could not find/open %s.\n", fontname )) }
    else { mm_log((1, "Error while opening %s, error code = 0x%x.\n",fontname, error )); }
    return NULL;
  }
  
  TT_Get_Face_Properties( handle->face, &(handle->properties) );
  /* First, look for a Unicode charmap */
  
  n = handle->properties.num_CharMaps;
  USTRCT( handle->char_map )=NULL; /* Invalidate character map */
  
  for ( i = 0; i < n; i++ ) {
    TT_Get_CharMap_ID( handle->face, i, &platform, &encoding );
    if ( (platform == 3 && encoding == 1 ) || (platform == 0 && encoding == 0 ) ) {
      TT_Get_CharMap( handle->face, i, &(handle->char_map) );
      break;
    }
  }

  /* Zero the pointsizes - and ordering */
  
  for(i=0;i<TT_CHC;i++) {
    USTRCT(handle->instanceh[i].instance)=NULL;
    handle->instanceh[i].order=i;
    handle->instanceh[i].ptsize=0;
    handle->instanceh[i].smooth=-1;
  }

  mm_log((1,"i_tt_new <- 0x%X\n",handle));
  return handle;
}



/*
 * raster map management
 */

/* 
=item i_tt_init_raster_map(bit, width, height, smooth)

Allocates internal memory for the bitmap as needed by the parameters (internal)
		 
   bit    - bitmap to allocate into
   width  - width of the bitmap
   height - height of the bitmap
   smooth - boolean (True: antialias on, False: antialias is off)

=cut
*/

static
void
i_tt_init_raster_map( TT_Raster_Map* bit, int width, int height, int smooth ) {

  mm_log((1,"i_tt_init_raster_map( bit 08x%08X, width %d, height %d, smooth %d)\n", bit, width, height, smooth));
  
  bit->rows  = height;
  bit->width = ( width + 3 ) & -4;
  bit->flow  = TT_Flow_Down;
  
  if ( smooth ) {
    bit->cols  = bit->width;
    bit->size  = bit->rows * bit->width;
  } else {
    bit->cols  = ( bit->width + 7 ) / 8;    /* convert to # of bytes     */
    bit->size  = bit->rows * bit->cols;     /* number of bytes in buffer */
  }
  
  mm_log((1,"i_tt_init_raster_map: bit->width %d, bit->cols %d, bit->rows %d, bit->size %d)\n", bit->width, bit->cols, bit->rows, bit->size ));

  bit->bitmap = (void *) mymalloc( bit->size );
  if ( !bit->bitmap ) m_fatal(0,"Not enough memory to allocate bitmap (%d)!\n",bit->size );
}


/*
=item i_tt_clear_raster_map(bit)

Frees the bitmap data and sets pointer to NULL (internal)
		 
   bit - bitmap to free

=cut
*/

static
void
i_tt_done_raster_map( TT_Raster_Map *bit ) {
  myfree( bit->bitmap );
  bit->bitmap = NULL;
}


/*
=item i_tt_clear_raster_map(bit)

Clears the specified bitmap (internal)
		 
   bit - bitmap to zero

=cut
*/


static
void
i_tt_clear_raster_map( TT_Raster_Map*  bit ) {
  memset( bit->bitmap, 0, bit->size );
}


/* 
=item i_tt_blit_or(dst, src, x_off, y_off)

function that blits one raster map into another (internal)
		 
   dst   - destination bitmap
   src   - source bitmap
   x_off - x offset into the destination bitmap
   y_off - y offset into the destination bitmap

=cut
*/

static
void
i_tt_blit_or( TT_Raster_Map *dst, TT_Raster_Map *src,int x_off, int y_off ) {
  int  x,  y;
  int  x1, x2, y1, y2;
  char *s, *d;
  
  x1 = x_off < 0 ? -x_off : 0;
  y1 = y_off < 0 ? -y_off : 0;
  
  x2 = (int)dst->cols - x_off;
  if ( x2 > src->cols ) x2 = src->cols;
  
  y2 = (int)dst->rows - y_off;
  if ( y2 > src->rows ) y2 = src->rows;

  if ( x1 >= x2 ) return;

  /* do the real work now */

  for ( y = y1; y < y2; ++y ) {
    s = ( (char*)src->bitmap ) + y * src->cols + x1;
    d = ( (char*)dst->bitmap ) + ( y + y_off ) * dst->cols + x1 + x_off;
    
    for ( x = x1; x < x2; ++x ) {
      if (*s > *d)
	*d = *s;
      d++;
      s++;
    }
  }
}

/* useful for debugging */
#if 0

static void dump_raster_map(FILE *out, TT_Raster_Map *bit ) {
  int x, y;
  fprintf(out, "cols %d rows %d  flow %d\n", bit->cols, bit->rows, bit->flow);
  for (y = 0; y < bit->rows; ++y) {
    fprintf(out, "%2d:", y);
    for (x = 0; x < bit->cols; ++x) {
      if ((x & 7) == 0 && x) putc(' ', out);
      fprintf(out, "%02x", ((unsigned char *)bit->bitmap)[y*bit->cols+x]);
    }
    putc('\n', out);
  }
}

#endif

/* 
=item i_tt_get_glyph(handle, inst, j) 

Function to see if a glyph exists and if so cache it (internal)
		 
   handle - pointer to font handle
   inst   - font instance
   j      - charcode of glyph

=cut
*/

static
int
i_tt_get_glyph( TT_Fonthandle *handle, int inst, unsigned char j) { /* FIXME: Check if unsigned char is enough */
  unsigned short load_flags, code;
  TT_Error error;

  mm_log((1, "i_tt_get_glyph(handle 0x%X, inst %d, j %d (%c))\n",handle,inst,j,j));
  mm_log((1, "handle->instanceh[inst].glyphs[j]=0x%08X\n",handle->instanceh[inst].glyphs[j] ));

  if ( TT_VALID(handle->instanceh[inst].glyphs[j]) ) {
    mm_log((1,"i_tt_get_glyph: %d in cache\n",j));
    return 1;
  }
  
  /* Ok - it wasn't cached - try to get it in */
  load_flags = TTLOAD_SCALE_GLYPH;
  if ( LTT_hinted ) load_flags |= TTLOAD_HINT_GLYPH;
  
  if ( !TT_VALID(handle->char_map) ) {
    code = (j - ' ' + 1) < 0 ? 0 : (j - ' ' + 1);
    if ( code >= handle->properties.num_Glyphs ) code = 0;
  } else code = TT_Char_Index( handle->char_map, j );
  
  if ( (error = TT_New_Glyph( handle->face, &handle->instanceh[inst].glyphs[j])) ) 
    mm_log((1, "Cannot allocate and load glyph: error 0x%x.\n", error ));
  if ( (error = TT_Load_Glyph( handle->instanceh[inst].instance, handle->instanceh[inst].glyphs[j], code, load_flags)) )
    mm_log((1, "Cannot allocate and load glyph: error 0x%x.\n", error ));
  
  /* At this point the glyph should be allocated and loaded */
  /* Next get the glyph metrics */
  
  error = TT_Get_Glyph_Metrics( handle->instanceh[inst].glyphs[j], &handle->instanceh[inst].gmetrics[j] );
  mm_log((1, "TT_Get_Glyph_Metrics: error 0x%x.\n", error ));
  return 1;
}


/* 
=item i_tt_destroy(handle)

Clears the data taken by a font including all cached data such as
pixmaps and glyphs
		 
   handle - pointer to font handle

=cut
*/

void
i_tt_destroy( TT_Fonthandle *handle) {
  TT_Close_Face( handle->face );
  myfree( handle );
  
  /* FIXME: Should these be freed automatically by the library? 

  TT_Done_Instance( instance );
  void
    i_tt_done_glyphs( void ) {
    int  i;

    if ( !glyphs ) return;
    
    for ( i = 0; i < 256; ++i ) TT_Done_Glyph( glyphs[i] );
    free( glyphs );
    
    glyphs = NULL;
  }
  */
}


/*
 * FreeType Rendering functions
 */


/* 
=item i_tt_render_glyph(handle, gmetrics, bit, smallbit, x_off, y_off, smooth)

Renders a single glyph into the bit rastermap (internal)

   handle   - pointer to font handle
   gmetrics - the metrics for the glyph to be rendered
   bit      - large bitmap that is the destination for the text
   smallbit - small bitmap that is used only if smooth is true
   x_off    - x offset of glyph
   y_off    - y offset of glyph
   smooth   - boolean (True: antialias on, False: antialias is off)

=cut
*/

static
void
i_tt_render_glyph( TT_Glyph glyph, TT_Glyph_Metrics* gmetrics, TT_Raster_Map *bit, TT_Raster_Map *small_bit, int x_off, int y_off, int smooth ) {
  
  mm_log((1,"i_tt_render_glyph(glyph 0x0%X, gmetrics 0x0%X, bit 0x%X, small_bit 0x%X, x_off %d, y_off %d, smooth %d)\n",
	  USTRCT(glyph), gmetrics, bit, small_bit, x_off,y_off,smooth));
  
  if ( !smooth ) TT_Get_Glyph_Bitmap( glyph, bit, x_off * 64, y_off * 64);
  else {
    TT_F26Dot6 xmin, ymin, xmax, ymax;

    xmin =  gmetrics->bbox.xMin & -64;
    ymin =  gmetrics->bbox.yMin & -64;
    xmax = (gmetrics->bbox.xMax + 63) & -64;
    ymax = (gmetrics->bbox.yMax + 63) & -64;
    
    i_tt_clear_raster_map( small_bit );
    TT_Get_Glyph_Pixmap( glyph, small_bit, -xmin, -ymin );
    i_tt_blit_or( bit, small_bit, xmin/64 + x_off, -ymin/64 - y_off );
  }
}


/*
=item i_tt_render_all_glyphs(handle, inst, bit, small_bit, cords, txt, len, smooth)

calls i_tt_render_glyph to render each glyph into the bit rastermap (internal)

   handle   - pointer to font handle
   inst     - font instance
   bit      - large bitmap that is the destination for the text
   smallbit - small bitmap that is used only if smooth is true
   txt      - string to render
   len      - length of the string to render
   smooth   - boolean (True: antialias on, False: antialias is off)

=cut
*/

static
void
i_tt_render_all_glyphs( TT_Fonthandle *handle, int inst, TT_Raster_Map *bit, TT_Raster_Map *small_bit, int cords[6], char* txt, int len, int smooth ) {
  unsigned char j;
  int i;
  TT_F26Dot6 x,y;
  
  mm_log((1,"i_tt_render_all_glyphs( handle 0x%X, inst %d, bit 0x%X, small_bit 0x%X, txt '%.*s', len %d, smooth %d)\n",
	  handle, inst, bit, small_bit, len, txt, len, smooth));
  
  /* 
     y=-( handle->properties.horizontal->Descender * handle->instanceh[inst].imetrics.y_ppem )/(handle->properties.header->Units_Per_EM);
  */

  x=-cords[0]; /* FIXME: If you font is antialiased this should be expanded by one to allow for aa expansion and the allocation too - do before passing here */
  y=-cords[1];
  
  for ( i = 0; i < len; i++ ) {
    j = txt[i];
    if ( !i_tt_get_glyph(handle,inst,j) ) continue;
    i_tt_render_glyph( handle->instanceh[inst].glyphs[j], &handle->instanceh[inst].gmetrics[j], bit, small_bit, x, y, smooth );
    x += handle->instanceh[inst].gmetrics[j].advance / 64;
  }
}


/*
 * Functions to render rasters (single channel images) onto images
 */

/* 
=item i_tt_dump_raster_map2(im, bit, xb, yb, cl, smooth)

Function to dump a raster onto an image in color used by i_tt_text() (internal).

   im     - image to dump raster on
   bit    - bitmap that contains the text to be dumped to im
   xb, yb - coordinates, left edge and baseline
   cl     - color to use for text
   smooth - boolean (True: antialias on, False: antialias is off)

=cut
*/

static
void
i_tt_dump_raster_map2( i_img* im, TT_Raster_Map* bit, int xb, int yb, i_color *cl, int smooth ) {
  char *bmap;
  i_color val;
  int c, i, ch, x, y;
  mm_log((1,"i_tt_dump_raster_map2(im 0x%x, bit 0x%X, xb %d, yb %d, cl 0x%X)\n",im,bit,xb,yb,cl));
  
  bmap = (char *)bit->bitmap;

  if ( smooth ) {

    for(y=0;y<bit->rows;y++) for(x=0;x<bit->width;x++) {
      c=(255*bmap[y*(bit->cols)+x])/4;
      i=255-c;
      i_gpix(im,x+xb,y+yb,&val);
      for(ch=0;ch<im->channels;ch++) val.channel[ch]=(c*cl->channel[ch]+i*val.channel[ch])/255;
      i_ppix(im,x+xb,y+yb,&val);
    }

  } else {

    for(y=0;y<bit->rows;y++) for(x=0;x<bit->width;x++) {
      c=( bmap[y*bit->cols+x/8] & (128>>(x%8)) ) ? 255 : 0;
      i=255-c;
      i_gpix(im,x+xb,y+yb,&val);
      for(ch=0;ch<im->channels;ch++) val.channel[ch]=(c*cl->channel[ch]+i*val.channel[ch])/255;
      i_ppix(im,x+xb,y+yb,&val);
    }

  }
}


/*
=item i_tt_dump_raster_map_channel(im, bit, xb, yb, channel, smooth)

Function to dump a raster onto a single channel image in color (internal)

   im      - image to dump raster on
   bit     - bitmap that contains the text to be dumped to im
   xb, yb  - coordinates, left edge and baseline
   channel - channel to copy to
   smooth  - boolean (True: antialias on, False: antialias is off)

=cut
*/

static
void
i_tt_dump_raster_map_channel( i_img* im, TT_Raster_Map*  bit, int xb, int yb, int channel, int smooth ) {
  char *bmap;
  i_color val;
  int c,x,y;

  mm_log((1,"i_tt_dump_raster_channel(im 0x%x, bit 0x%X, xb %d, yb %d, channel %d)\n",im,bit,xb,yb,channel));
  
  bmap = (char *)bit->bitmap;
  
  if ( smooth ) {
    for(y=0;y<bit->rows;y++) for(x=0;x<bit->width;x++) {
      c=(255*bmap[y*(bit->cols)+x])/4;
      i_gpix(im,x+xb,y+yb,&val);
      val.channel[channel]=c;
      i_ppix(im,x+xb,y+yb,&val);
    }
  } else {
    for(y=0;y<bit->rows;y++) for(x=0;x<bit->width;x++) {
      c=( bmap[y*bit->cols+x/8] & (128>>(x%8)) ) ? 255 : 0;
      i_gpix(im,x+xb,y+yb,&val);
      val.channel[channel]=c;
      i_ppix(im,x+xb,y+yb,&val);
    }
  }
}


/* 
=item i_tt_rasterize(handle, bit, cords, points, txt, len, smooth) 

interface for generating single channel raster of text (internal)

   handle - pointer to font handle
   bit    - the bitmap that is allocated, rendered into and NOT freed
   cords  - the bounding box (modified in place)
   points - font size to use
   txt    - string to render
   len    - length of the string to render
   smooth - boolean (True: antialias on, False: antialias is off)

=cut
*/

static
int
i_tt_rasterize( TT_Fonthandle *handle, TT_Raster_Map *bit, int cords[6], float points, char* txt, int len, int smooth ) {
  int inst;
  int width, height;
  TT_Raster_Map small_bit;
  
  /* find or install an instance */
  if ( (inst=i_tt_get_instance(handle,points,smooth)) < 0) { 
    mm_log((1,"i_tt_rasterize: get instance failed\n"));
    return 0;
  }
  
  /* calculate bounding box */
  i_tt_bbox_inst( handle, inst, txt, len, cords );
  
  width  = cords[2]-cords[0];
  height = cords[3]-cords[1];
  
  mm_log((1,"i_tt_rasterize: width=%d, height=%d\n",width, height )); 
  
  i_tt_init_raster_map ( bit, width, height, smooth );
  i_tt_clear_raster_map( bit );
  if ( smooth ) i_tt_init_raster_map( &small_bit, handle->instanceh[inst].imetrics.x_ppem + 32, height, smooth );
  
  i_tt_render_all_glyphs( handle, inst, bit, &small_bit, cords, txt, len, smooth );

  /*  ascent = ( handle->properties.horizontal->Ascender  * handle->instanceh[inst].imetrics.y_ppem ) / handle->properties.header->Units_Per_EM; */
  
  if ( smooth ) i_tt_done_raster_map( &small_bit );
  return 1;
}



/* 
 * Exported text rendering interfaces
 */


/*
=item i_tt_cp(handle, im, xb, yb, channel, points, txt, len, smooth)

Interface to text rendering into a single channel in an image

   handle  - pointer to font handle
   im      - image to render text on to
   xb, yb  - coordinates, left edge and baseline
   channel - channel to render into
   points  - font size to use
   txt     - string to render
   len     - length of the string to render
   smooth  - boolean (True: antialias on, False: antialias is off)

=cut
*/

undef_int
i_tt_cp( TT_Fonthandle *handle, i_img *im, int xb, int yb, int channel, float points, char* txt, int len, int smooth ) {

  int cords[6];
  int ascent, st_offset;
  TT_Raster_Map bit;
  
  if (! i_tt_rasterize( handle, &bit, cords, points, txt, len, smooth ) ) return 0;
  
  ascent=cords[3];
  st_offset=cords[0];

  i_tt_dump_raster_map_channel( im, &bit, xb-st_offset , yb-ascent, channel, smooth );
  i_tt_done_raster_map( &bit );

  return 1;
}


/* 
=item i_tt_text(handle, im, xb, yb, cl, points, txt, len, smooth) 

Interface to text rendering in a single color onto an image

   handle  - pointer to font handle
   im      - image to render text on to
   xb, yb  - coordinates, left edge and baseline
   cl      - color to use for text
   points  - font size to use
   txt     - string to render
   len     - length of the string to render
   smooth  - boolean (True: antialias on, False: antialias is off)

=cut
*/

undef_int
i_tt_text( TT_Fonthandle *handle, i_img *im, int xb, int yb, i_color *cl, float points, char* txt, int len, int smooth) {
  int cords[6];
  int ascent, st_offset;
  TT_Raster_Map bit;
  
  if (! i_tt_rasterize( handle, &bit, cords, points, txt, len, smooth ) ) return 0;
  
  ascent=cords[3];
  st_offset=cords[0];

  i_tt_dump_raster_map2( im, &bit, xb+st_offset, yb-ascent, cl, smooth ); 
  i_tt_done_raster_map( &bit );

  return 1;
}


/*
=item i_tt_bbox_inst(handle, inst, txt, len, cords) 

Function to get texts bounding boxes given the instance of the font (internal)

   handle - pointer to font handle
   inst   -  font instance
   txt    -  string to measure
   len    -  length of the string to render
   cords  - the bounding box (modified in place)

=cut
*/

static
undef_int
i_tt_bbox_inst( TT_Fonthandle *handle, int inst ,const char *txt, int len, int cords[6] ) {
  int i, upm, ascent, descent, gascent, gdescent, width, casc, cdesc, first, start;
  unsigned int j;
  unsigned char *ustr;
  ustr=(unsigned char*)txt;
  
  mm_log((1,"i_tt_box_inst(handle 0x%X,inst %d,txt '%.*s', len %d)\n",handle,inst,len,txt,len));

  upm     = handle->properties.header->Units_Per_EM;
  gascent  = ( handle->properties.horizontal->Ascender  * handle->instanceh[inst].imetrics.y_ppem + upm - 1) / upm;
  gdescent = ( handle->properties.horizontal->Descender * handle->instanceh[inst].imetrics.y_ppem - upm + 1) / upm;
  
  width   = 0;
  start   = 0;
  
  mm_log((1, "i_tt_box_inst: gascent=%d gdescent=%d\n", gascent, gdescent));

  first=1;
  for ( i = 0; i < len; ++i ) {
    j = ustr[i];
    if ( i_tt_get_glyph(handle,inst,j) ) {
      TT_Glyph_Metrics *gm = handle->instanceh[inst].gmetrics + j;
      width += gm->advance   / 64;
      casc   = (gm->bbox.yMax+63) / 64;
      cdesc  = (gm->bbox.yMin-63) / 64;

      mm_log((1, "i_tt_box_inst: glyph='%c' casc=%d cdesc=%d\n", j, casc, cdesc));

      if (first) {
	start    = gm->bbox.xMin / 64;
	ascent   = (gm->bbox.yMax+63) / 64;
	descent  = (gm->bbox.yMin-63) / 64;
	first = 0;
      }
      if (i == len-1) {
	/* the right-side bearing - in case the right-side of a 
	   character goes past the right of the advance width,
	   as is common for italic fonts
	*/
	int rightb = gm->advance - gm->bearingX 
	  - (gm->bbox.xMax - gm->bbox.xMin);
	/* fprintf(stderr, "font info last: %d %d %d %d\n", 
	   gm->bbox.xMax, gm->bbox.xMin, gm->advance, rightb); */
	if (rightb < 0)
	  width -= rightb/64;
      }

      ascent  = (ascent  >  casc ?  ascent : casc );
      descent = (descent < cdesc ? descent : cdesc);
    }
  }
  
  cords[0]=start;
  cords[1]=gdescent;
  cords[2]=width;
  cords[3]=gascent;
  cords[4]=descent;
  cords[5]=ascent;
  return 1;
}


/*
=item i_tt_bbox(handle, points, txt, len, cords)

Interface to get a strings bounding box

   handle - pointer to font handle
   points - font size to use
   txt    - string to render
   len    - length of the string to render
   cords  - the bounding box (modified in place)

=cut
*/

undef_int
i_tt_bbox( TT_Fonthandle *handle, float points,char *txt,int len,int cords[6]) {
  int inst;
  
  mm_log((1,"i_tt_box(handle 0x%X,points %f,txt '%.*s', len %d)\n",handle,points,len,txt,len));

  if ( (inst=i_tt_get_instance(handle,points,-1)) < 0) {
    mm_log((1,"i_tt_text: get instance failed\n"));
    return 0;
  }

  return i_tt_bbox_inst(handle, inst, txt, len, cords);
}



#endif /* HAVE_LIBTT */


/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>

=head1 SEE ALSO

Imager(3)

=cut
*/
