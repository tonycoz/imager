#ifndef IMAGER_IMW32_H
#define IMAGER_IMW32_H

#include "imdatatypes.h"

extern int i_wf_bbox(const char *face, int size, const char *text, int length, int *bbox, int utf8);
extern int i_wf_text(const char *face, i_img *im, int tx, int ty, const i_color *cl, 
		     int size, const char *text, int len, int align, int aa, int utf8);
extern int i_wf_cp(const char *face, i_img *im, int tx, int ty, int channel, 
		   int size, const char *text, int len, int align, int aa, int utf8);
extern int i_wf_addfont(char const *file);
extern int i_wf_delfont(char const *file);

#endif
