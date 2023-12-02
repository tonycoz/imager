#ifndef IMAGER_IMSGI_H
#define IMAGER_IMSGI_H

#include "imext.h"

extern i_img *
i_readsgi_wiol(io_glue *ig, int partial);

extern int
i_writesgi_wiol(i_io_glue_t *ig, i_img *im);

#endif
