/*
=head1 NAME

freetyp2.c - font support via the FreeType library version 2.

=head1 SYNOPSIS

  if (!i_ft2_init()) { error }
  FT2_Fonthandle *font;
  font = i_ft2_new(name, index);
  if (!i_ft2_setdpi(font, xdpi, ydpi)) { error }
  if (!i_ft2_getdpi(font, &xdpi, &ydpi)) { error }
  double matrix[6];
  if (!i_ft2_settransform(font, matrix)) { error }
  int bbox[6];
  if (!i_ft2_bbox(font, cheight, cwidth, text, length, bbox, utf8)) { error }
  i_img *im = ...;
  i_color cl;
  if (!i_ft2_text(font, im, tx, ty, cl, cheight, cwidth, text, length, align,
                  aa)) { error }
  if (!i_ft2_cp(font, im, tx, ty, channel, cheight, cwidth, text, length,
                align, aa)) { error }
  i_ft2_destroy(font);

=head1 DESCRIPTION

Implements Imager font support using the FreeType2 library.

The FreeType2 library understands several font file types, including
Truetype, Type1 and Windows FNT.

=over 

=cut
*/

#include "image.h"
#include <stdio.h>
#include <ft2build.h>
#include FT_FREETYPE_H

static void ft2_push_message(int code);
static unsigned long utf8_advance(char **p, int *len);

static FT_Library library;

/*
=item i_ft2_init(void)

Initializes the Freetype 2 library.

Returns true on success, false on failure.

=cut
*/
int
i_ft2_init(void) {
  FT_Error error;

  i_clear_error();
  error = FT_Init_FreeType(&library);
  if (error) {
    ft2_push_message(error);
    i_push_error(0, "Initializing Freetype2");
    return 0;
  }
  return 1;
}

struct FT2_Fonthandle {
  FT_Face face;
  int xdpi, ydpi;
  int hint;
  FT_Encoding encoding;

  /* used to adjust so we can align the draw point to the top-left */
  double matrix[6];
};

/* the following is used to select a "best" encoding */
static struct enc_score {
  FT_Encoding encoding;
  int score;
} enc_scores[] =
{
  /* the selections here are fairly arbitrary
     ideally we need to give the user a list of encodings available
     and a mechanism to choose one */
  { ft_encoding_unicode,        10 },
  { ft_encoding_sjis,            8 },
  { ft_encoding_gb2312,          8 },
  { ft_encoding_big5,            8 },
  { ft_encoding_wansung,         8 },
  { ft_encoding_johab,           8 },  
  { ft_encoding_latin_2,         6 },
  { ft_encoding_apple_roman,     6 },
  { ft_encoding_adobe_standard,  6 },
  { ft_encoding_adobe_expert,    6 },
};

/*
=item i_ft2_new(char *name, int index)

Creates a new font object, from the file given by I<name>.  I<index>
is the index of the font in a file with multiple fonts, where 0 is the
first font.

Return NULL on failure.

=cut
*/

FT2_Fonthandle *
i_ft2_new(char *name, int index) {
  FT_Error error;
  FT2_Fonthandle *result;
  FT_Face face;
  double matrix[6] = { 1, 0, 0,
                       0, 1, 0 };
  int i, j;
  FT_Encoding encoding;
  int score;

  mm_log((1, "i_ft2_new(name %p, index %d)\n", name, index));

  i_clear_error();
  error = FT_New_Face(library, name, index, &face);
  if (error) {
    ft2_push_message(error);
    i_push_error(error, "Opening face");
    return NULL;
  }

  encoding = face->num_charmaps ? face->charmaps[0]->encoding : ft_encoding_unicode;
  score = 0;
  for (i = 0; i < face->num_charmaps; ++i) {
    FT_Encoding enc_entry = face->charmaps[i]->encoding;
    mm_log((2, "i_ft2_new, encoding %lX platform %u encoding %u\n",
            enc_entry, face->charmaps[i]->platform_id,
            face->charmaps[i]->encoding_id));
    for (j = 0; j < sizeof(enc_scores) / sizeof(*enc_scores); ++j) {
      if (enc_scores[j].encoding == enc_entry && enc_scores[j].score > score) {
        encoding = enc_entry;
        score = enc_scores[j].score;
        break;
      }
    }
  }
  FT_Select_Charmap(face, encoding);
  mm_log((2, "i_ft2_new, selected encoding %lX\n", encoding));

  result = mymalloc(sizeof(FT2_Fonthandle));
  result->face = face;
  result->xdpi = result->ydpi = 72;
  result->encoding = encoding;

  /* by default we disable hinting on a call to i_ft2_settransform()
     if we don't do this, then the hinting can the untransformed text
     to be a different size to the transformed text.
     Obviously we have it initially enabled.
  */
  result->hint = 1; 

  /* I originally forgot this:   :/ */
  /*i_ft2_settransform(result, matrix); */
  result->matrix[0] = 1; result->matrix[1] = 0; result->matrix[2] = 0;
  result->matrix[3] = 0; result->matrix[4] = 1; result->matrix[5] = 0;

  return result;
}

