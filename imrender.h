#ifndef IMAGER_IMRENDER_H
#define IMAGER_IMRENDER_H

#include "rendert.h"

extern void
i_render_init(i_render *r, i_img *im, int width);
extern void
i_render_done(i_render *r);
extern void
i_render_color(i_render *r, int x, int y, int width, unsigned char const *src,
               i_color const *color);

#endif
