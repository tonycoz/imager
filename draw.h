#include "imager.h"

typedef struct {
  int min,max;
}  minmax;

typedef struct {
  minmax *data;
  int lines;
} i_mmarray;

/* FIXME: Merge this into datatypes.{c,h} */

void i_mmarray_cr(i_mmarray *ar,int l);
void i_mmarray_dst(i_mmarray *ar);
void i_mmarray_add(i_mmarray *ar,int x,int y);
int i_mmarray_gmin(i_mmarray *ar,int y);
int i_mmarray_getm(i_mmarray *ar,int y);
void i_mmarray_render(i_img *im,i_mmarray *ar,i_color *val);
void i_mmarray_info(i_mmarray *ar);

