#ifndef _IOLAYER_H_
#define _IOLAYER_H_


/* How the IO layer works:
 * 
 * Start by getting an io_glue object.  Then define its
 * datasource via io_obj_setp_buffer or io_obj_setp_cb.  Before
 * using the io_glue object be sure to call io_glue_commit_types().
 * After that data can be read via the io_glue->readcb() method.
 *
 */


#include "iolayert.h"

/* #define BBSIZ 1096 */
#define BBSIZ 16384
#define IO_FAKE_SEEK 1<<0L
#define IO_TEMP_SEEK 1<<1L


void io_glue_commit_types(io_glue *ig);
void io_glue_gettypes    (io_glue *ig, int reqmeth);

/* XS functions */
io_glue *io_new_fd(int fd);
io_glue *io_new_bufchain(void);
io_glue *io_new_buffer(char *data, size_t len, i_io_closebufp_t closecb, void *closedata);
io_glue *io_new_cb(void *p, i_io_readl_t readcb, i_io_writel_t writecb, i_io_seekl_t seekcb, i_io_closel_t closecb, i_io_destroyl_t destroycb);
size_t   io_slurp(io_glue *ig, unsigned char **c);
void     io_glue_destroy(io_glue *ig);

#endif /* _IOLAYER_H_ */