/*
=item i_ft2_destroy(FT2_Fonthandle *handle)

Destroys a font object, which must have been the return value of
i_ft2_new().

=cut
*/
void
i_ft2_destroy(FT2_Fonthandle *handle) {
  FT_Done_Face(handle->face);
  myfree(handle);
}

/*
=item i_ft2_setdpi(FT2_Fonthandle *handle, int xdpi, int ydpi)

Sets the resolution in dots per inch at which point sizes scaled, by
default xdpi and ydpi are 72, so that 1 point maps to 1 pixel.

Both xdpi and ydpi should be positive.

Return true on success.

=cut
*/
int
i_ft2_setdpi(FT2_Fonthandle *handle, int xdpi, int ydpi) {
  i_clear_error();
  if (xdpi > 0 && ydpi > 0) {
    handle->xdpi = xdpi;
    handle->ydpi = ydpi;
    return 0;
  }
  else {
    i_push_error(0, "resolutions must be positive");
    return 0;
  }
}

/*
=item i_ft2_getdpi(FT2_Fonthandle *handle, int *xdpi, int *ydpi)

Retrieves the current horizontal and vertical resolutions at which
point sizes are scaled.

=cut
*/
int
i_ft2_getdpi(FT2_Fonthandle *handle, int *xdpi, int *ydpi) {
  *xdpi = handle->xdpi;
  *ydpi = handle->ydpi;

  return 1;
}

/*
=item i_ft2_settransform(FT2_FontHandle *handle, double *matrix)

Sets a transormation matrix for output.

This should be a 2 x 3 matrix like:

 matrix[0]   matrix[1]   matrix[2]
 matrix[3]   matrix[4]   matrix[5]

=cut
*/
int
i_ft2_settransform(FT2_Fonthandle *handle, double *matrix) {
  FT_Matrix m;
  FT_Vector v;
  int i;

  m.xx = matrix[0] * 65536;
  m.xy = matrix[1] * 65536;
  v.x  = matrix[2]; /* this could be pels of 26.6 fixed - not sure */
  m.yx = matrix[3] * 65536;
  m.yy = matrix[4] * 65536;
  v.y  = matrix[5]; /* see just above */

  FT_Set_Transform(handle->face, &m, &v);

  for (i = 0; i < 6; ++i)
    handle->matrix[i] = matrix[i];
  handle->hint = 0;

  return 1;
}

/*
=item i_ft2_sethinting(FT2_Fonthandle *handle, int hinting)

If hinting is non-zero then glyph hinting is enabled, otherwise disabled.

i_ft2_settransform() disables hinting to prevent distortions in
gradual text transformations.

=cut
*/
int i_ft2_sethinting(FT2_Fonthandle *handle, int hinting) {
  handle->hint = hinting;
  return 1;
}

