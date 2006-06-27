#ifndef IMAGER_IOLAYERT_H
#define IMAGER_IOLAYERT_H

#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <sys/types.h>
#include <stddef.h>

typedef enum { FDSEEK, FDNOSEEK, BUFFER, CBSEEK, CBNOSEEK, BUFCHAIN } io_type;

#ifdef _MSC_VER
typedef int ssize_t;
#endif

typedef struct i_io_glue_t i_io_glue_t;

/* compatibility for now */
typedef i_io_glue_t io_glue;

/* Callbacks we give out */

typedef ssize_t(*i_io_readp_t) (io_glue *ig, void *buf, size_t count);
typedef ssize_t(*i_io_writep_t)(io_glue *ig, const void *buf, size_t count);
typedef off_t  (*i_io_seekp_t) (io_glue *ig, off_t offset, int whence);
typedef int    (*i_io_closep_t)(io_glue *ig);
typedef ssize_t(*i_io_sizep_t) (io_glue *ig);

typedef void   (*i_io_closebufp_t)(void *p);
typedef void (*i_io_destroyp_t)(i_io_glue_t *ig);


/* Callbacks we get */

typedef ssize_t(*i_io_readl_t) (void *p, void *buf, size_t count);
typedef ssize_t(*i_io_writel_t)(void *p, const void *buf, size_t count);
typedef off_t  (*i_io_seekl_t) (void *p, off_t offset, int whence);
typedef int    (*i_io_closel_t)(void *p);
typedef void   (*i_io_destroyl_t)(void *p);
typedef ssize_t(*i_io_sizel_t) (void *p);

extern char *io_type_names[];



/* Structures to describe data sources */

typedef struct {
  io_type	type;
  int		fd;
} io_fdseek;

typedef struct {
  io_type	type;		/* Must be first parameter */
  char		*name;		/* Data source name */
  char		*data;
  size_t	len;
  i_io_closebufp_t     closecb;        /* free memory mapped segment or decrement refcount */
  void          *closedata;
} io_buffer;

typedef struct {
  io_type	type;		/* Must be first parameter */
  char		*name;		/* Data source name */
  void		*p;		/* Callback data */
  i_io_readl_t	readcb;
  i_io_writel_t	writecb;
  i_io_seekl_t	seekcb;
  i_io_closel_t closecb;
  i_io_destroyl_t      destroycb;
} io_cb;

typedef union {
  io_type       type;
  io_fdseek     fdseek;
  io_buffer	buffer;
  io_cb		cb;
} io_obj;

struct i_io_glue_t {
  io_obj	source;
  int		flags;		/* Flags */
  void		*exdata;	/* Pair specific data */
  i_io_readp_t	readcb;
  i_io_writep_t	writecb;
  i_io_seekp_t	seekcb;
  i_io_closep_t	closecb;
  i_io_sizep_t	sizecb;
  i_io_destroyp_t destroycb;
};

#define i_io_type(ig) ((ig)->source.ig_type)
#define i_io_read(ig, buf, size) ((ig)->readcb((ig), (buf), (size)))
#define i_io_write(ig, data, size) ((ig)->writecb((ig), (data), (size)))
#define i_io_seek(ig, offset, whence) ((ig)->seekcb((ig), (offset), (whence)))
#define i_io_close(ig) ((ig)->closecb(ig))


#endif
