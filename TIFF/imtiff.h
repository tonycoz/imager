#ifndef IMAGER_IMTIFF_H
#define IMAGER_IMTIFF_H

#include "imdatatypes.h"

void i_tiff_init(void);
i_img   * i_readtiff_wiol(io_glue *ig, int allow_incomplete, int page);
i_img  ** i_readtiff_multi_wiol(io_glue *ig, int *count);
undef_int i_writetiff_wiol(i_img *im, io_glue *ig);
undef_int i_writetiff_multi_wiol(io_glue *ig, i_img **imgs, int count);
undef_int i_writetiff_wiol_faxable(i_img *im, io_glue *ig, int fine);
undef_int i_writetiff_multi_wiol_faxable(io_glue *ig, i_img **imgs, int count, int fine);
char const * i_tiff_libversion(void);
char const * i_tiff_builddate(void);
int i_tiff_has_compression(char const *name);

typedef struct {
  const char *description;
  const char *name;
  unsigned code;
} i_tiff_codec;

i_tiff_codec *i_tiff_get_codecs(size_t *pcount);

#endif
