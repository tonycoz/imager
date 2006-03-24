#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "imext.h"
#include "imperl.h"
#include "imicon.h"

DEFINE_IMAGER_CALLBACKS;

MODULE = Imager::File::ICO  PACKAGE = Imager::File::ICO

PROTOTYPES: DISABLE

Imager::ImgRaw
i_readico_single(ig, index)
	Imager::IO ig
	int index

void
i_readico_multi(ig)
	Imager::IO ig
      PREINIT:
        i_img **imgs;
        int count;
        int i;
      PPCODE:
        imgs = i_readico_multi(ig, &count);
        if (imgs) {
          EXTEND(SP, count);
          for (i = 0; i < count; ++i) {
            SV *sv = sv_newmortal();
            sv_setref_pv(sv, "Imager::ImgRaw", (void *)imgs[i]);
            PUSHs(sv);
          }
          myfree(imgs);
        }


BOOT:
	PERL_INITIALIZE_IMAGER_CALLBACKS;
