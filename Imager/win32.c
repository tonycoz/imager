#include "image.h"
#define STRICT
#include <windows.h>

/*
=head1 NAME

win32.c - implements some win32 specific code, specifically Win32 font support.

=head1 SYNOPSIS

   int bbox[6];
   if (i_wf_bbox(facename, size, text, text_len, bbox)) {
     // we have the bbox
   }
   i_wf_text(face, im, tx, ty, cl, size, text, len, align, aa);
   i_wf_cp(face, im, tx, ty, channel, size, text, len, align, aa)

=head1 DESCRIPTION

An Imager interface to font output using the Win32 GDI.

=over

=cut
*/

#define fixed(x) ((x).value + ((x).fract) / 65536.0)

static void set_logfont(char *face, int size, LOGFONT *lf);

static LPVOID render_text(char *face, int size, char *text, int length, int aa,
                          HBITMAP *pbm, SIZE *psz, TEXTMETRIC *tm);

/*
=item i_wf_bbox(face, size, text, length, bbox)

Calculate a bounding box for the text.

=cut
*/

int i_wf_bbox(char *face, int size, char *text, int length, int *bbox) {
  LOGFONT lf;
  HFONT font, oldFont;
  HDC dc;
  SIZE sz;
  TEXTMETRIC tm;
  ABC first, last;
  GLYPHMETRICS gm;
  int i;
  MAT2 mat;

  set_logfont(face, size, &lf);
  font = CreateFontIndirect(&lf);
  if (!font) 
    return 0;
  dc = GetDC(NULL);
  oldFont = (HFONT)SelectObject(dc, font);

#if 1
  if (!GetTextExtentPoint32(dc, text, length, &sz)
      || !GetTextMetrics(dc, &tm)) {
    SelectObject(dc, oldFont);
    ReleaseDC(NULL, dc);
    DeleteObject(font);
    return 0;
  }
  /* if there's a way to get a characters ascent/descent reliably, I can't
     see it.  GetGlyphOutline() seems to return the same size for
     all characters.
  */
  bbox[1] = bbox[4] = tm.tmDescent;
  bbox[2] = sz.cx;
  bbox[3] = bbox[5] = tm.tmAscent;
  
  if (GetCharABCWidths(dc, text[0], text[0], &first)
      && GetCharABCWidths(dc, text[length-1], text[length-1], &last)) {
    bbox[0] = first.abcA;
    if (last.abcC < 0)
      bbox[2] -= last.abcC;
  }
  else {
    bbox[0] = 0;
  }
#else
  for (i = 0; i < length; ++i) {
    memset(&gm, 0, sizeof(gm));
    memset(&mat, 0, sizeof(mat));
    mat.eM11.value = 1;
    mat.eM22.value = 1;
    if (GetGlyphOutline(dc, GGO_METRICS, text[i], &gm, 0, NULL, &mat) != GDI_ERROR) {
      printf("%02X: black (%d, %d) origin (%d, %d) cell(%d, %d)\n",
	     text[i], gm.gmBlackBoxX, gm.gmBlackBoxY, gm.gmptGlyphOrigin.x, 
	     gm.gmptGlyphOrigin.y, gm.gmCellIncX, gm.gmCellIncY);
      printf("  : mat [ %-8f  %-8f ]\n", fixed(mat.eM11), fixed(mat.eM12));
      printf("        [ %-8f  %-8f ]\n", fixed(mat.eM21), fixed(mat.eM22));
    }
    else {
      printf("Could not get metrics for '\\x%02X'\n", text[i]);
    }
    if (GetCharABCWidths(dc, text[i], text[i], &first)) {
      printf("%02X: %d %d %d\n", text[i], first.abcA, first.abcB, first.abcC);
    }
  }
#endif

  SelectObject(dc, oldFont);
  ReleaseDC(NULL, dc);
  DeleteObject(font);

  return 6;
}

/*
=item i_wf_text(face, im, tx, ty, cl, size, text, len, align, aa)

Draws the text in the given color.

=cut
*/

int
i_wf_text(char *face, i_img *im, int tx, int ty, i_color *cl, int size, 
	  char *text, int len, int align, int aa) {
  unsigned char *bits;
  HBITMAP bm;
  SIZE sz;
  int line_width;
  int x, y;
  int ch;
  TEXTMETRIC tm;
  int top;

  bits = render_text(face, size, text, len, aa, &bm, &sz, &tm);
  if (!bits)
    return 0;
  
  line_width = sz.cx * 3;
  line_width = (line_width + 3) / 4 * 4;
  top = ty;
  if (align)
    top -= tm.tmAscent;

  for (y = 0; y < sz.cy; ++y) {
    for (x = 0; x < sz.cx; ++x) {
      i_color pel;
      int scale = bits[3 * x];
      i_gpix(im, tx+x, top+sz.cy-y-1, &pel);
      for (ch = 0; ch < im->channels; ++ch) {
	pel.channel[ch] = 
	  ((255-scale) * pel.channel[ch] + scale*cl->channel[ch]) / 255.0;
      }
      i_ppix(im, tx+x, top+sz.cy-y-1, &pel);
    }
    bits += line_width;
  }
  DeleteObject(bm);

  return 1;
}