/*
=item i_ft2_bbox(FT2_Fonthandle *handle, double cheight, double cwidth, char *text, int len, int *bbox)

Retrieves bounding box information for the font at the given 
character width and height.  This ignores the transformation matrix.

Returns non-zero on success.

=cut
*/
int
i_ft2_bbox(FT2_Fonthandle *handle, double cheight, double cwidth, 
           char *text, int len, int *bbox, int utf8) {
  FT_Error error;
  int width;
  int index;
  int first;
  int ascent = 0, descent = 0;
  int glyph_ascent, glyph_descent;
  FT_Glyph_Metrics *gm;
  int start = 0;

  mm_log((1, "i_ft2_bbox(handle %p, cheight %f, cwidth %f, text %p, len %d, bbox %p)\n",
	  handle, cheight, cwidth, text, len, bbox));

  error = FT_Set_Char_Size(handle->face, cwidth*64, cheight*64, 
                           handle->xdpi, handle->ydpi);
  if (error) {
    ft2_push_message(error);
    i_push_error(0, "setting size");
  }

  first = 1;
  width = 0;
  while (len) {
    unsigned long c;
    if (utf8) {
      c = utf8_advance(&text, &len);
      if (c == ~0UL) {
        i_push_error(0, "invalid UTF8 character");
        return 0;
      }
    }
    else {
      c = (unsigned char)*text++;
      --len;
    }

    index = FT_Get_Char_Index(handle->face, c);
    error = FT_Load_Glyph(handle->face, index, FT_LOAD_DEFAULT);
    if (error) {
      ft2_push_message(error);
      i_push_errorf(0, "loading glyph for character \\x%02x (glyph 0x%04X)", 
                    c, index);
      return 0;
    }
    gm = &handle->face->glyph->metrics;
    glyph_ascent = gm->horiBearingY / 64;
    glyph_descent = glyph_ascent - gm->height/64;
    if (first) {
      start = gm->horiBearingX / 64;
      /* handles -ve values properly */
      ascent = glyph_ascent;
      descent = glyph_descent;
      first = 0;
    }

    if (glyph_ascent > ascent)
      ascent = glyph_ascent;
    if (glyph_descent < descent)
      descent = glyph_descent;

    width += gm->horiAdvance / 64;

    if (len == 0) {
      /* last character 
       handle the case where the right the of the character overlaps the 
       right*/
      int rightb = gm->horiAdvance - gm->horiBearingX - gm->width;
      if (rightb < 0)
        width -= rightb / 64;
    }
  }

  bbox[0] = start;
  bbox[1] = handle->face->size->metrics.descender / 64;
  bbox[2] = width;
  bbox[3] = handle->face->size->metrics.ascender / 64;
  bbox[4] = descent;
  bbox[5] = ascent;

  return 1;
}

/*
=item transform_box(FT2_FontHandle *handle, int bbox[4])

bbox contains coorinates of a the top-left and bottom-right of a bounding 
box relative to a point.

This is then transformed and the values in bbox[4] are the top-left
and bottom-right of the new bounding box.

This is meant to provide the bounding box of a transformed character
box.  The problem is that if the character was round and is rotated,
the real bounding box isn't going to be much different from the
original, but this function will return a _bigger_ bounding box.  I
suppose I could work my way through the glyph outline, but that's
too much hard work.

=cut
*/
void ft2_transform_box(FT2_Fonthandle *handle, int bbox[4]) {
  double work[8];
  double *matrix = handle->matrix;
  int i;
  
  work[0] = matrix[0] * bbox[0] + matrix[1] * bbox[1];
  work[1] = matrix[3] * bbox[0] + matrix[4] * bbox[1];
  work[2] = matrix[0] * bbox[2] + matrix[1] * bbox[1];
  work[3] = matrix[3] * bbox[2] + matrix[4] * bbox[1];
  work[4] = matrix[0] * bbox[0] + matrix[1] * bbox[3];
  work[5] = matrix[3] * bbox[0] + matrix[4] * bbox[3];
  work[6] = matrix[0] * bbox[2] + matrix[1] * bbox[3];
  work[7] = matrix[3] * bbox[2] + matrix[4] * bbox[3];

  bbox[0] = floor(i_min(i_min(work[0], work[2]),i_min(work[4], work[6])));
  bbox[1] = floor(i_min(i_min(work[1], work[3]),i_min(work[5], work[7])));
  bbox[2] = ceil(i_max(i_max(work[0], work[2]),i_max(work[4], work[6])));
  bbox[3] = ceil(i_max(i_max(work[1], work[3]),i_max(work[5], work[7])));
}

/*
=item expand_bounds(int bbox[4], int bbox2[4]) 

Treating bbox[] and bbox2[] as 2 bounding boxes, produces a new
bounding box in bbox[] that encloses both.

=cut
*/
static void expand_bounds(int bbox[4], int bbox2[4]) {
  bbox[0] = i_min(bbox[0], bbox2[0]);
  bbox[1] = i_min(bbox[1], bbox2[1]);
  bbox[2] = i_max(bbox[2], bbox2[2]);
  bbox[3] = i_max(bbox[3], bbox2[3]);
}

