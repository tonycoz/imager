#ifndef BUILTIN_H_
#define BUILTIN_H_

#include "imdatatypes.h"
#include <stddef.h>

typedef struct i_bif_handle_tag i_bif_handle;

extern i_bif_handle *i_bif_new(const char *name);
extern void i_bif_destroy(i_bif_handle *handle);

extern int i_bif_bbox(i_bif_handle *handle, double size, char const *text, int len, int utf8, int *bbox);
extern int i_bif_text(i_bif_handle *handle, i_img *im, int tx, int ty, double size, i_color *cl, const char *text, int len, int align, int utf8);
extern int i_bif_has_chars(i_bif_handle *handle, char const *test, int len, int utf8, char *out);
extern int i_bif_face_name(i_bif_handle *handle, char *name_buf, size_t name_buf_size);

#endif
