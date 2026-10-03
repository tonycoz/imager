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

static inline io_type
i_io_type(io_glue *ig) {
  return ig->type;
}

static inline ssize_t
i_io_raw_read(io_glue *ig, void *buf, size_t size) {
  return ig->vtbl->readcb(ig, buf, size);
}

static inline ssize_t
i_io_raw_write(io_glue *ig, const void *data, size_t size) {
  return ig->vtbl->writecb(ig, data, size);
}

static inline off_t
i_io_raw_seek(io_glue *ig, off_t offset, int whence) {
  return ig->vtbl->seekcb(ig, offset, whence);
}

static inline int
i_io_raw_close(io_glue *ig) {
  return ig->vtbl->closecb(ig);
}

static inline int
i_io_is_buffered(io_glue *ig) {
  return ig->buffered;
}

static inline off_t
i_io_size(io_glue *ig) {
  return ig->vtbl->sizecb ? ig->vtbl->sizecb(ig) : (off_t)-1;
}

static inline int
i_io_mmap(io_glue *ig, const void **pdata, size_t *psize) {
  return ig->vtbl->mmapcb ? ig->vtbl->mmapcb(ig, pdata, psize) : 0;
}

static inline int
i_io_munmap(io_glue *ig) {
  return ig->vtbl->munmapcb ? ig->vtbl->munmapcb(ig) : 0;
}


#endif
