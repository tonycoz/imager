/* Declares utility functions useful across various files which
   aren't meant to be available externally
*/

#ifndef IMAGEI_H_
#define IMAGEI_H_

#include "image.h"

/* wrapper functions that implement the floating point sample version of a 
   function in terms of the 8-bit sample version
*/
extern int i_ppixf_fp(i_img *im, int x, int y, i_fcolor *pix);
extern int i_gpixf_fp(i_img *im, int x, int y, i_fcolor *pix);
extern int i_plinf_fp(i_img *im, int l, int r, int y, i_fcolor *pix);
extern int i_glinf_fp(i_img *im, int l, int r, int y, i_fcolor *pix);
extern int i_gsampf_fp(i_img *im, int l, int r, int y, i_fsample_t *samp,
                       int *chans, int chan_count);

/* wrapper functions that forward palette calls to the underlying image,
   assuming the underlying image is the first pointer in whatever
   ext_data points at
*/
extern int i_addcolors_forward(i_img *im, i_color *, int count);
extern int i_getcolors_forward(i_img *im, int i, i_color *, int count);
extern int i_colorcount_forward(i_img *im);
extern int i_maxcolors_forward(i_img *im);
extern int i_findcolor_forward(i_img *im, i_color *color, i_palidx *entry);
extern int i_setcolors_forward(i_img *im, int index, i_color *colors, 
                               int count);

#define SampleFTo16(num) ((int)((num) * 65535.0 + 0.01))
/* we add that little bit to avoid rounding issues */
#define Sample16ToF(num) ((num) / 65535.0)

#define SampleFTo8(num) ((int)((num) * 255.0 + 0.01))
#define Sample8ToF(num) ((num) / 255.0)

#define Sample16To8(num) ((num) / 257)
#define Sample8To16(num) ((num) * 257)

extern void i_get_combine(int combine, i_fill_combine_f *, i_fill_combinef_f *);

#endif