/*
=item i_ft2_bbox_r(FT2_Fonthandle *handle, double cheight, double cwidth, char *text, int len, int vlayout, int utf8, int *bbox)

Retrieves bounding box information for the font at the given 
character width and height.

This version finds the rectangular bounding box of the glyphs, with
the text as transformed by the transformation matrix.  As with
i_ft2_bbox (bbox[0], bbox[1]) will the the offset from the start of
the topline to the top-left of the bounding box.  Unlike i_ft2_bbox()
this could be near the bottom left corner of the box.

(bbox[4], bbox[5]) is the offset to the start of the baseline.
(bbox[6], bbox[7]) is the offset from the start of the baseline to the
end of the baseline.

Returns non-zero on success.

=cut
*/
int
i_ft2_bbox_r(FT2_Fonthandle *handle, double cheight, double cwidth, 
           char *text, int len, int vlayout, int utf8, int *bbox) {
  FT_Error error;
  int width;
  int index;
  int first;
  int ascent = 0, descent = 0;
  int glyph_ascent, glyph_descent;
  FT_Glyph_Metrics *gm;
  int start = 0;
  int work[4];
  int bounds[4];
  double x = 0, y = 0;
  int i;
  FT_GlyphSlot slot;
  int advx, advy;
  int loadFlags = FT_LOAD_DEFAULT;

  if (vlayout)
    loadFlags |= FT_LOAD_VERTICAL_LAYOUT;

  error = FT_Set_Char_Size(handle->face, cwidth*64, cheight*64, 
                           handle->xdpi, handle->ydpi);
  if (error) {
    ft2_push_message(error);
    i_push_error(0, "setting size");
  }

  first = 1;
  width = 0;
  while (len) {
    unsigned long c;
    if (utf8) {
      c = utf8_advance(&text, &len);
      if (c == ~0UL) {
        i_push_error(0, "invalid UTF8 character");
        return 0;
      }
    }
    else {
      c = (unsigned char)*text++;
      --len;
    }

    index = FT_Get_Char_Index(handle->face, c);
    error = FT_Load_Glyph(handle->face, index, loadFlags);
    if (error) {
      ft2_push_message(error);
      i_push_errorf(0, "loading glyph for character \\x%02x (glyph 0x%04X)", 
                    c, index);
      return 0;
    }
    slot = handle->face->glyph; 
    gm = &slot->metrics;

    /* these probably don't mean much for vertical layouts */
    glyph_ascent = gm->horiBearingY / 64;
    glyph_descent = glyph_ascent - gm->height/64;
    if (vlayout) {
      work[0] = gm->vertBearingX;
      work[1] = gm->vertBearingY;
    }
    else {
      work[0] = gm->horiBearingX;
      work[1] = gm->horiBearingY;
    }
    work[2] = gm->width  + work[0];
    work[3] = work[1] - gm->height;
    if (first) {
      bbox[4] = work[0] * handle->matrix[0] + work[1] * handle->matrix[1] + handle->matrix[2];
      bbox[5] = work[0] * handle->matrix[3] + work[1] * handle->matrix[4] + handle->matrix[5];
      bbox[4] = bbox[4] < 0 ? -(-bbox[4] + 32)/64 : (bbox[4] + 32) / 64;
      bbox[5] /= 64;
    }
    ft2_transform_box(handle, work);
    for (i = 0; i < 4; ++i)
      work[i] /= 64;
    work[0] += x;
    work[1] += y;
    work[2] += x;
    work[3] += y;
    if (first) {
      for (i = 0; i < 4; ++i)
        bounds[i] = work[i];
      ascent = glyph_ascent;
      descent = glyph_descent;
      first = 0;
    }
    else {
      expand_bounds(bounds, work);
    }
    x += slot->advance.x / 64;
    y += slot->advance.y / 64;
    
    if (glyph_ascent > ascent)
      ascent = glyph_ascent;
    if (glyph_descent > descent)
      descent = glyph_descent;

    if (len == 0) {
      /* last character 
       handle the case where the right the of the character overlaps the 
       right*/
      /*int rightb = gm->horiAdvance - gm->horiBearingX - gm->width;
      if (rightb < 0)
      width -= rightb / 64;*/
    }
  }

  /* at this point bounds contains the bounds relative to the CP,
     and x, y hold the final position relative to the CP */
  /*bounds[0] -= x;
  bounds[1] -= y;
  bounds[2] -= x;
  bounds[3] -= y;*/

  bbox[0] = bounds[0];
  bbox[1] = -bounds[3];
  bbox[2] = bounds[2];
  bbox[3] = -bounds[1];
  bbox[6] = x;
  bbox[7] = -y;

  return 1;
}



