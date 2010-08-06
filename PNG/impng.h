#ifndef IMAGER_IMPNG_H
#define IMAGER_IMPNG_H

#include "imext.h"

i_img    *i_readpng_wiol(io_glue *ig);
undef_int i_writepng_wiol(i_img *im, io_glue *ig);

#endif
