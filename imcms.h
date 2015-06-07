#ifndef IMAGER_IMCMS_H
#define IMAGER_IMCMS_H

#include "imdatatypes.h"

extern void imcms_initialize(im_context_t ctx);
extern void imcms_finalize(im_context_t ctx);

extern imcms_curve_t imcms_srgb_curve(im_context_t ctx);
extern imcms_curve_t imcms_linear_curve(im_context_t ctx);
extern imcms_curve_t imcms_gamma_curve(im_context_t ctx, double gamma);
extern void imcms_free_curve(imcms_curve_t curve);

extern double imcms_to_linearf(imcms_curve_t curve, double value);
extern double imcms_from_linearf(imcms_curve_t curve, double value);

extern imcms_profile_t imcms_srgb_profile(im_context_t ctx);
extern imcms_profile_t imcms_sgray_profile(im_context_t ctx);
extern imcms_profile_t imcms_read_profile_file(const char *filename);
extern imcms_profile_t imcms_read_profile_mem(void *buf, size_t size);
extern int imcms_profile_channels(imcms_profile_t profile);
extern int imcms_profile_curves(imcms_profile_t profile, imcms_curve_t *curves, int count);
extern void imcms_free_profile(imcms_profile_t profile);

extern imcms_reader_t imcms_start_reader(i_img_dim width, i_img_dim height, int color_model);
extern void imcms_end_reader(imcms_reader_t reader);

#endif
