#ifndef IMAGER_IMCMST_H
#define IMAGER_IMCMST_H

struct imcms_curve_base_struct {
  i_sample16_t *to_linear;
  unsigned char *from_linear;
};

typedef struct imcms_curve_base_struct *imcms_curve_t;

typedef struct imcms_profile_struct *imcms_profile_t;

typedef struct imcms_reader_struct *imcms_reader_t;

#define imcms_to_linear(curve, value) \
  ((curve)->to_linear[(unsigned char)(value)])
#define imcms_from_linear(curve, value) \
  ((curve)->from_linear[(i_sample16_t)(value)])

#endif
