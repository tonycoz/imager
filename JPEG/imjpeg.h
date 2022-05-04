#ifndef IMAGER_IMJPEG_H
#define IMAGER_IMJPEG_H

#include "imdatatypes.h"

i_img*
i_readjpeg_wiol(io_glue *data, int length, char** iptc_itext, int *itlength);

undef_int
i_writejpeg_wiol(i_img *im, io_glue *ig, int qfactor);

extern const char *
i_libjpeg_version(void);

extern int
is_turbojpeg(void);

extern int
is_mozjpeg(void);

/* whether arithmetic coding is available for encode/decode */
extern int
has_decode_arith_coding(void);
extern int
has_encode_arith_coding(void);


#endif
