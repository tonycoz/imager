#ifndef IMBIFF_H_
#define IMBIFF_H_

#include <stddef.h>

typedef struct {
  /* width and height of the  glyph */
  int width;
  int height;

  /* distance from the top of the glyph to the baseline, negative if the 
     char is below the baseline */
  int baseline;

  /* distance from the draw point */
  int offset;
  int advance;
  const unsigned char *data;
  size_t data_size;
} i_bif_glyph;

typedef struct {
  int ch;
  const i_bif_glyph *glyph;
} i_bif_mapping;

typedef struct {
  /* scale height in pixels - bottom of descender to top of ascender */
  int size;

  int ascent;

  int descent;

  int xwidth;

  const i_bif_glyph *default_glyph;

  const i_bif_mapping *chars;
  size_t char_count;
} i_bif_font;

typedef struct {
  const char *name;

  const i_bif_font * const *fonts;
  size_t font_count;
} i_bif_face;

#endif
