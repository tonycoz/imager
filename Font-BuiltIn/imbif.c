#include "imbif.h"
#include <string.h>
#include "imext.h"
#include "tiny.h"
#include <math.h>

struct i_bif_handle_tag {
  i_bif_face const *face;
};

static i_bif_face const *faces[] = {
  &i_face_tiny
};

static int face_count = sizeof(faces) / sizeof(*faces);

i_bif_handle *
i_bif_new(const char *name) {
  i_bif_handle *font;
  const i_bif_face *found_face = NULL;
  int i;

  i_clear_error();
  if (!name) {
    i_push_error(0, "No face specified");
    return NULL;
  }

  for (i = 0; i < face_count; ++i) {
    i_bif_face const * face = faces[i];
    if (stricmp(face->name, name) == 0) {
      found_face = face;
      break;
    }
  }

  if (!found_face) {
    i_push_errorf(0, "Face %s not found", name);
    return NULL;
  }

  font = mymalloc(sizeof(i_bif_handle));
  font->face = found_face;

  return font;
}

void
i_bif_destroy(i_bif_handle *font) {
  myfree(font);
}

static
const i_bif_font *
_find_font(i_bif_handle *handle, int size) {
  i_bif_face const *face = handle->face;
  int i;
  for (i = 0; i < face->font_count; ++i) {
    if (face->fonts[i]->size >= size) {
      return face->fonts[i];
    }
  }

  return NULL;
}

static
const i_bif_glyph *
_find_glyph(i_bif_font const *font, int code) {
  /* linear for now */
  int i = 0;
  while (i < font->char_count) {
    if (font->chars[i].ch == code)
      return font->chars[i].glyph;
    ++i;
  }

  return font->default_glyph;
}

int 
i_bif_bbox(i_bif_handle *handle, double size, char const *text, int len, int utf8, int *bbox) {
  int isize = ceil(size);
  i_bif_font const * font = _find_font(handle, isize);
  int width = 0;
  int advance = 0;
  int ascent = 0;
  int descent = 0;
  int neg_width = 0;
  int rightb = 0;
  int i;

  if (!font) {
    i_push_errorf(0, "No size %d font found", isize);
    return 0;
  }

  if (len) {
    const char *p = text;
    const i_bif_glyph *glyph;
    
    while (len) {
      unsigned long c;
      if (utf8) {
	c = i_utf8_advance(&p, &len);
	if (c == ~0UL) {
	  i_push_error(0, "invalid UTF8 character");
	  return 0;
	}
      }
      else {
	c = (unsigned char)*text++;
	--len;
      }

      glyph = _find_glyph(font, c);
      advance += glyph->advance;
      if (glyph->baseline > ascent)
	ascent = glyph->baseline;
      if (glyph->baseline - glyph->height > descent)
	descent = glyph->baseline - glyph->height;
      neg_width = glyph->offset;
    }
    
    /* get rightb from the last char processed */
    rightb = glyph->advance - glyph->width - glyph->offset;
  }

  bbox[BBOX_GLOBAL_DESCENT] = font->descent;
  bbox[BBOX_GLOBAL_ASCENT] = font->ascent;
  bbox[BBOX_ADVANCE_WIDTH] = advance;
  bbox[BBOX_NEG_WIDTH] = neg_width;
  bbox[BBOX_POS_WIDTH] = width - (rightb < 0 ? rightb : 0);
  bbox[BBOX_DESCENT] = descent;
  bbox[BBOX_ASCENT] = ascent;
  bbox[BBOX_RIGHT_BEARING] = rightb;

  return BBOX_RIGHT_BEARING + 1;
}

const int dir_cos[4] = { 1, 0, -1, 0 };
const int dir_sin[4] = { 0, 1, 0, -1 };

int 
i_bif_text(i_bif_handle *handle, i_img *im, int tx, int ty, double size, i_color *cl, const char *text, int len, int align, int utf8, int dir) {
  int isize = ceil(size);
  i_bif_font const * font = _find_font(handle, isize);
  int dcos, dsin;

  if (!font) {
    i_push_errorf(0, "No size %d font found", isize);
    return 0;
  }

  dir = dir & 3;
  dcos = dir_cos[dir];
  dsin = dir_sin[dir];

  if (len) {
    const char *p = text;
    while (len) {
      const i_bif_glyph *glyph;
      unsigned long c;
      int px, py;
      int outx, outy;
      int width_bytes;
      unsigned char *gbyte;

      if (utf8) {
	c = i_utf8_advance(&p, &len);
	if (c == ~0UL) {
	  i_push_error(0, "invalid UTF8 character");
	  return 0;
	}
      }
      else {
	c = (unsigned char)*text++;
	--len;
      }

      glyph = _find_glyph(font, c);
      width_bytes = (glyph->width + 7) / 8;
      
      outy = ty - glyph->baseline;
      gbyte = glyph->data;
      for (py = 0; py < glyph->height; ++py) {
	if (outy >= 0 && outy < im->ysize) {
	  outx = tx;
	  for (px = 0; px < width_bytes; ++px) {
	    unsigned char b = *gbyte++;
	    if (b) {
	      unsigned char mask = 0x80;
	      while (mask) {
		if (b & mask)
		  i_ppix(im, outx, outy, cl);
		++outx;
		mask >>= 1;
	      }
	    }
	    else {
	      outx += 8;
	    }
	  }
	}
	else
	  gbyte += width_bytes;
	++outy;
      }

      tx += glyph->advance;
    }
  }

  return 1;
}

extern int i_bif_has_chars(i_bif_handle *handle, char const *test, int len, int utf8, char *out);
extern int i_bif_face_name(i_bif_handle *handle, char *name_buf, size_t name_buf_size);

