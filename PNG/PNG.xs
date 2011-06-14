#define PERL_NO_GET_CONTEXT
#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "imext.h"
#include "imperl.h"
#include "impng.h"

DEFINE_IMAGER_CALLBACKS;

MODULE = Imager::File::PNG  PACKAGE = Imager::File::PNG

Imager::ImgRaw
i_readpng_wiol(ig)
        Imager::IO     ig

undef_int
i_writepng_wiol(im, ig)
    Imager::ImgRaw     im
        Imager::IO     ig

unsigned
i_png_lib_version()

BOOT:
	PERL_INITIALIZE_IMAGER_CALLBACKS;
