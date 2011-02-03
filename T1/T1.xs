#define PERL_NO_GET_CONTEXT
#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "imext.h"
#include "imperl.h"
#include "imt1.h"

DEFINE_IMAGER_CALLBACKS;

MODULE = Imager::Font::T1  PACKAGE = Imager::Font::T1

undef_int
i_init_t1(t1log)
	int t1log

void
i_t1_set_aa(st)
      	       int     st

int
i_t1_new(pfb,afm)
       	      char*    pfb
       	      char*    afm

int
i_t1_destroy(font_id)
       	       int     font_id


undef_int
i_t1_cp(im,xb,yb,channel,fontnum,points,str_sv,len_ignored,align,utf8=0,flags="")
    Imager::ImgRaw     im
	       int     xb
	       int     yb
	       int     channel
	       int     fontnum
             float     points
	        SV*    str_sv
	       int     align
               int     utf8
              char*    flags
             PREINIT:
               char *str;
               STRLEN len;
             CODE:
#ifdef SvUTF8
               if (SvUTF8(str_sv))
                 utf8 = 1;
#endif
               str = SvPV(str_sv, len);
               RETVAL = i_t1_cp(im, xb,yb,channel,fontnum,points,str,len,align,
                                  utf8,flags);
           OUTPUT:
             RETVAL


void
i_t1_bbox(fontnum,point,str_sv,len_ignored,utf8=0,flags="")
               int     fontnum
	     float     point
	        SV*    str_sv
               int     utf8
              char*    flags
	     PREINIT:
               char *str;
               STRLEN len;
	       int     cords[BOUNDING_BOX_COUNT];
               int i;
               int rc;
	     PPCODE:
#ifdef SvUTF8
               if (SvUTF8(str_sv))
                 utf8 = 1;
#endif
               str = SvPV(str_sv, len);
               rc = i_t1_bbox(fontnum,point,str,len,cords,utf8,flags);
               if (rc > 0) {
                 EXTEND(SP, rc);
                 for (i = 0; i < rc; ++i)
                   PUSHs(sv_2mortal(newSViv(cords[i])));
               }



undef_int
i_t1_text(im,xb,yb,cl,fontnum,points,str_sv,len_ignored,align,utf8=0,flags="")
    Imager::ImgRaw     im
	       int     xb
	       int     yb
     Imager::Color    cl
	       int     fontnum
             float     points
	        SV*    str_sv
	       int     align
               int     utf8
              char*    flags
             PREINIT:
               char *str;
               STRLEN len;
             CODE:
#ifdef SvUTF8
               if (SvUTF8(str_sv))
                 utf8 = 1;
#endif
               str = SvPV(str_sv, len);
               RETVAL = i_t1_text(im, xb,yb,cl,fontnum,points,str,len,align,
                                  utf8,flags);
           OUTPUT:
             RETVAL

void
i_t1_has_chars(handle, text_sv, utf8 = 0)
        int handle
        SV  *text_sv
        int utf8
      PREINIT:
        char const *text;
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
        count = i_t1_has_chars(handle, text, len, utf8, work);
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
i_t1_face_name(handle)
        int handle
      PREINIT:
        char name[255];
        int len;
      PPCODE:
        len = i_t1_face_name(handle, name, sizeof(name));
        if (len) {
          EXTEND(SP, 1);
          PUSHs(sv_2mortal(newSVpv(name, strlen(name))));
        }

void
i_t1_glyph_name(handle, text_sv, utf8 = 0)
        int handle
        SV *text_sv
        int utf8
      PREINIT:
        char const *text;
        STRLEN work_len;
        size_t len;
        char name[255];
      PPCODE:
#ifdef SvUTF8
        if (SvUTF8(text_sv))
          utf8 = 1;
#endif
        text = SvPV(text_sv, work_len);
        len = work_len;
        while (len) {
          unsigned long ch;
          if (utf8) {
            ch = i_utf8_advance(&text, &len);
            if (ch == ~0UL) {
              i_push_error(0, "invalid UTF8 character");
              break;
            }
          }
          else {
            ch = *text++;
            --len;
          }
          EXTEND(SP, 1);
          if (i_t1_glyph_name(handle, ch, name, sizeof(name))) {
            PUSHs(sv_2mortal(newSVpv(name, 0)));
          }
          else {
            PUSHs(&PL_sv_undef);
          } 
        }

BOOT:
	PERL_INITIALIZE_IMAGER_CALLBACKS;
