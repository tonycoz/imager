#ifndef IMBIFF_H_
#define IMBIFF_H_

typedef struct {
  /* width and height of the  glyph */
  int width;
  int height;

  /* distance from the top of the glyph to the baseline, negative if the 
     char is below the baseline */
  int baseline;

  /* distance from the draw point 
  int offset;
  int advance;
  unsigned char *data;
  size_t data_size;
} i_bif_glyph;

typedef struct {
  int ch;
  i_bif_glyph *glyph;
} i_bif_mapping;

typedef struct {
  /* scale height in pixels - bottom of descender to top of ascender */
  int size;

  int ascent;

  int descent;

  int xwidth;

  i_bif_mapping *chars;
  size_t char_count;
} i_bif_font;

typedef struct {
  const char *name;

  i_bif_font *fonts;
  size_t font_count;
} i_bif_face;

#endif