static int
make_bmp_map(FT_Bitmap *bitmap, unsigned char *map);

/*
=item i_ft2_text(FT2_Fonthandle *handle, i_img *im, int tx, int ty, i_color *cl, double cheight, double cwidth, char *text, int len, int align, int aa)

Renders I<text> to (I<tx>, I<ty>) in I<im> using color I<cl> at the given 
I<cheight> and I<cwidth>.

If align is 0, then the text is rendered with the top-left of the
first character at (I<tx>, I<ty>).  If align is non-zero then the text
is rendered with (I<tx>, I<ty>) aligned with the base-line of the
characters.

If aa is non-zero then the text is anti-aliased.

Returns non-zero on success.

=cut
*/
int
i_ft2_text(FT2_Fonthandle *handle, i_img *im, int tx, int ty, i_color *cl,
           double cheight, double cwidth, char *text, int len, int align,
           int aa, int vlayout, int utf8) {
  FT_Error error;
  int index;
  FT_Glyph_Metrics *gm;
  int bbox[6];
  FT_GlyphSlot slot;
  int x, y;
  unsigned char *bmp;
  unsigned char map[256];
  char last_mode = ft_pixel_mode_none; 
  int last_grays = -1;
  int ch;
  i_color pel;
  int loadFlags = FT_LOAD_DEFAULT;

  mm_log((1, "i_ft2_text(handle %p, im %p, tx %d, ty %d, cl %p, cheight %f, cwidth %f, text %p, len %d, align %d, aa %d)\n",
	  handle, im, tx, ty, cl, cheight, cwidth, text, align, aa));

  if (vlayout) {
    if (!FT_HAS_VERTICAL(handle->face)) {
      i_push_error(0, "face has no vertical metrics");
      return 0;
    }
    loadFlags |= FT_LOAD_VERTICAL_LAYOUT;
  }
  if (!handle->hint)
    loadFlags |= FT_LOAD_NO_HINTING;

  /* set the base-line based on the string ascent */
  if (!i_ft2_bbox(handle, cheight, cwidth, text, len, bbox, utf8))
    return 0;

  if (!align) {
    /* this may need adjustment */
    tx -= bbox[0] * handle->matrix[0] + bbox[5] * handle->matrix[1] + handle->matrix[2];
    ty += bbox[0] * handle->matrix[3] + bbox[5] * handle->matrix[4] + handle->matrix[5];
  }
  while (len) {
    unsigned long c;
    if (utf8) {
      c = utf8_advance(&text, &len);
      if (c == ~0UL) {
        i_push_error(0, "invalid UTF8 character");
        return 0;
      }
    }
    else {
      c = (unsigned char)*text++;
      --len;
    }
    
    index = FT_Get_Char_Index(handle->face, c);
    error = FT_Load_Glyph(handle->face, index, loadFlags);
    if (error) {
      ft2_push_message(error);
      i_push_errorf(0, "loading glyph for character \\x%02x (glyph 0x%04X)", 
                    c, index);
      return 0;
    }
    slot = handle->face->glyph;
    gm = &slot->metrics;

    error = FT_Render_Glyph(slot, aa ? ft_render_mode_normal : ft_render_mode_mono);
    if (error) {
      ft2_push_message(error);
      i_push_errorf(0, "rendering glyph 0x%04X (character \\x%02X)");
      return 0;
    }
    if (slot->bitmap.pixel_mode == ft_pixel_mode_mono) {
      bmp = slot->bitmap.buffer;
      for (y = 0; y < slot->bitmap.rows; ++y) {
        int pos = 0;
        int bit = 0x80;
        for (x = 0; x < slot->bitmap.width; ++x) {
          if (bmp[pos] & bit)
            i_ppix(im, tx+x+slot->bitmap_left, ty+y-slot->bitmap_top, cl);

          bit >>= 1;
          if (bit == 0) {
            bit = 0x80;
            ++pos;
          }
        }
        bmp += slot->bitmap.pitch;
      }
    }
    else {
      /* grey scale or something we can treat as greyscale */
      /* we create a map to convert from the bitmap values to 0-255 */
      if (last_mode != slot->bitmap.pixel_mode 
          || last_grays != slot->bitmap.num_grays) {
        if (!make_bmp_map(&slot->bitmap, map))
          return 0;
        last_mode = slot->bitmap.pixel_mode;
        last_grays = slot->bitmap.num_grays;
      }
      
      bmp = slot->bitmap.buffer;
      for (y = 0; y < slot->bitmap.rows; ++y) {
        for (x = 0; x < slot->bitmap.width; ++x) {
          int value = map[bmp[x]];
          if (value) {
            i_gpix(im, tx+x+slot->bitmap_left, ty+y-slot->bitmap_top, &pel);
            for (ch = 0; ch < im->channels; ++ch) {
              pel.channel[ch] = 
                ((255-value)*pel.channel[ch] + value * cl->channel[ch]) / 255;
            }
            i_ppix(im, tx+x+slot->bitmap_left, ty+y-slot->bitmap_top, &pel);
          }
        }
        bmp += slot->bitmap.pitch;
      }
    }

    tx += slot->advance.x / 64;
    ty -= slot->advance.y / 64;
  }

  return 1;
}

