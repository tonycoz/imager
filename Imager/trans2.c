#include "image.h"
#include "regmach.h"

/* foo test */

i_img* i_transform2(int width, int height, int channels,
		    struct rm_op *ops, int ops_count, 
		    double *n_regs, int n_regs_count, 
		    i_color *c_regs, int c_regs_count, 
		    i_img **in_imgs, int in_imgs_count)
{
  i_img *new_img;
  int x, y;
  i_color val;
  int i;
  
  /* since the number of images is variable and the image numbers
     for getp? are fixed, we can check them here instead of in the 
     register machine - this will help performance */
  for (i = 0; i < ops_count; ++i) {
    switch (ops[i].code) {
    case rbc_getp1:
    case rbc_getp2:
    case rbc_getp3:
      if (ops[i].code - rbc_getp1 + 1 > in_imgs_count) {
	/* Foo */
	return NULL;
      }
    }
  }

  new_img = i_img_empty_ch(NULL, width, height, channels);
  for (x = 0; x < width; ++x) {
    for (y = 0; y < height; ++y) {
      n_regs[0] = x;
      n_regs[1] = y;
      val = rm_run(ops, ops_count, n_regs, n_regs_count, c_regs, c_regs_count, 
		   in_imgs, in_imgs_count);
      i_ppix(new_img, x, y, &val);
    }
  }
  
  return new_img;
}
