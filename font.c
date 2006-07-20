#include "imager.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_LIBT1
#include <t1lib.h>
#endif


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
  rc = i_tt_bbox(handle, points, "foo", 3, int cords[6], utf8);
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
i_init_fonts(int t1log) {
  mm_log((1,"Initializing fonts\n"));

#ifdef HAVE_LIBT1
  i_init_t1(t1log);
#endif
  
#ifdef HAVE_LIBTT
  i_init_tt();
#endif

#ifdef HAVE_FT2
  if (!i_ft2_init())
    return 0;
#endif

  return(1); /* FIXME: Always true - check the return values of the init_t1 and init_tt functions */
}




#ifdef HAVE_LIBT1

static int t1_get_flags(char const *flags);
static char *t1_from_utf8(char const *in, int len, int *outlen);

static void t1_push_error(void);

static int t1_active_fonts = 0;
static int t1_initialized = 0;

/* 
=item i_init_t1(t1log)

Initializes the t1lib font rendering engine.

=cut
*/

undef_int
i_init_t1(int t1log) {
  int init_flags = IGNORE_CONFIGFILE|IGNORE_FONTDATABASE;
  mm_log((1,"init_t1()\n"));

  if (t1_active_fonts) {
    mm_log((1, "Cannot re-initialize T1 - active fonts\n"));
    return 1;
  }

  if (t1_initialized) {
    T1_CloseLib();
  }
  
  if (t1log)
    init_flags |= LOGFILE;
  if ((T1_InitLib(init_flags) == NULL)){
    mm_log((1,"Initialization of t1lib failed\n"));
    return(1);
  }
  T1_SetLogLevel(T1LOG_DEBUG);
  i_t1_set_aa(1); /* Default Antialias value */

  ++t1_initialized;

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
  t1_initialized = 0;
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

  ++t1_active_fonts;

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

  --t1_active_fonts;

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
i_t1_cp(i_img *im,int xb,int yb,int channel,int fontnum,float points,char* str,int len,int align, int utf8, char const *flags) {
  GLYPH *glyph;
  int xsize,ysize,x,y;
  i_color val;
  int mod_flags = t1_get_flags(flags);

  unsigned int ch_mask_store;
  
  if (im == NULL) { mm_log((1,"i_t1_cp: Null image in input\n")); return(0); }

  if (utf8) {
    int worklen;
    char *work = t1_from_utf8(str, len, &worklen);
    glyph=T1_AASetString( fontnum, work, worklen, 0, mod_flags, points, NULL);
    myfree(work);
  }
  else {
    glyph=T1_AASetString( fontnum, str, len, 0, mod_flags, points, NULL);
  }
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

static void
t1_fix_bbox(BBox *bbox, const char *str, int len, int advance, 
	    int space_position) {
  /* never called with len == 0 */
  if (str[0] == space_position && bbox->llx > 0)
    bbox->llx = 0;
  if (str[len-1] == space_position && bbox->urx < advance)
    bbox->urx = advance;
  if (bbox->lly > bbox->ury)
    bbox->lly = bbox->ury = 0; 
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

int
i_t1_bbox(int fontnum,float points,const char *str,int len,int cords[6], int utf8,char const *flags) {
  BBox bbox;
  BBox gbbox;
  int mod_flags = t1_get_flags(flags);
  int advance;
  int space_position = T1_GetEncodingIndex(fontnum, "space");
  
  mm_log((1,"i_t1_bbox(fontnum %d,points %.2f,str '%.*s', len %d)\n",fontnum,points,len,str,len));
  T1_LoadFont(fontnum);  /* FIXME: Here a return code is ignored - haw haw haw */ 

  if (len == 0) {
    /* len == 0 has special meaning to T1lib, but it means there's
       nothing to draw, so return that */
    bbox.llx = bbox.lly = bbox.urx = bbox.ury = 0;
    advance = 0;
  }
  else {
    if (utf8) {
      int worklen;
      char *work = t1_from_utf8(str, len, &worklen);
      advance = T1_GetStringWidth(fontnum, work, worklen, 0, mod_flags);
      bbox = T1_GetStringBBox(fontnum,work,worklen,0,mod_flags);
      t1_fix_bbox(&bbox, work, worklen, advance, space_position);
      myfree(work);
    }
    else {
      advance = T1_GetStringWidth(fontnum, (char *)str, len, 0, mod_flags);
      bbox = T1_GetStringBBox(fontnum,(char *)str,len,0,mod_flags);
      t1_fix_bbox(&bbox, str, len, advance, space_position);
    }
  }
  gbbox = T1_GetFontBBox(fontnum);
  
  mm_log((1,"bbox: (%d,%d,%d,%d)\n",
	  (int)(bbox.llx*points/1000),
	  (int)(gbbox.lly*points/1000),
	  (int)(bbox.urx*points/1000),
	  (int)(gbbox.ury*points/1000),
	  (int)(bbox.lly*points/1000),
	  (int)(bbox.ury*points/1000) ));


  cords[BBOX_NEG_WIDTH]=((float)bbox.llx*points)/1000;
  cords[BBOX_POS_WIDTH]=((float)bbox.urx*points)/1000;

  cords[BBOX_GLOBAL_DESCENT]=((float)gbbox.lly*points)/1000;
  cords[BBOX_GLOBAL_ASCENT]=((float)gbbox.ury*points)/1000;

  cords[BBOX_DESCENT]=((float)bbox.lly*points)/1000;
  cords[BBOX_ASCENT]=((float)bbox.ury*points)/1000;

  cords[BBOX_ADVANCE_WIDTH] = ((float)advance * points)/1000;
  cords[BBOX_RIGHT_BEARING] = 
    cords[BBOX_ADVANCE_WIDTH] - cords[BBOX_POS_WIDTH];

  return BBOX_RIGHT_BEARING+1;
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
i_t1_text(i_img *im,int xb,int yb,const i_color *cl,int fontnum,float points,const char* str,int len,int align, int utf8, char const *flags) {
  GLYPH *glyph;
  int xsize,ysize,x,y,ch;
  i_color val;
  unsigned char c,i;
  int mod_flags = t1_get_flags(flags);

  if (im == NULL) { mm_log((1,"i_t1_cp: Null image in input\n")); return(0); }

  if (utf8) {
    int worklen;
    char *work = t1_from_utf8(str, len, &worklen);
    glyph=T1_AASetString( fontnum, work, worklen, 0, mod_flags, points, NULL);
    myfree(work);
  }
  else {
    /* T1_AASetString() accepts a char * not a const char */
    glyph=T1_AASetString( fontnum, (char *)str, len, 0, mod_flags, points, NULL);
  }
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

/*
=item t1_get_flags(flags)

Processes the characters in I<flags> to create a mod_flags value used
by some T1Lib functions.

=cut
 */
static int
t1_get_flags(char const *flags) {
  int mod_flags = T1_KERNING;

  while (*flags) {
    switch (*flags++) {
    case 'u': case 'U': mod_flags |= T1_UNDERLINE; break;
    case 'o': case 'O': mod_flags |= T1_OVERLINE; break;
    case 's': case 'S': mod_flags |= T1_OVERSTRIKE; break;
      /* ignore anything we don't recognize */
    }
  }

  return mod_flags;
}

/*
=item t1_from_utf8(char const *in, int len, int *outlen)

Produces an unencoded version of I<in> by dropping any Unicode
character over 255.

Returns a newly allocated buffer which should be freed with myfree().
Sets *outlen to the number of bytes used in the output string.

=cut
*/

static char *
t1_from_utf8(char const *in, int len, int *outlen) {
  /* at this point len is from a perl SV, so can't approach MAXINT */
  char *out = mymalloc(len+1); /* checked 5Nov05 tonyc */
  char *p = out;
  unsigned long c;

  while (len) {
    c = i_utf8_advance(&in, &len);
    if (c == ~0UL) {
      myfree(out);
      i_push_error(0, "invalid UTF8 character");
      return 0;
    }
    /* yeah, just drop them */
    if (c < 0x100) {
      *p++ = (char)c;
    }
  }
  *p = '\0';
  *outlen = p - out;

  return out;
}

/*
=item i_t1_has_chars(font_num, text, len, utf8, out)

Check if the given characters are defined by the font.  Note that len
is the number of bytes, not the number of characters (when utf8 is
non-zero).

out[char index] will be true if the character exists.

Accepts UTF-8, but since T1 can only have 256 characters, any chars
with values over 255 will simply be returned as false.

Returns the number of characters that were checked.

=cut
*/

int
i_t1_has_chars(int font_num, const char *text, int len, int utf8,
               char *out) {
  int count = 0;
  
  mm_log((1, "i_t1_has_chars(font_num %d, text %p, len %d, utf8 %d)\n", 
          font_num, text, len, utf8));

  i_clear_error();
  if (T1_LoadFont(font_num)) {
    t1_push_error();
    return 0;
  }

  while (len) {
    unsigned long c;
    if (utf8) {
      c = i_utf8_advance(&text, &len);
      if (c == ~0UL) {
        i_push_error(0, "invalid UTF8 character");
        return 0;
      }
    }
    else {
      c = (unsigned char)*text++;
      --len;
    }
    
    if (c >= 0x100) {
      /* limit of 256 characters for T1 */
      *out++ = 0;
    }
    else {
      char const * name = T1_GetCharName(font_num, (unsigned char)c);

      if (name) {
        *out++ = strcmp(name, ".notdef") != 0;
      }
      else {
        mm_log((2, "  No name found for character %lx\n", c));
        *out++ = 0;
      }
    }
    ++count;
  }

  return count;
}

/*
=item i_t1_face_name(font_num, name_buf, name_buf_size)

Copies the face name of the given C<font_num> to C<name_buf>.  Returns
the number of characters required to store the name (which can be
larger than C<name_buf_size>, including the space required to store
the terminating NUL).

If name_buf is too small (as specified by name_buf_size) then the name
will be truncated.  name_buf will always be NUL termintaed.

=cut
*/

int
i_t1_face_name(int font_num, char *name_buf, size_t name_buf_size) {
  char *name;

  T1_errno = 0;
  if (T1_LoadFont(font_num)) {
    t1_push_error();
    return 0;
  }
  name = T1_GetFontName(font_num);

  if (name) {
    strncpy(name_buf, name, name_buf_size);
    name_buf[name_buf_size-1] = '\0';
    return strlen(name) + 1;
  }
  else {
    t1_push_error();
    return 0;
  }
}

int
i_t1_glyph_name(int font_num, unsigned long ch, char *name_buf, 
                 size_t name_buf_size) {
  char *name;

  i_clear_error();
  if (ch > 0xFF) {
    return 0;
  }
  if (T1_LoadFont(font_num)) {
    t1_push_error();
    return 0;
  }
  name = T1_GetCharName(font_num, (unsigned char)ch);
  if (name) {
    if (strcmp(name, ".notdef")) {
      strncpy(name_buf, name, name_buf_size);
      name_buf[name_buf_size-1] = '\0';
      return strlen(name) + 1;
    }
    else {
      return 0;
    }
  }
  else {
    t1_push_error();
    return 0;
  }
}

static void
t1_push_error(void) {
  switch (T1_errno) {
  case 0: 
    i_push_error(0, "No error"); 
    break;

#ifdef T1ERR_SCAN_FONT_FORMAT
  case T1ERR_SCAN_FONT_FORMAT:
    i_push_error(T1ERR_SCAN_FONT_FORMAT, "SCAN_FONT_FORMAT"); 
    break;
#endif

#ifdef T1ERR_SCAN_FILE_OPEN_ERR
  case T1ERR_SCAN_FILE_OPEN_ERR:
    i_push_error(T1ERR_SCAN_FILE_OPEN_ERR, "SCAN_FILE_OPEN_ERR"); 
    break;
#endif

#ifdef T1ERR_SCAN_OUT_OF_MEMORY
  case T1ERR_SCAN_OUT_OF_MEMORY:
    i_push_error(T1ERR_SCAN_OUT_OF_MEMORY, "SCAN_OUT_OF_MEMORY"); 
    break;
#endif

#ifdef T1ERR_SCAN_ERROR
  case T1ERR_SCAN_ERROR:
    i_push_error(T1ERR_SCAN_ERROR, "SCAN_ERROR"); 
    break;
#endif

#ifdef T1ERR_SCAN_FILE_EOF
  case T1ERR_SCAN_FILE_EOF:
    i_push_error(T1ERR_SCAN_FILE_EOF, "SCAN_FILE_EOF"); 
    break;
#endif

#ifdef T1ERR_PATH_ERROR
  case T1ERR_PATH_ERROR:
    i_push_error(T1ERR_PATH_ERROR, "PATH_ERROR"); 
    break;
#endif

#ifdef T1ERR_PARSE_ERROR
  case T1ERR_PARSE_ERROR:
    i_push_error(T1ERR_PARSE_ERROR, "PARSE_ERROR"); 
    break;
#endif

#ifdef T1ERR_TYPE1_ABORT
  case T1ERR_TYPE1_ABORT:
    i_push_error(T1ERR_TYPE1_ABORT, "TYPE1_ABORT"); 
    break;
#endif

#ifdef T1ERR_INVALID_FONTID
  case T1ERR_INVALID_FONTID:
    i_push_error(T1ERR_INVALID_FONTID, "INVALID_FONTID"); 
    break;
#endif

#ifdef T1ERR_INVALID_PARAMETER
  case T1ERR_INVALID_PARAMETER:
    i_push_error(T1ERR_INVALID_PARAMETER, "INVALID_PARAMETER"); 
    break;
#endif

#ifdef T1ERR_OP_NOT_PERMITTED
  case T1ERR_OP_NOT_PERMITTED:
    i_push_error(T1ERR_OP_NOT_PERMITTED, "OP_NOT_PERMITTED"); 
    break;
#endif

#ifdef T1ERR_ALLOC_MEM
  case T1ERR_ALLOC_MEM:
    i_push_error(T1ERR_ALLOC_MEM, "ALLOC_MEM"); 
    break;
#endif

#ifdef T1ERR_FILE_OPEN_ERR
  case T1ERR_FILE_OPEN_ERR:
    i_push_error(T1ERR_FILE_OPEN_ERR, "FILE_OPEN_ERR"); 
    break;
#endif

#ifdef T1ERR_UNSPECIFIED
  case T1ERR_UNSPECIFIED:
    i_push_error(T1ERR_UNSPECIFIED, "UNSPECIFIED"); 
    break;
#endif

#ifdef T1ERR_NO_AFM_DATA
  case T1ERR_NO_AFM_DATA:
    i_push_error(T1ERR_NO_AFM_DATA, "NO_AFM_DATA"); 
    break;
#endif

#ifdef T1ERR_X11
  case T1ERR_X11:
    i_push_error(T1ERR_X11, "X11"); 
    break;
#endif

#ifdef T1ERR_COMPOSITE_CHAR
  case T1ERR_COMPOSITE_CHAR:
    i_push_error(T1ERR_COMPOSITE_CHAR, "COMPOSITE_CHAR"); 
    break;
#endif

  default:
    i_push_errorf(T1_errno, "unknown error %d", (int)T1_errno);
  }
}

#endif /* HAVE_LIBT1 */


/* Truetype font support */
#ifdef HAVE_LIBTT

/* These are enabled by default when configuring Freetype 1.x
   I haven't a clue how to reliably detect it at compile time.

   We need a compilation probe in Makefile.PL
*/
#define FTXPOST 1
#define FTXERR18 1

#include <freetype.h>
#define TT_CHC 5

#ifdef FTXPOST
#include <ftxpost.h>
#endif

#ifdef FTXERR18
#include <ftxerr18.h>
#endif

/* some versions of FT1.x don't seem to define this - it's font defined
   so it won't change */
#ifndef TT_MS_LANGID_ENGLISH_GENERAL
#define TT_MS_LANGID_ENGLISH_GENERAL 0x0409
#endif

/* convert a code point into an index in the glyph cache */
#define TT_HASH(x) ((x) & 0xFF)

typedef struct i_glyph_entry_ {
  TT_Glyph glyph;
  unsigned long ch;
} i_tt_glyph_entry;

#define TT_NOCHAR (~0UL)

struct TT_Instancehandle_ {
  TT_Instance instance;
  TT_Instance_Metrics imetrics;
  TT_Glyph_Metrics gmetrics[256];
  i_tt_glyph_entry glyphs[256];
  int smooth;
  int ptsize;
  int order;
};

typedef struct TT_Instancehandle_ TT_Instancehandle;

struct TT_Fonthandle_ {
  TT_Face face;
  TT_Face_Properties properties;
  TT_Instancehandle instanceh[TT_CHC];
  TT_CharMap char_map;
#ifdef FTXPOST
  int loaded_names;
  TT_Error load_cond;
#endif
};

/* Defines */

#define USTRCT(x) ((x).z)
#define TT_VALID( handle )  ( ( handle ).z != NULL )

static void i_tt_push_error(TT_Error rc);

/* Prototypes */

static  int i_tt_get_instance( TT_Fonthandle *handle, int points, int smooth );
static void i_tt_init_raster_map( TT_Raster_Map* bit, int width, int height, int smooth );
static void i_tt_done_raster_map( TT_Raster_Map *bit );
static void i_tt_clear_raster_map( TT_Raster_Map* bit );
static void i_tt_blit_or( TT_Raster_Map *dst, TT_Raster_Map *src,int x_off, int y_off );
static  int i_tt_get_glyph( TT_Fonthandle *handle, int inst, unsigned long j );
static void 
i_tt_render_glyph( TT_Glyph glyph, TT_Glyph_Metrics* gmetrics, 
                   TT_Raster_Map *bit, TT_Raster_Map *small_bit, 
                   int x_off, int y_off, int smooth );
static int
i_tt_render_all_glyphs( TT_Fonthandle *handle, int inst, TT_Raster_Map *bit, 
                        TT_Raster_Map *small_bit, int cords[6], 
                        char const* txt, int len, int smooth, int utf8 );
static void i_tt_dump_raster_map2( i_img* im, TT_Raster_Map* bit, int xb, int yb, const i_color *cl, int smooth );
static void i_tt_dump_raster_map_channel( i_img* im, TT_Raster_Map* bit, int xb, int yb, int channel, int smooth );
static  int
i_tt_rasterize( TT_Fonthandle *handle, TT_Raster_Map *bit, int cords[6], 
                float points, char const* txt, int len, int smooth, int utf8 );
static undef_int i_tt_bbox_inst( TT_Fonthandle *handle, int inst ,const char *txt, int len, int cords[6], int utf8 );


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
i_init_tt() {
  TT_Error  error;
  mm_log((1,"init_tt()\n"));
  error = TT_Init_FreeType( &engine );
  if ( error ){
    mm_log((1,"Initialization of freetype failed, code = 0x%x\n",error));
    return(1);
  }

#ifdef FTXPOST
  error = TT_Init_Post_Extension( engine );
  if (error) {
    mm_log((1, "Initialization of Post extension failed = 0x%x\n", error));
    return 1;
  }
#endif

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
  
  mm_log((1,"i_tt_get_instance(handle 0x%X, points %d, smooth %d)\n",
          handle,points,smooth));
  
  if (smooth == -1) { /* Smooth doesn't matter for this search */
    for(i=0;i<TT_CHC;i++) {
      if (handle->instanceh[i].ptsize==points) {
        mm_log((1,"i_tt_get_instance: in cache - (non selective smoothing search) returning %d\n",i));
        return i;
      }
    }
    smooth=1; /* We will be adding a font - add it as smooth then */
  } else { /* Smooth doesn't matter for this search */
    for(i=0;i<TT_CHC;i++) {
      if (handle->instanceh[i].ptsize == points 
          && handle->instanceh[i].smooth == smooth) {
        mm_log((1,"i_tt_get_instance: in cache returning %d\n",i));
        return i;
      }
    }
  }
  
  /* Found the instance in the cache - return the cache index */
  
  for(idx=0;idx<TT_CHC;idx++) {
    if (!(handle->instanceh[idx].order)) break; /* find the lru item */
  }

  mm_log((1,"i_tt_get_instance: lru item is %d\n",idx));
  mm_log((1,"i_tt_get_instance: lru pointer 0x%X\n",
          USTRCT(handle->instanceh[idx].instance) ));
  
  if ( USTRCT(handle->instanceh[idx].instance) ) {
    mm_log((1,"i_tt_get_instance: freeing lru item from cache %d\n",idx));

    /* Free cached glyphs */
    for(i=0;i<256;i++)
      if ( USTRCT(handle->instanceh[idx].glyphs[i].glyph) )
	TT_Done_Glyph( handle->instanceh[idx].glyphs[i].glyph );

    for(i=0;i<256;i++) {
      handle->instanceh[idx].glyphs[i].ch = TT_NOCHAR;
      USTRCT(handle->instanceh[idx].glyphs[i].glyph)=NULL;
    }

    /* Free instance if needed */
    TT_Done_Instance( handle->instanceh[idx].instance );
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

  /* Zero the memory for the glyph storage so they are not thought as
     cached if they haven't been cached since this new font was loaded */

  for(i=0;i<256;i++) {
    handle->instanceh[idx].glyphs[i].ch = TT_NOCHAR;
    USTRCT(handle->instanceh[idx].glyphs[i].glyph)=NULL;
  }
  
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
i_tt_new(const char *fontname) {
  TT_Error error;
  TT_Fonthandle *handle;
  unsigned short i,n;
  unsigned short platform,encoding;

  i_clear_error();
  
  mm_log((1,"i_tt_new(fontname '%s')\n",fontname));
  
  /* allocate memory for the structure */
  
  handle = mymalloc( sizeof(TT_Fonthandle) ); /* checked 5Nov05 tonyc */

  /* load the typeface */
  error = TT_Open_Face( engine, fontname, &handle->face );
  if ( error ) {
    if ( error == TT_Err_Could_Not_Open_File ) {
      mm_log((1, "Could not find/open %s.\n", fontname ));
    }
    else {
      mm_log((1, "Error while opening %s, error code = 0x%x.\n",fontname, 
              error )); 
    }
    i_tt_push_error(error);
    return NULL;
  }
  
  TT_Get_Face_Properties( handle->face, &(handle->properties) );

  /* First, look for a Unicode charmap */
  n = handle->properties.num_CharMaps;
  USTRCT( handle->char_map )=NULL; /* Invalidate character map */
  
  for ( i = 0; i < n; i++ ) {
    TT_Get_CharMap_ID( handle->face, i, &platform, &encoding );
    if ( (platform == 3 && encoding == 1 ) 
         || (platform == 0 && encoding == 0 ) ) {
      mm_log((2,"i_tt_new - found char map platform %u encoding %u\n", 
              platform, encoding));
      TT_Get_CharMap( handle->face, i, &(handle->char_map) );
      break;
    }
  }
  if (!USTRCT(handle->char_map) && n != 0) {
    /* just use the first one */
    TT_Get_CharMap( handle->face, 0, &(handle->char_map));
  }

  /* Zero the pointsizes - and ordering */
  
  for(i=0;i<TT_CHC;i++) {
    USTRCT(handle->instanceh[i].instance)=NULL;
    handle->instanceh[i].order=i;
    handle->instanceh[i].ptsize=0;
    handle->instanceh[i].smooth=-1;
  }

#ifdef FTXPOST
  handle->loaded_names = 0;
#endif

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

  /* rows can be 0 for some glyphs, for example ' ' */
  if (bit->rows && bit->size / bit->rows != bit->cols) {
    i_fatal(0, "Integer overflow calculating bitmap size (%d, %d)\n",
            bit->width, bit->rows);
  }
  
  mm_log((1,"i_tt_init_raster_map: bit->width %d, bit->cols %d, bit->rows %d, bit->size %d)\n", bit->width, bit->cols, bit->rows, bit->size ));

  bit->bitmap = (void *) mymalloc( bit->size ); /* checked 6Nov05 tonyc */
  if ( !bit->bitmap ) i_fatal(0,"Not enough memory to allocate bitmap (%d)!\n",bit->size );
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
i_tt_get_glyph( TT_Fonthandle *handle, int inst, unsigned long j) {
  unsigned short load_flags, code;
  TT_Error error;

  mm_log((1, "i_tt_get_glyph(handle 0x%X, inst %d, j %d (%c))\n",
          handle,inst,j, ((j >= ' ' && j <= '~') ? j : '.')));
  
  /*mm_log((1, "handle->instanceh[inst].glyphs[j]=0x%08X\n",handle->instanceh[inst].glyphs[j] ));*/

  if ( TT_VALID(handle->instanceh[inst].glyphs[TT_HASH(j)].glyph)
       && handle->instanceh[inst].glyphs[TT_HASH(j)].ch == j) {
    mm_log((1,"i_tt_get_glyph: %d in cache\n",j));
    return 1;
  }

  if ( TT_VALID(handle->instanceh[inst].glyphs[TT_HASH(j)].glyph) ) {
    /* clean up the entry */
    TT_Done_Glyph( handle->instanceh[inst].glyphs[TT_HASH(j)].glyph );
    USTRCT( handle->instanceh[inst].glyphs[TT_HASH(j)].glyph ) = NULL;
    handle->instanceh[inst].glyphs[TT_HASH(j)].ch = TT_NOCHAR;
  }
  
  /* Ok - it wasn't cached - try to get it in */
  load_flags = TTLOAD_SCALE_GLYPH;
  if ( LTT_hinted ) load_flags |= TTLOAD_HINT_GLYPH;
  
  if ( !TT_VALID(handle->char_map) ) {
    code = (j - ' ' + 1) < 0 ? 0 : (j - ' ' + 1);
    if ( code >= handle->properties.num_Glyphs ) code = 0;
  } else code = TT_Char_Index( handle->char_map, j );
  
  if ( (error = TT_New_Glyph( handle->face, &handle->instanceh[inst].glyphs[TT_HASH(j)].glyph)) ) {
    mm_log((1, "Cannot allocate and load glyph: error 0x%x.\n", error ));
    i_push_error(error, "TT_New_Glyph()");
    return 0;
  }
  if ( (error = TT_Load_Glyph( handle->instanceh[inst].instance, handle->instanceh[inst].glyphs[TT_HASH(j)].glyph, code, load_flags)) ) {
    mm_log((1, "Cannot allocate and load glyph: error 0x%x.\n", error ));
    /* Don't leak */
    TT_Done_Glyph( handle->instanceh[inst].glyphs[TT_HASH(j)].glyph );
    USTRCT( handle->instanceh[inst].glyphs[TT_HASH(j)].glyph ) = NULL;
    i_push_error(error, "TT_Load_Glyph()");
    return 0;
  }

  /* At this point the glyph should be allocated and loaded */
  handle->instanceh[inst].glyphs[TT_HASH(j)].ch = j;

  /* Next get the glyph metrics */
  error = TT_Get_Glyph_Metrics( handle->instanceh[inst].glyphs[TT_HASH(j)].glyph, 
                                &handle->instanceh[inst].gmetrics[TT_HASH(j)] );
  if (error) {
    mm_log((1, "TT_Get_Glyph_Metrics: error 0x%x.\n", error ));
    TT_Done_Glyph( handle->instanceh[inst].glyphs[TT_HASH(j)].glyph );
    USTRCT( handle->instanceh[inst].glyphs[TT_HASH(j)].glyph ) = NULL;
    handle->instanceh[inst].glyphs[TT_HASH(j)].ch = TT_NOCHAR;
    i_push_error(error, "TT_Get_Glyph_Metrics()");
    return 0;
  }

  return 1;
}

/*
=item i_tt_has_chars(handle, text, len, utf8, out)

Check if the given characters are defined by the font.  Note that len
is the number of bytes, not the number of characters (when utf8 is
non-zero).

Returns the number of characters that were checked.

=cut
*/

int
i_tt_has_chars(TT_Fonthandle *handle, char const *text, int len, int utf8,
               char *out) {
  int count = 0;
  mm_log((1, "i_tt_has_chars(handle %p, text %p, len %d, utf8 %d)\n", 
          handle, text, len, utf8));

  while (len) {
    unsigned long c;
    int index;
    if (utf8) {
      c = i_utf8_advance(&text, &len);
      if (c == ~0UL) {
        i_push_error(0, "invalid UTF8 character");
        return 0;
      }
    }
    else {
      c = (unsigned char)*text++;
      --len;
    }
    
    if (TT_VALID(handle->char_map)) {
      index = TT_Char_Index(handle->char_map, c);
    }
    else {
      index = (c - ' ' + 1) < 0 ? 0 : (c - ' ' + 1);
      if (index >= handle->properties.num_Glyphs)
        index = 0;
    }
    *out++ = index != 0;
    ++count;
  }

  return count;
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
int
i_tt_render_all_glyphs( TT_Fonthandle *handle, int inst, TT_Raster_Map *bit,
                        TT_Raster_Map *small_bit, int cords[6], 
                        char const* txt, int len, int smooth, int utf8 ) {
  unsigned long j;
  TT_F26Dot6 x,y;
  
  mm_log((1,"i_tt_render_all_glyphs( handle 0x%X, inst %d, bit 0x%X, small_bit 0x%X, txt '%.*s', len %d, smooth %d, utf8 %d)\n",
	  handle, inst, bit, small_bit, len, txt, len, smooth, utf8));
  
  /* 
     y=-( handle->properties.horizontal->Descender * handle->instanceh[inst].imetrics.y_ppem )/(handle->properties.header->Units_Per_EM);
  */

  x=-cords[0]; /* FIXME: If you font is antialiased this should be expanded by one to allow for aa expansion and the allocation too - do before passing here */
  y=-cords[4];
  
  while (len) {
    if (utf8) {
      j = i_utf8_advance(&txt, &len);
      if (j == ~0UL) {
        i_push_error(0, "invalid UTF8 character");
        return 0;
      }
    }
    else {
      j = (unsigned char)*txt++;
      --len;
    }
    if ( !i_tt_get_glyph(handle,inst,j) ) 
      continue;
    i_tt_render_glyph( handle->instanceh[inst].glyphs[TT_HASH(j)].glyph, 
                       &handle->instanceh[inst].gmetrics[TT_HASH(j)], bit, 
                       small_bit, x, y, smooth );
    x += handle->instanceh[inst].gmetrics[TT_HASH(j)].advance / 64;
  }

  return 1;
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
i_tt_dump_raster_map2( i_img* im, TT_Raster_Map* bit, int xb, int yb, const i_color *cl, int smooth ) {
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
i_tt_rasterize( TT_Fonthandle *handle, TT_Raster_Map *bit, int cords[6], float points, char const* txt, int len, int smooth, int utf8 ) {
  int inst;
  int width, height;
  TT_Raster_Map small_bit;
  
  /* find or install an instance */
  if ( (inst=i_tt_get_instance(handle,points,smooth)) < 0) { 
    mm_log((1,"i_tt_rasterize: get instance failed\n"));
    return 0;
  }
  
  /* calculate bounding box */
  if (!i_tt_bbox_inst( handle, inst, txt, len, cords, utf8 ))
    return 0;
    
  
  width  = cords[2]-cords[0];
  height = cords[5]-cords[4];
  
  mm_log((1,"i_tt_rasterize: width=%d, height=%d\n",width, height )); 
  
  i_tt_init_raster_map ( bit, width, height, smooth );
  i_tt_clear_raster_map( bit );
  if ( smooth ) i_tt_init_raster_map( &small_bit, handle->instanceh[inst].imetrics.x_ppem + 32, height, smooth );
  
  if (!i_tt_render_all_glyphs( handle, inst, bit, &small_bit, cords, txt, len, 
                               smooth, utf8 )) {
    if ( smooth ) 
      i_tt_done_raster_map( &small_bit );
    return 0;
  }

  if ( smooth ) i_tt_done_raster_map( &small_bit );
  return 1;
}



/* 
 * Exported text rendering interfaces
 */


/*
=item i_tt_cp(handle, im, xb, yb, channel, points, txt, len, smooth, utf8)

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
i_tt_cp( TT_Fonthandle *handle, i_img *im, int xb, int yb, int channel, float points, char const* txt, int len, int smooth, int utf8, int align ) {

  int cords[BOUNDING_BOX_COUNT];
  int ascent, st_offset, y;
  TT_Raster_Map bit;
  
  i_clear_error();
  if (! i_tt_rasterize( handle, &bit, cords, points, txt, len, smooth, utf8 ) ) return 0;
  
  ascent=cords[BBOX_ASCENT];
  st_offset=cords[BBOX_NEG_WIDTH];
  y = align ? yb-ascent : yb;

  i_tt_dump_raster_map_channel( im, &bit, xb-st_offset , y, channel, smooth );
  i_tt_done_raster_map( &bit );

  return 1;
}


/* 
=item i_tt_text(handle, im, xb, yb, cl, points, txt, len, smooth, utf8) 

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
i_tt_text( TT_Fonthandle *handle, i_img *im, int xb, int yb, const i_color *cl, float points, char const* txt, int len, int smooth, int utf8, int align) {
  int cords[BOUNDING_BOX_COUNT];
  int ascent, st_offset, y;
  TT_Raster_Map bit;

  i_clear_error();
  
  if (! i_tt_rasterize( handle, &bit, cords, points, txt, len, smooth, utf8 ) ) return 0;
  
  ascent=cords[BBOX_ASCENT];
  st_offset=cords[BBOX_NEG_WIDTH];
  y = align ? yb-ascent : yb;

  i_tt_dump_raster_map2( im, &bit, xb+st_offset, y, cl, smooth ); 
  i_tt_done_raster_map( &bit );

  return 1;
}


/*
=item i_tt_bbox_inst(handle, inst, txt, len, cords, utf8) 

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
i_tt_bbox_inst( TT_Fonthandle *handle, int inst ,const char *txt, int len, int cords[BOUNDING_BOX_COUNT], int utf8 ) {
  int upm, casc, cdesc, first;
  
  int start    = 0;
  int width    = 0;
  int gdescent = 0;
  int gascent  = 0;
  int descent  = 0;
  int ascent   = 0;
  int rightb   = 0;

  unsigned long j;
  unsigned char *ustr;
  ustr=(unsigned char*)txt;

  mm_log((1,"i_tt_box_inst(handle 0x%X,inst %d,txt '%.*s', len %d, utf8 %d)\n",handle,inst,len,txt,len, utf8));

  upm     = handle->properties.header->Units_Per_EM;
  gascent  = ( handle->properties.horizontal->Ascender  * handle->instanceh[inst].imetrics.y_ppem + upm - 1) / upm;
  gdescent = ( handle->properties.horizontal->Descender * handle->instanceh[inst].imetrics.y_ppem - upm + 1) / upm;
  
  width   = 0;
  start   = 0;
  
  mm_log((1, "i_tt_box_inst: gascent=%d gdescent=%d\n", gascent, gdescent));

  first=1;
  while (len) {
    if (utf8) {
      j = i_utf8_advance(&txt, &len);
      if (j == ~0UL) {
        i_push_error(0, "invalid UTF8 character");
        return 0;
      }
    }
    else {
      j = (unsigned char)*txt++;
      --len;
    }
    if ( i_tt_get_glyph(handle,inst,j) ) {
      TT_Glyph_Metrics *gm = handle->instanceh[inst].gmetrics + TT_HASH(j);
      width += gm->advance   / 64;
      casc   = (gm->bbox.yMax+63) / 64;
      cdesc  = (gm->bbox.yMin-63) / 64;

      mm_log((1, "i_tt_box_inst: glyph='%c' casc=%d cdesc=%d\n", 
              ((j >= ' ' && j <= '~') ? j : '.'), casc, cdesc));

      if (first) {
	start    = gm->bbox.xMin / 64;
	ascent   = (gm->bbox.yMax+63) / 64;
	descent  = (gm->bbox.yMin-63) / 64;
	first = 0;
      }
      if (!len) { /* if at end of string */
	/* the right-side bearing - in case the right-side of a 
	   character goes past the right of the advance width,
	   as is common for italic fonts
	*/
	rightb = gm->advance - gm->bearingX 
	  - (gm->bbox.xMax - gm->bbox.xMin);
	/* fprintf(stderr, "font info last: %d %d %d %d\n", 
	   gm->bbox.xMax, gm->bbox.xMin, gm->advance, rightb); */
      }

      ascent  = (ascent  >  casc ?  ascent : casc );
      descent = (descent < cdesc ? descent : cdesc);
    }
  }
  
  cords[BBOX_NEG_WIDTH]=start;
  cords[BBOX_GLOBAL_DESCENT]=gdescent;
  cords[BBOX_POS_WIDTH]=width;
  if (rightb < 0)
    cords[BBOX_POS_WIDTH] -= rightb / 64;
  cords[BBOX_GLOBAL_ASCENT]=gascent;
  cords[BBOX_DESCENT]=descent;
  cords[BBOX_ASCENT]=ascent;
  cords[BBOX_ADVANCE_WIDTH] = width;
  cords[BBOX_RIGHT_BEARING] = rightb / 64;

  return BBOX_RIGHT_BEARING + 1;
}


/*
=item i_tt_bbox(handle, points, txt, len, cords, utf8)

Interface to get a strings bounding box

   handle - pointer to font handle
   points - font size to use
   txt    - string to render
   len    - length of the string to render
   cords  - the bounding box (modified in place)

=cut
*/

undef_int
i_tt_bbox( TT_Fonthandle *handle, float points,const char *txt,int len,int cords[6], int utf8) {
  int inst;

  i_clear_error();
  mm_log((1,"i_tt_box(handle 0x%X,points %f,txt '%.*s', len %d, utf8 %d)\n",handle,points,len,txt,len, utf8));

  if ( (inst=i_tt_get_instance(handle,points,-1)) < 0) {
    i_push_errorf(0, "i_tt_get_instance(%g)", points);
    mm_log((1,"i_tt_text: get instance failed\n"));
    return 0;
  }

  return i_tt_bbox_inst(handle, inst, txt, len, cords, utf8);
}

/*
=item i_tt_face_name(handle, name_buf, name_buf_size)

Retrieve's the font's postscript name.

This is complicated by the need to handle encodings and so on.

=cut
 */
int
i_tt_face_name(TT_Fonthandle *handle, char *name_buf, size_t name_buf_size) {
  TT_Face_Properties props;
  int name_count;
  int i;
  TT_UShort platform_id, encoding_id, lang_id, name_id;
  TT_UShort name_len;
  TT_String *name;
  int want_index = -1; /* an acceptable but not perfect name */
  int score = 0;

  i_clear_error();
  
  TT_Get_Face_Properties(handle->face, &props);
  name_count = props.num_Names;
  for (i = 0; i < name_count; ++i) {
    TT_Get_Name_ID(handle->face, i, &platform_id, &encoding_id, &lang_id, 
                   &name_id);

    TT_Get_Name_String(handle->face, i, &name, &name_len);

    if (platform_id != TT_PLATFORM_APPLE_UNICODE && name_len
        && name_id == TT_NAME_ID_PS_NAME) {
      int might_want_index = -1;
      int might_score = 0;
      if ((platform_id == TT_PLATFORM_MACINTOSH && encoding_id == TT_MAC_ID_ROMAN)
          ||
          (platform_id == TT_PLATFORM_MICROSOFT && encoding_id == TT_MS_LANGID_ENGLISH_UNITED_STATES)) {
        /* exactly what we want */
        want_index = i;
        break;
      }
      
      if (platform_id == TT_PLATFORM_MICROSOFT
          && (encoding_id & 0xFF) == TT_MS_LANGID_ENGLISH_GENERAL) {
        /* any english is good */
        might_want_index = i;
        might_score = 9;
      }
      /* there might be something in between */
      else {
        /* anything non-unicode is better than nothing */
        might_want_index = i;
        might_score = 1;
      }
      if (might_score > score) {
        score = might_score;
        want_index = might_want_index;
      }
    }
  }

  if (want_index != -1) {
    TT_Get_Name_String(handle->face, want_index, &name, &name_len);
    
    strncpy(name_buf, name, name_buf_size);
    name_buf[name_buf_size-1] = '\0';

    return strlen(name) + 1;
  }
  else {
    i_push_error(0, "no face name present");
    return 0;
  }
}

void i_tt_dump_names(TT_Fonthandle *handle) {
  TT_Face_Properties props;
  int name_count;
  int i;
  TT_UShort platform_id, encoding_id, lang_id, name_id;
  TT_UShort name_len;
  TT_String *name;
  
  TT_Get_Face_Properties(handle->face, &props);
  name_count = props.num_Names;
  for (i = 0; i < name_count; ++i) {
    TT_Get_Name_ID(handle->face, i, &platform_id, &encoding_id, &lang_id, 
                   &name_id);
    TT_Get_Name_String(handle->face, i, &name, &name_len);

    printf("# %d: plat %d enc %d lang %d name %d value ", i, platform_id,
           encoding_id, lang_id, name_id);
    if (platform_id == TT_PLATFORM_APPLE_UNICODE) {
      printf("(unicode)\n");
    }
    else {
      printf("'%s'\n", name);
    }
  }
}

int
i_tt_glyph_name(TT_Fonthandle *handle, unsigned long ch, char *name_buf, 
                 size_t name_buf_size) {
#ifdef FTXPOST
  TT_Error rc;
  TT_String *psname;
  TT_UShort index;

  i_clear_error();

  if (!handle->loaded_names) {
    TT_Post post;
    mm_log((1, "Loading PS Names"));
    handle->load_cond = TT_Load_PS_Names(handle->face, &post);
    ++handle->loaded_names;
  }

  if (handle->load_cond) {
    i_push_errorf(rc, "error loading names (%d)", handle->load_cond);
    return 0;
  }
  
  index = TT_Char_Index(handle->char_map, ch);
  if (!index) {
    i_push_error(0, "no such character");
    return 0;
  }

  rc = TT_Get_PS_Name(handle->face, index, &psname);

  if (rc) {
    i_push_error(rc, "error getting name");
    return 0;
  }

  strncpy(name_buf, psname, name_buf_size);
  name_buf[name_buf_size-1] = '\0';

  return strlen(psname) + 1;
#else
  mm_log((1, "FTXPOST extension not enabled\n"));
  i_clear_error();
  i_push_error(0, "Use of FTXPOST extension disabled");

  return 0;
#endif
}

/*
=item i_tt_push_error(code)

Push an error message and code onto the Imager error stack.

=cut
*/
static void
i_tt_push_error(TT_Error rc) {
#ifdef FTXERR18
  TT_String const *msg = TT_ErrToString18(rc);

  i_push_error(rc, msg);
#else
  i_push_errorf(rc, "Error code 0x%04x", (unsigned)rc);
#endif
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