/*
=item i_ft2_cp(FT2_Fonthandle *handle, i_img *im, int tx, int ty, int channel, double cheight, double cwidth, char *text, int len, int align, int aa)

Renders I<text> to (I<tx>, I<ty>) in I<im> to I<channel> at the given 
I<cheight> and I<cwidth>.

If align is 0, then the text is rendered with the top-left of the
first character at (I<tx>, I<ty>).  If align is non-zero then the text
is rendered with (I<tx>, I<ty>) aligned with the base-line of the
characters.

If aa is non-zero then the text is anti-aliased.

Returns non-zero on success.

=cut
*/

i_ft2_cp(FT2_Fonthandle *handle, i_img *im, int tx, int ty, int channel,
         double cheight, double cwidth, char *text, int len, int align,
         int aa, int vlayout, int utf8) {
  int bbox[8];
  i_img *work;
  i_color cl, cl2;
  int x, y;

  mm_log((1, "i_ft2_cp(handle %p, im %p, tx %d, ty %d, channel %d, cheight %f, cwidth %f, text %p, len %d, ...)\n", 
	  handle, im, tx, ty, channel, cheight, cwidth, text, len));

  if (vlayout && !FT_HAS_VERTICAL(handle->face)) {
    i_push_error(0, "face has no vertical metrics");
    return 0;
  }

  if (!i_ft2_bbox_r(handle, cheight, cwidth, text, len, vlayout, utf8, bbox))
    return 0;

  work = i_img_empty_ch(NULL, bbox[2]-bbox[0]+1, bbox[3]-bbox[1]+1, 1);
  cl.channel[0] = 255;
  if (!i_ft2_text(handle, work, -bbox[0], -bbox[1], &cl, cheight, cwidth, 
                  text, len, 1, aa, vlayout, utf8))
    return 0;

  if (!align) {
    tx -= bbox[4];
    ty += bbox[5];
  }
  
  /* render to the specified channel */
  /* this will be sped up ... */
  for (y = 0; y < work->ysize; ++y) {
    for (x = 0; x < work->xsize; ++x) {
      i_gpix(work, x, y, &cl);
      i_gpix(im, tx + x + bbox[0], ty + y + bbox[1], &cl2);
      cl2.channel[channel] = cl.channel[0];
      i_ppix(im, tx + x + bbox[0], ty + y + bbox[1], &cl2);
    }
  }
  i_img_destroy(work);
  return 1;
}

/*
=item i_ft2_has_chars(handle, char *text, int len, int utf8, char *out)

Check if the given characters are defined by the font.

Returns the number of characters that were checked.

=cut
*/
int i_ft2_has_chars(FT2_Fonthandle *handle, char *text, int len, int utf8, 
                       char *out) {
  int count = 0;
  mm_log((1, "i_ft2_check_chars(handle %p, text %p, len %d, utf8 %d)\n", 
	  handle, text, len, utf8));

  while (len) {
    unsigned long c;
    int index;
    if (utf8) {
      c = utf8_advance(&text, &len);
      if (c == ~0UL) {
        i_push_error(0, "invalid UTF8 character");
        return 0;
      }
    }
    else {
      c = (unsigned char)*text++;
      --len;
    }
    
    index = FT_Get_Char_Index(handle->face, c);
    *out++ = index != 0;
    ++count;
  }

  return count;
}

