#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
#ifdef __cplusplus
}
#endif

#include "imext.h"
#include "imperl.h"
#include "imbif.h"

typedef i_bif_handle *Imager__Font__BI;
#define i_bif_DESTROY i_bif_destroy

DEFINE_IMAGER_CALLBACKS;


MODULE = Imager::Font::BuiltIn   PACKAGE = Imager::Font::BuiltIn

PROTOTYPES: ENABLE

Imager::Font::BI
i_bif_new(face)
	const char *face

int
i_bif_text(font, im, tx, ty, size, cl, text_sv, align, utf8=0, dir=0)
	Imager::Font::BI font
	Imager::ImgRaw im
	int tx
	int ty
	double size
	Imager::Color cl
	SV *text_sv
	int align
	int utf8
	int dir
      PREINIT:
	char *text;
	STRLEN len;
      CODE:
#ifdef SvUTF8
        if (SvUTF8(text_sv)) {
          utf8 = 1;
        }
#endif
	text = SvPV(text_sv, len);
	RETVAL = i_bif_text(font, im, tx, ty, size, cl, text, len, align, utf8, dir);
      OUTPUT:
	RETVAL

int
i_bif_bbox(font, size, text_sv, utf8)
	Imager::Font::BI font
	double size
	SV *text_sv
	int utf8
      PREINIT:
        int bbox[BOUNDING_BOX_COUNT];
        int i;
        char *text;
        STRLEN text_len;
        int rc;
      PPCODE:
        text = SvPV(text_sv, text_len);
#ifdef SvUTF8
        if (SvUTF8(text_sv))
          utf8 = 1;
#endif
        rc = i_bif_bbox(font, size, text, text_len, utf8, bbox);
        if (rc) {
          EXTEND(SP, rc);
          for (i = 0; i < rc; ++i)
            PUSHs(sv_2mortal(newSViv(bbox[i])));
        }

void
i_bif_has_chars(handle, text_sv, utf8)
        Imager::Font::BI handle
        SV  *text_sv
        int utf8
      PREINIT:
        char *text;
        STRLEN len;
        char *work;
        int count;
        int i;
      PPCODE:
#ifdef SvUTF8
        if (SvUTF8(text_sv))
          utf8 = 1;
#endif
        text = SvPV(text_sv, len);
        work = mymalloc(len);
        count = i_bif_has_chars(handle, text, len, utf8, work);
        if (GIMME_V == G_ARRAY) {
          EXTEND(SP, count);
          for (i = 0; i < count; ++i) {
            PUSHs(sv_2mortal(newSViv(work[i])));
          }
        }
        else {
          EXTEND(SP, 1);
          PUSHs(sv_2mortal(newSVpv(work, count)));
        }
        myfree(work);

void
i_ft2_face_name(handle)
        Imager::Font::BI handle
      PREINIT:
        char name[255];
        int len;
      PPCODE:
        len = i_ft2_face_name(handle, name, sizeof(name));
        if (len) {
          EXTEND(SP, 1);
          PUSHs(sv_2mortal(newSVpv(name, 0)));
        }

MODULE = Imager::Font::BuiltIn   PACKAGE = Imager::Font::BI PREFIX=i_bif_

void
i_bif_DESTROY(handle)
	Imager::Font::BI handle

BOOT:
        PERL_INITIALIZE_IMAGER_CALLBACKS;