/*
=item i_wf_cp(face, im, tx, ty, channel, size, text, len, align, aa)

Draws the text in the given channel.

=cut
*/

int
i_wf_cp(char *face, i_img *im, int tx, int ty, int channel, int size, 
	  char *text, int len, int align, int aa) {
  unsigned char *bits;
  HBITMAP bm;
  SIZE sz;
  int line_width;
  int x, y;
  int ch;
  TEXTMETRIC tm;
  int top;

  bits = render_text(face, size, text, len, aa, &bm, &sz, &tm);
  if (!bits)
    return 0;
  
  line_width = sz.cx * 3;
  line_width = (line_width + 3) / 4 * 4;
  top = ty;
  if (align)
    top -= tm.tmAscent;

  for (y = 0; y < sz.cy; ++y) {
    for (x = 0; x < sz.cx; ++x) {
      i_color pel;
      int scale = bits[3 * x];
      i_gpix(im, tx+x, top+sz.cy-y-1, &pel);
      pel.channel[channel] = scale;
      i_ppix(im, tx+x, top+sz.cy-y-1, &pel);
    }
    bits += line_width;
  }
  DeleteObject(bm);

  return 1;
}

/*
=back

=head1 INTERNAL FUNCTIONS

=over

=item set_logfont(face, size, lf)

Fills in a LOGFONT structure with reasonable defaults.

=cut
*/

static void set_logfont(char *face, int size, LOGFONT *lf) {
  memset(lf, 0, sizeof(LOGFONT));

  lf->lfHeight = -size; /* character height rather than cell height */
  lf->lfCharSet = DEFAULT_CHARSET;
  lf->lfOutPrecision = OUT_TT_PRECIS;
  lf->lfClipPrecision = CLIP_DEFAULT_PRECIS;
  lf->lfQuality = PROOF_QUALITY;
  strncpy(lf->lfFaceName, face, sizeof(lf->lfFaceName)-1);
  /* NUL terminated by the memset at the top */
}

/*
=item render_text(face, size, text, length, aa, pbm, psz, tm)

renders the text to an in-memory RGB bitmap 

It would be nice to render to greyscale, but Windows doesn't have
native greyscale bitmaps.

=cut
*/
static LPVOID render_text(char *face, int size, char *text, int length, int aa,
		   HBITMAP *pbm, SIZE *psz, TEXTMETRIC *tm) {
  BITMAPINFO bmi;
  BITMAPINFOHEADER *bmih = &bmi.bmiHeader;
  HDC dc, bmpDc;
  LOGFONT lf;
  HFONT font, oldFont;
  SIZE sz;
  HBITMAP bm, oldBm;
  LPVOID bits;
  
  dc = GetDC(NULL);
  set_logfont(face, size, &lf);

#ifdef ANTIALIASED_QUALITY
  /* See KB article Q197076
     "INFO: Controlling Anti-aliased Text via the LOGFONT Structure"
  */
  lf.lfQuality = aa ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY;
#endif

  bmpDc = CreateCompatibleDC(dc);
  if (bmpDc) {
    font = CreateFontIndirect(&lf);
    if (font) {
      oldFont = SelectObject(bmpDc, font);
      GetTextExtentPoint32(bmpDc, text, length, &sz);
      GetTextMetrics(bmpDc, tm);
      
      memset(&bmi, 0, sizeof(bmi));
      bmih->biSize = sizeof(*bmih);
      bmih->biWidth = sz.cx;
      bmih->biHeight = sz.cy;
      bmih->biPlanes = 1;
      bmih->biBitCount = 24;
      bmih->biCompression = BI_RGB;
      bmih->biSizeImage = 0;
      bmih->biXPelsPerMeter = 72 / 2.54 * 100;
      bmih->biYPelsPerMeter = bmih->biXPelsPerMeter;
      bmih->biClrUsed = 0;
      bmih->biClrImportant = 0;
      
      bm = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);

      if (bm) {
	oldBm = SelectObject(bmpDc, bm);
	SetTextColor(bmpDc, RGB(255, 255, 255));
	SetBkColor(bmpDc, RGB(0, 0, 0));
	TextOut(bmpDc, 0, 0, text, length);
	SelectObject(bmpDc, oldBm);
      }
      else {
	i_push_errorf(0, "Could not create DIB section for render: %ld",
		      GetLastError());
	SelectObject(bmpDc, oldFont);
	DeleteObject(font);
	DeleteDC(bmpDc);
	ReleaseDC(NULL, dc);
	return NULL;
      }
      SelectObject(bmpDc, oldFont);
      DeleteObject(font);
    }
    else {
      i_push_errorf(0, "Could not create logical font: %ld",
		    GetLastError());
      DeleteDC(bmpDc);
      ReleaseDC(NULL, dc);
      return NULL;
    }
    DeleteDC(bmpDc);
  }
  else {
    i_push_errorf(0, "Could not create rendering DC: %ld", GetLastError());
    ReleaseDC(NULL, dc);
    return NULL;
  }

  ReleaseDC(NULL, dc);

  *pbm = bm;
  *psz = sz;

  return bits;
}

/*
=head1 BUGS

Should really use a structure so we can set more attributes.

Should support UTF8

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
*/
