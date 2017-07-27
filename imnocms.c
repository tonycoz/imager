#include "imageri.h"

/* these functions can be called during context setup, so no:
  - logging,
  - wrapped malloc
*/

/* 
   Provides a tone curve only CMS.

   Until we need more profile functions this will do.
*/

#define TONE_CURVE_SAMPLES 256

typedef enum {
  dct_srgb,
  dct_gamma,
  dct_linear
} dummy_curve_type_t;

typedef struct imcms_curve_dummy_struct {
  struct imcms_curve_base_struct base;
  dummy_curve_type_t type;
  double gamma;
  double inv_gamma;
} imcms_curve_dummy;

typedef struct imcms_profile_struct {
  int channels;
  imcms_curve_t curve;
} imcms_profile_dummy;

static im_slot_t cms_slot = -1;

void
imcms_initialize(im_context_t ctx) {
  /* nothing to do */
}

void
imcms_finalize(im_context_t ctx) {
  /* currently a no-op */
  (void)(ctx);
}

static imcms_curve_t
build_curve(dummy_curve_type_t type, double gamma) {
  imcms_curve_dummy *result = malloc(sizeof(imcms_curve_dummy));
  int i;

  result->type = type;
  result->gamma = gamma;
  result->inv_gamma = 1 / gamma;

  for (i = 0; i < 256; ++i)
    result->base.to_linear[i] = SampleFTo16(imcms_to_linearf((imcms_curve_t)result, Sample8ToF(i)));

  for (i = 0; i < 65536; ++i)
    result->base.from_linear[i] =
      SampleFTo8(imcms_from_linearf((imcms_curve_t)result, Sample16ToF(i)));

  return &result->base;
}

imcms_curve_t
imcms_srgb_curve(im_context_t ctx) {
  return build_curve(dct_srgb, 0);
}

imcms_curve_t
imcms_linear_curve(im_context_t ctx) {
  return build_curve(dct_linear, 0);
}

imcms_curve_t
imcms_gamma_curve(im_context_t ctx, double gamma) {
  return build_curve(dct_gamma, gamma);
}

void
imcms_free_curve(imcms_curve_t curve) {
  free(curve);
}

double
imcms_to_linearf(imcms_curve_t curve, double value) {
  imcms_curve_dummy *rcurve = (imcms_curve_dummy *)curve;

  switch (rcurve->type) {
  case dct_srgb:
    if (value <= 0.04045)
      return value / 12.92;
    else
      return pow(((value + 0.055) / (1+0.055)), 2.4);
    
  case dct_gamma:
    return pow(value, rcurve->gamma);
    
  case dct_linear:
    return value;
  }
  return -1;
}

double
imcms_from_linearf(imcms_curve_t curve, double value) {
  imcms_curve_dummy *rcurve = (imcms_curve_dummy *)curve;

  switch (rcurve->type) {
  case dct_srgb:
    if (value <= 0.0031308)
      return value * 12.92;
    else
      return (1+0.055) * pow(value, (1/2.4)) - 0.055;
    
  case dct_gamma:
    return pow(value, rcurve->inv_gamma);
    
  case dct_linear:
    return value;
  }
  return -1;
}

static imcms_profile_t
build_profile(int channels) {
  imcms_profile_dummy *result = malloc(sizeof(imcms_profile_dummy));

  result->channels = channels;
  result->curve = NULL;

  return result;
}

imcms_profile_t
imcms_srgb_profile(im_context_t ctx) {
  return build_profile(3);
}

imcms_profile_t
imcms_sgray_profile(im_context_t ctx) {
  return build_profile(1);
}

void
imcms_free_profile(imcms_profile_t prof) {
  imcms_profile_dummy *profile = (imcms_profile_dummy *)prof;
  if (profile->curve)
    imcms_free_curve(profile->curve);
  free(profile);
}

int
imcms_profile_curves(imcms_profile_t prof, imcms_curve_t *curves,
		     int count) {
  imcms_profile_dummy *profile = (imcms_profile_dummy *)prof;
  int i;

  if (count != profile->channels)
    return 0;

  if (!profile->curve) {
    profile->curve = imcms_srgb_curve(NULL);
    if (!profile->curve) {
      i_push_error(0, "imcms_profile_curves: could not create curve");
      return 0;
    }
  }

  for (i = 0; i < count; ++i) {
    curves[i] = profile->curve;
  }

  return 1;
}
