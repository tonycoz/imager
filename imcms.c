#include "imageri.h"
#include <lcms2.h>

/* these functions can be called during context setup, so no:
  - logging,
  - wrapped malloc
*/

#define TONE_CURVE_SAMPLES 256

typedef struct imcms_curve_lcms2_struct {
  struct imcms_curve_base_struct base;
  cmsToneCurve *curve;
  cmsToneCurve *reverse;
} imcms_curve_lcms2;

typedef struct imcms_profile_struct {
  cmsHPROFILE profile;
  int channels;
  imcms_curve_t *curves;
} imcms_profile_lcms2;

static im_slot_t cms_slot = -1;

static void
ctx_destroy(void *ctx) {
  cmsDeleteContext(ctx);
}

void
imcms_initialize(im_context_t ctx) {
  cmsContext cms_ctx;
  if (cms_slot == -1) {
    cms_slot = im_context_slot_new(ctx_destroy);
  }

  cms_ctx = cmsCreateContext(NULL, ctx);
  im_context_slot_set(ctx, cms_slot, cms_ctx);
}

void
imcms_finalize(im_context_t ctx) {
  /* currently a no-op */
  (void)(ctx);
}

#define dCMSctx(ctx) cmsContext cms_ctx = im_context_slot_get(ctx, cms_slot)

static imcms_curve_t
build_curve(cmsToneCurve *curve) {
  imcms_curve_lcms2 *result = malloc(sizeof(imcms_curve_lcms2));
  int i;

  if (!result) {
    cmsFreeToneCurve(curve);
    return NULL;
  }

  result->curve = curve;
  result->reverse = cmsReverseToneCurveEx(TONE_CURVE_SAMPLES, curve);

  for (i = 0; i < IM_TO_LINEAR_COUNT; ++i)
    result->base.to_linear[i] = cmsEvalToneCurve16(curve, Sample8To16(i));

  for (i = 0; i < IM_FROM_LINEAR_COUNT; ++i)
    result->base.from_linear[i] =
      Sample16To8(cmsEvalToneCurve16(result->reverse, i));

  return &result->base;
}

static const cmsFloat64Number srgb_params[5] =
  {
    2.4,
    1. / 1.055,
    0.055 / 1.055,
    1. / 12.92,
    0.04045
  };

imcms_curve_t
imcms_srgb_curve(im_context_t ctx) {
  dCMSctx(ctx);
  return build_curve(cmsBuildParametricToneCurve(cms_ctx, 4, srgb_params));
}

imcms_curve_t
imcms_linear_curve(im_context_t ctx) {
  dCMSctx(ctx);
  return build_curve(cmsBuildGamma(cms_ctx, 1.0));
}

imcms_curve_t
imcms_gamma_curve(im_context_t ctx, double gamma) {
  dCMSctx(ctx);
  return build_curve(cmsBuildGamma(cms_ctx, gamma));
}

void
imcms_free_curve(imcms_curve_t curve) {
  imcms_curve_lcms2 *rcurve = (imcms_curve_lcms2 *)curve;

  cmsFreeToneCurve(rcurve->curve);
  cmsFreeToneCurve(rcurve->reverse);
  free(curve);
}

double
imcms_to_linearf(imcms_curve_t curve, double value) {
  imcms_curve_lcms2 *rcurve = (imcms_curve_lcms2 *)curve;

  return cmsEvalToneCurveFloat(rcurve->curve, value);
}

double
imcms_from_linearf(imcms_curve_t curve, double value) {
  imcms_curve_lcms2 *rcurve = (imcms_curve_lcms2 *)curve;

  return cmsEvalToneCurveFloat(rcurve->reverse, value);
}

static imcms_profile_t
build_profile(cmsHPROFILE profile) {
  imcms_profile_lcms2 *result = malloc(sizeof(imcms_profile_lcms2));

  if (!result) {
    cmsCloseProfile(profile);
    return NULL;
  }

  result->profile = profile;
  result->channels = cmsChannelsOf(cmsGetColorSpace(profile));
  result->curves = NULL;

  return result;
}

imcms_profile_t
imcms_srgb_profile(im_context_t ctx) {
  dCMSctx(ctx);

  return build_profile(cmsCreate_sRGBProfileTHR(cms_ctx));
}

static const cmsCIExyY srgb_white = { 0.3127, 0.3291, 1.0 };

imcms_profile_t
imcms_sgray_profile(im_context_t ctx) {
  cmsHPROFILE profile;
  cmsToneCurve *curve;
  dCMSctx(ctx);

  curve = cmsBuildParametricToneCurve(cms_ctx, 4, srgb_params);

  profile = cmsCreateGrayProfileTHR(cms_ctx, &srgb_white, curve);

  cmsFreeToneCurve(curve);

  return build_profile(profile);
}

void
imcms_free_profile(imcms_profile_t prof) {
  imcms_profile_lcms2 *profile = (imcms_profile_lcms2 *)prof;
  cmsCloseProfile(profile->profile);
  if (profile->curves) {
    int i;
    for (i = 0; i < profile->channels; ++i)
      imcms_free_curve(profile->curves[i]);
  }
  free(profile);
}

int
imcms_profile_curves(imcms_profile_t prof, imcms_curve_t *curves,
		     int count) {
  imcms_profile_lcms2 *profile = (imcms_profile_lcms2 *)prof;

  if (count != profile->channels)
    return 0;

  if (!profile->curves) {
    cmsColorSpaceSignature sig = cmsGetColorSpace(profile->profile);
    profile->curves = calloc(sizeof(imcms_curve_t), profile->channels);
    if (!profile->curves)
      return 0;

    if (sig == cmsSigRgbData) {
      profile->curves[0] = build_curve(cmsDupToneCurve(cmsReadTag(profile->profile, cmsSigRedTRCTag)));
      profile->curves[1] = build_curve(cmsDupToneCurve(cmsReadTag(profile->profile, cmsSigGreenTRCTag)));
      profile->curves[2] = build_curve(cmsDupToneCurve(cmsReadTag(profile->profile, cmsSigBlueTRCTag)));
    }
    else if (sig == cmsSigGrayData) {
      profile->curves[0] = build_curve(cmsDupToneCurve(cmsReadTag(profile->profile, cmsSigGrayTRCTag)));
    }
    else {
      i_push_error(0, "imcms_profile_curves: Only RGB or Gray supported");
      return 0;
    }
  }

  memcpy(curves, profile->curves, sizeof(imcms_curve_t) * count);

  return 1;
}
