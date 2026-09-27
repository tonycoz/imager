#ifndef IMAGER_IMINLINE_H
#define IMAGER_IMINLINE_H

static inline int
im_mult_overflow2(size_t *out, size_t sz1, size_t sz2) {
  *out = im_size_t_max;
  if (sz1 == 0 || sz2 == 0)
    return 1;
  size_t work_sz = sz1 * sz2;
  if (work_sz / sz1 != sz2)
    return 1;
  *out = work_sz;
  return 0;
}

static inline int
im_mult_overflow3(size_t *out, size_t sz1, size_t sz2, size_t sz3) {
  *out = im_size_t_max;
  if (sz1 == 0 || sz2 == 0 || sz3 == 0)
    return 1;
  size_t work_sz = sz1 * sz2 * sz3;
  if (work_sz / sz1 / sz2 != sz3)
    return 1;
  *out = work_sz;
  return 0;
}

static inline int
im_mult_overflow4(size_t *out, size_t sz1, size_t sz2, size_t sz3,
                   size_t sz4) {
  *out = im_size_t_max;
  if (sz1 == 0 || sz2 == 0 || sz3 == 0 || sz4 == 0)
    return 1;
  size_t work_sz = sz1 * sz2 * sz3 * sz4;
  if (work_sz / sz1 / sz2 / sz3 != sz4)
    return 1;
  *out = work_sz;
  return 0;
}

#endif