/* uses a method described in fterrors.h to build an error translation
   function
*/
#undef __FT_ERRORS_H__
#define FT_ERRORDEF(e, v, s) case v: i_push_error(code, s); return;
#define FT_ERROR_START_LIST
#define FT_ERROR_END_LIST

/*
=back

=head2 Internal Functions

These functions are used in the implementation of freetyp2.c and should not
(usually cannot) be called from outside it.

=over

=item ft2_push_message(int code)

Pushes an error message corresponding to code onto the error stack.

=cut
*/
static void ft2_push_message(int code) {
  char unknown[40];

  switch (code) {
#include FT_ERRORS_H
  }

  sprintf(unknown, "Unknown Freetype2 error code 0x%04X\n", code);
  i_push_error(code, unknown);
}

/*
=item make_bmp_map(FT_Bitmap *bitmap, unsigned char *map)

Creates a map to convert grey levels from the glyphs bitmap into
values scaled 0..255.

=cut
*/
static int
make_bmp_map(FT_Bitmap *bitmap, unsigned char *map) {
  int scale;
  int i;

  switch (bitmap->pixel_mode) {
  case ft_pixel_mode_grays:
    scale = bitmap->num_grays;
    break;
    
  default:
    i_push_errorf(0, "I can't handle pixel mode %d", bitmap->pixel_mode);
    return 0;
  }

  /* build the table */
  for (i = 0; i < scale; ++i)
    map[i] = i * 255 / (bitmap->num_grays - 1);

  return 1;
}

struct utf8_size {
  int mask, expect;
  int size;
};

struct utf8_size utf8_sizes[] =
{
  { 0x80, 0x00, 1 },
  { 0xE0, 0xC0, 2 },
  { 0xF0, 0xE0, 3 },
  { 0xF8, 0xF0, 4 },
};

/*
=item utf8_advance(char **p, int *len)

Retreive a UTF8 character from the stream.

Modifies *p and *len to indicate the consumed characters.

This doesn't support the extended UTF8 encoding used by later versions
of Perl.

=cut
*/

unsigned long utf8_advance(char **p, int *len) {
  unsigned char c;
  int i, ci, clen = 0;
  unsigned char codes[3];
  if (*len == 0)
    return ~0UL;
  c = *(*p)++; --*len;

  for (i = 0; i < sizeof(utf8_sizes)/sizeof(*utf8_sizes); ++i) {
    if ((c & utf8_sizes[i].mask) == utf8_sizes[i].expect) {
      clen = utf8_sizes[i].size;
    }
  }
  if (clen == 0 || *len < clen-1) {
    --*p; ++*len;
    return ~0UL;
  }

  /* check that each character is well formed */
  i = 1;
  ci = 0;
  while (i < clen) {
    if (((*p)[ci] & 0xC0) != 0x80) {
      --*p; ++*len;
      return ~0UL;
    }
    codes[ci] = (*p)[ci];
    ++ci; ++i;
  }
  *p += clen-1; *len -= clen-1;
  if (c & 0x80) {
    if ((c & 0xE0) == 0xC0) {
      return ((c & 0x1F) << 6) + (codes[0] & 0x3F);
    }
    else if ((c & 0xF0) == 0xE0) {
      return ((c & 0x0F) << 12) | ((codes[0] & 0x3F) << 6) | (codes[1] & 0x3f);
    }
    else if ((c & 0xF8) == 0xF0) {
      return ((c & 0x07) << 18) | ((codes[0] & 0x3F) << 12) 
              | ((codes[1] & 0x3F) << 6) | (codes[2] & 0x3F);
    }
    else {
      *p -= clen; *len += clen;
      return ~0UL;
    }
  }
  else {
    return c;
  }
}

/*
=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>, with a fair amount of help from
reading the code in font.c.

=head1 SEE ALSO

font.c, Imager::Font(3), Imager(3)

http://www.freetype.org/

=cut
*/

