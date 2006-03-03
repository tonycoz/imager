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


#include <stdio.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <sys/types.h>

/* #define BBSIZ 1096 */
#define BBSIZ 16384
#define IO_FAKE_SEEK 1<<0L
#define IO_TEMP_SEEK 1<<1L


typedef union { int i; void *p; } iorp;

typedef enum { FDSEEK, FDNOSEEK, BUFFER, CBSEEK, CBNOSEEK, BUFCHAIN } io_type;

#ifdef _MSC_VER
typedef int ssize_t;
#endif

struct io_glue;

/* Callbacks we give out */

typedef ssize_t(*readp) (struct io_glue *ig, void *buf, size_t count);
typedef ssize_t(*writep)(struct io_glue *ig, const void *buf, size_t count);
typedef off_t  (*seekp) (struct io_glue *ig, off_t offset, int whence);
typedef void   (*closep)(struct io_glue *ig);
typedef ssize_t(*sizep) (struct io_glue *ig);

typedef void   (*closebufp)(void *p);


/* Callbacks we get */

typedef ssize_t(*readl) (void *p, void *buf, size_t count);
typedef ssize_t(*writel)(void *p, const void *buf, size_t count);
typedef off_t  (*seekl) (void *p, off_t offset, int whence);
typedef void   (*closel)(void *p);
typedef void   (*destroyl)(void *p);
typedef ssize_t(*sizel) (void *p);

extern char *io_type_names[];


typedef struct io_blink {
  char buf[BBSIZ];
  /* size_t cnt; */
  size_t len;			/* How large is this buffer = BBZIS for now */
  struct io_blink *next;
  struct io_blink *prev;
} io_blink;


/* Structures that describe callback interfaces */

typedef struct {
  off_t offset;
  off_t cpos;
} io_ex_rseek;


typedef struct {
  off_t offset;
  off_t cpos;
  io_blink *head;
  io_blink *tail;
  io_blink *cp;
} io_ex_fseek;


typedef struct {
  off_t offset;			/* Offset of the source - not used */
  off_t length;			/* Total length of chain in bytes */
  io_blink *head;		/* Start of chain */
  io_blink *tail;		/* End of chain */
  off_t tfill;			/* End of stream in last link */
  io_blink *cp;			/* Current element of list */
  off_t cpos;			/* Offset within the current */
  off_t gpos;			/* Global position in stream */
} io_ex_bchain;

typedef struct {
  off_t offset;			/* Offset of the source - not used */
  off_t cpos;			/* Offset within the current */
} io_ex_buffer;



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
  closebufp     closecb;        /* free memory mapped segment or decrement refcount */
  void          *closedata;
} io_buffer;

typedef struct {
  io_type	type;		/* Must be first parameter */
  char		*name;		/* Data source name */
  void		*p;		/* Callback data */
  readl		readcb;
  writel	writecb;
  seekl		seekcb;
  closel        closecb;
  destroyl      destroycb;
} io_cb;

typedef union {
  io_type       type;
  io_fdseek     fdseek;
  io_buffer	buffer;
  io_cb		cb;
} io_obj;

typedef struct io_glue {
  io_obj	source;
  int		flags;		/* Flags */
  void		*exdata;	/* Pair specific data */
  readp		readcb;
  writep	writecb;
  seekp		seekcb;
  closep	closecb;
  sizep		sizecb;
} io_glue;

void io_obj_setp_buffer(io_obj *io, char *p, size_t len, closebufp closecb, void *closedata);
void io_obj_setp_cb      (io_obj *io, void *p, readl readcb, writel writecb, seekl seekcb);
void io_obj_setp_cb2     (io_obj *io, void *p, readl readcb, writel writecb, seekl seekcb, closel closecb, destroyl destroycb);
void io_glue_commit_types(io_glue *ig);
void io_glue_gettypes    (io_glue *ig, int reqmeth);


/* XS functions */
io_glue *io_new_fd(int fd);
io_glue *io_new_bufchain(void);
io_glue *io_new_buffer(char *data, size_t len, closebufp closecb, void *closedata);
io_glue *io_new_cb(void *p, readl readcb, writel writecb, seekl seekcb, closel closecb, destroyl destroycb);
size_t   io_slurp(io_glue *ig, unsigned char **c);
void     io_glue_DESTROY(io_glue *ig);

#endif /* _IOLAYER_H_ */
