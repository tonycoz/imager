#ifndef IMAGER_IMICON_H
#define IMAGER_IMICON_H

#include "imext.h"

extern i_img *
i_readico_single(io_glue *ig, int index);
extern i_img **
i_readico_multi(io_glue *ig, int *count);

#endif
