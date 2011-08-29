#include "imager.h"
#include "iolayer.h"
#include "imerror.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef _MSC_VER
#include <io.h>
#endif
#include <string.h>
#include <errno.h>
#include "imageri.h"

#define IOL_DEB(x)


char *io_type_names[] = { "FDSEEK", "FDNOSEEK", "BUFFER", "CBSEEK", "CBNOSEEK", "BUFCHAIN" };

typedef struct io_blink {
  char buf[BBSIZ];
  /* size_t cnt; */
  size_t len;			/* How large is this buffer = BBZIS for now */
  struct io_blink *next;
  struct io_blink *prev;
} io_blink;


typedef struct {
  i_io_glue_t	base;
  int		fd;
} io_fdseek;

typedef struct {
  i_io_glue_t   base;
  char		*data;
  size_t	len;
  i_io_closebufp_t     closecb;        /* free memory mapped segment or decrement refcount */
  void          *closedata;
} io_buffer;

typedef struct {
  i_io_glue_t   base;
  void		*p;		/* Callback data */
  i_io_readl_t	readcb;
  i_io_writel_t	writecb;
  i_io_seekl_t	seekcb;
  i_io_closel_t closecb;
  i_io_destroyl_t      destroycb;
} io_cb;

#if 0
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
#endif

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

static void io_obj_setp_buffer(io_buffer *io, char *p, size_t len, i_io_closebufp_t closecb, void *closedata);
static void io_obj_setp_cb2     (io_cb *io, void *p, i_io_readl_t readcb, i_io_writel_t writecb, i_io_seekl_t seekcb, i_io_closel_t closecb, i_io_destroyl_t destroycb);

/* turn current offset, file length, whence and offset into a new offset */
#define calc_seek_offset(curr_off, length, offset, whence) \
  (((whence) == SEEK_SET) ? (offset) : \
   ((whence) == SEEK_CUR) ? (curr_off) + (offset) : \
   ((whence) == SEEK_END) ? (length) + (offset) : -1)

/*
=head1 NAME

iolayer.c - encapsulates different source of data into a single framework.

=head1 SYNOPSIS

  io_glue *ig = io_new_fd( fileno(stdin) );
  method = io_reqmeth( IOL_NOSEEK | IOL_MMAP ); // not implemented yet

  switch (method) {
  case IOL_NOSEEK:
    code that uses ig->readcb()
    to read data goes here.
    break;
  case IOL_MMAP:
    code that uses ig->readcb()
    to read data goes here.
    break;
  }  

  io_glue_destroy(ig);
  // and much more

=head1 DESCRIPTION

iolayer.c implements the basic functions to create and destroy io_glue
objects for Imager.  The typical usage pattern for data sources is:

   1. Create the source (io_new_fd)
   2. Define how you want to get data from it (io_reqmeth)
   3. read from it using the interface requested (ig->readdb, ig->mmapcb)
   4. Close the source, which 
      shouldn't really close the underlying source. (io_glue DESTROY)

=head1 FUNCTION REFERENCE

Some of these functions are internal.

=over

=cut
*/

static ssize_t fd_read(io_glue *ig, void *buf, size_t count);
static ssize_t fd_write(io_glue *ig, const void *buf, size_t count);
static off_t fd_seek(io_glue *ig, off_t offset, int whence);
static int fd_close(io_glue *ig);
static ssize_t fd_size(io_glue *ig);
static const char *my_strerror(int err);

/*
 * Callbacks for sources that cannot seek
 */

/* fakeseek_read: read method for when emulating a seekable source
static
ssize_t
fakeseek_read(io_glue *ig, void *buf, size_t count) {
  io_ex_fseek *exdata = ig->exdata; 
  return 0;
}
*/



/*
 * Callbacks for sources that can seek 
 */

/*
=item realseek_read(ig, buf, count)

Does the reading from a source that can be seeked on

   ig    - io_glue object
   buf   - buffer to return data in
   count - number of bytes to read into buffer max

=cut
*/

static
ssize_t 
realseek_read(io_glue *igo, void *buf, size_t count) {
  io_cb        *ig = (io_cb *)igo;
  io_ex_rseek *ier = igo->exdata;
  void *p          = ig->p;
  ssize_t       rc = 0;
  size_t        bc = 0;
  char       *cbuf = buf;

  IOL_DEB( printf("realseek_read:  buf = %p, count = %d\n", 
		  buf, count) );
  /* Is this a good idea? Would it be better to handle differently?
     skip handling? */
  while( count!=bc && (rc = ig->readcb(p,cbuf+bc,count-bc))>0 ) {
    bc+=rc;
  }
  
  ier->cpos += bc;
  IOL_DEB( printf("realseek_read: rc = %d, bc = %d\n", rc, bc) );
  return rc < 0 ? rc : bc;
}


/*
=item realseek_write(ig, buf, count)

Does the writing to a 'source' that can be seeked on

   ig    - io_glue object
   buf   - buffer that contains data
   count - number of bytes to write

=cut
*/

static
ssize_t 
realseek_write(io_glue *igo, const void *buf, size_t count) {
  io_cb        *ig = (io_cb *)igo;
  io_ex_rseek *ier = igo->exdata;
  void          *p = ig->p;
  ssize_t       rc = 0;
  size_t        bc = 0;
  char       *cbuf = (char*)buf; 
  
  IOL_DEB( printf("realseek_write: ig = %p, ier->cpos = %ld, buf = %p, "
                  "count = %d\n", ig, (long) ier->cpos, buf, count) );

  /* Is this a good idea? Would it be better to handle differently? 
     skip handling? */
  while( count!=bc && (rc = ig->writecb(p,cbuf+bc,count-bc))>0 ) {
    bc+=rc;
  }

  ier->cpos += bc;
  IOL_DEB( printf("realseek_write: rc = %d, bc = %d\n", rc, bc) );
  return rc < 0 ? rc : bc;
}


/*
=item realseek_close(ig)

Closes a source that can be seeked on.  Not sure if this should be an
actual close or not.  Does nothing for now.  Should be fixed.

   ig - data source

=cut */

static
int
realseek_close(io_glue *igo) {
  io_cb *ig = (io_cb *)igo;
  mm_log((1, "realseek_close(ig %p)\n", ig));
  if (ig->closecb)
    return ig->closecb(ig->p);
  else
    return 0;
}


/* realseek_seek(ig, offset, whence)

Implements seeking for a source that is seekable, the purpose of having this is to be able to
have an offset into a file that is different from what the underlying library thinks.

   ig     - data source
   offset - offset into stream
   whence - whence argument a la lseek

=cut
*/

static
off_t
realseek_seek(io_glue *igo, off_t offset, int whence) {
  /*  io_ex_rseek *ier = ig->exdata; Needed later */
  io_cb *ig = (io_cb *)igo;
  void *p = ig->p;
  off_t rc;
  IOL_DEB( printf("realseek_seek(ig %p, offset %ld, whence %d)\n", ig, (long) offset, whence) );
  rc = ig->seekcb(p, offset, whence);

  IOL_DEB( printf("realseek_seek: rc %ld\n", (long) rc) );
  return rc;
  /* FIXME: How about implementing this offset handling stuff? */
}

static
void
realseek_destroy(io_glue *igo) {
  io_cb *ig = (io_cb *)igo;
  io_ex_rseek *ier = igo->exdata;

  if (ig->destroycb)
    ig->destroycb(ig->p);

  myfree(ier);
}

/*
 * Callbacks for sources that are a fixed size buffer
 */

/*
=item buffer_read(ig, buf, count)

Does the reading from a buffer source

   ig    - io_glue object
   buf   - buffer to return data in
   count - number of bytes to read into buffer max

=cut
*/

static
ssize_t 
buffer_read(io_glue *igo, void *buf, size_t count) {
  io_buffer *ig = (io_buffer *)igo;
  io_ex_buffer *ieb = igo->exdata;

  IOL_DEB( printf("buffer_read: ieb->cpos = %ld, buf = %p, count = %d\n", (long) ieb->cpos, buf, count) );

  if ( ieb->cpos+count > ig->len ) {
    mm_log((1,"buffer_read: short read: cpos=%ld, len=%ld, count=%ld\n", (long)ieb->cpos, (long)ig->len, (long)count));
    count = ig->len - ieb->cpos;
  }
  
  memcpy(buf, ig->data+ieb->cpos, count);
  ieb->cpos += count;
  IOL_DEB( printf("buffer_read: count = %ld\n", (long)count) );
  return count;
}


/*
=item buffer_write(ig, buf, count)

Does nothing, returns -1

   ig    - io_glue object
   buf   - buffer that contains data
   count - number of bytes to write

=cut
*/

static
ssize_t 
buffer_write(io_glue *ig, const void *buf, size_t count) {
  mm_log((1, "buffer_write called, this method should never be called.\n"));
  return -1;
}


/*
=item buffer_close(ig)

Closes a source that can be seeked on.  Not sure if this should be an actual close
or not.  Does nothing for now.  Should be fixed.

   ig - data source

=cut
*/

static
int
buffer_close(io_glue *ig) {
  mm_log((1, "buffer_close(ig %p)\n", ig));

  return 0;
}


/* buffer_seek(ig, offset, whence)

Implements seeking for a buffer source.

   ig     - data source
   offset - offset into stream
   whence - whence argument a la lseek

=cut
*/

static
off_t
buffer_seek(io_glue *igo, off_t offset, int whence) {
  io_buffer *ig = (io_buffer *)igo;
  io_ex_buffer *ieb = igo->exdata;
  off_t reqpos = 
    calc_seek_offset(ieb->cpos, ig->len, offset, whence);
  
  if (reqpos > ig->len) {
    mm_log((1, "seeking out of readable range\n"));
    return (off_t)-1;
  }
  if (reqpos < 0) {
    i_push_error(0, "seek before beginning of file");
    return (off_t)-1;
  }
  
  ieb->cpos = reqpos;
  IOL_DEB( printf("buffer_seek(ig %p, offset %ld, whence %d)\n", ig, (long) offset, whence) );

  return reqpos;
  /* FIXME: How about implementing this offset handling stuff? */
}

static
void
buffer_destroy(io_glue *igo) {
  io_buffer *ig = (io_buffer *)igo;
  io_ex_buffer *ieb = igo->exdata;

  if (ig->closecb) {
    mm_log((1,"calling close callback %p for io_buffer\n", 
	    ig->closecb));
    ig->closecb(ig->closedata);
  }
  myfree(ieb);
}



/*
 * Callbacks for sources that are a chain of variable sized buffers
 */



/* Helper functions for buffer chains */

static
io_blink*
io_blink_new(void) {
  io_blink *ib;

  mm_log((1, "io_blink_new()\n"));

  ib = mymalloc(sizeof(io_blink));

  ib->next = NULL;
  ib->prev = NULL;
  ib->len  = BBSIZ;

  memset(&ib->buf, 0, ib->len);
  return ib;
}



/*
=item io_bchain_advance(ieb)

Advances the buffer chain to the next link - extending if
necessary.  Also adjusts the cpos and tfill counters as needed.

   ieb   - buffer chain object

=cut
*/

static
void
io_bchain_advance(io_ex_bchain *ieb) {
  if (ieb->cp->next == NULL) {
    ieb->tail = io_blink_new();
    ieb->tail->prev = ieb->cp;
    ieb->cp->next   = ieb->tail;

    ieb->tfill = 0; /* Only set this if we added a new slice */
  }
  ieb->cp    = ieb->cp->next;
  ieb->cpos  = 0;
}



/*
=item io_bchain_destroy()

frees all resources used by a buffer chain.

=cut
*/

void
io_destroy_bufchain(io_ex_bchain *ieb) {
  io_blink *cp;
  mm_log((1, "io_destroy_bufchain(ieb %p)\n", ieb));
  cp = ieb->head;
  
  while(cp) {
    io_blink *t = cp->next;
    myfree(cp);
    cp = t;
  }
}




/*

static
void
bufchain_dump(io_ex_bchain *ieb) {
  mm_log((1, "  buf_chain_dump(ieb %p)\n"));
  mm_log((1, "  buf_chain_dump: ieb->offset = %d\n", ieb->offset));
  mm_log((1, "  buf_chain_dump: ieb->length = %d\n", ieb->length));
  mm_log((1, "  buf_chain_dump: ieb->head   = %p\n", ieb->head  ));
  mm_log((1, "  buf_chain_dump: ieb->tail   = %p\n", ieb->tail  ));
  mm_log((1, "  buf_chain_dump: ieb->tfill  = %d\n", ieb->tfill ));
  mm_log((1, "  buf_chain_dump: ieb->cp     = %p\n", ieb->cp    ));
  mm_log((1, "  buf_chain_dump: ieb->cpos   = %d\n", ieb->cpos  ));
  mm_log((1, "  buf_chain_dump: ieb->gpos   = %d\n", ieb->gpos  ));
}
*/

/*
 * TRUE if lengths are NOT equal
 */

/*
static
void
chainlencert( io_glue *ig ) {
  int clen;
  int cfl           = 0;
  size_t csize      = 0;
  size_t cpos       = 0;
  io_ex_bchain *ieb = ig->exdata;
  io_blink *cp      = ieb->head;
  

  if (ieb->gpos > ieb->length) mm_log((1, "BBAR : ieb->gpos = %d, ieb->length = %d\n", ieb->gpos, ieb->length));

  while(cp) {
    clen = (cp == ieb->tail) ? ieb->tfill : cp->len;
    if (ieb->head == cp && cp->prev) mm_log((1, "Head of chain has a non null prev\n"));
    if (ieb->tail == cp && cp->next) mm_log((1, "Tail of chain has a non null next\n"));
    
    if (ieb->head != cp && !cp->prev) mm_log((1, "Middle of chain has a null prev\n"));
    if (ieb->tail != cp && !cp->next) mm_log((1, "Middle of chain has a null next\n"));
    
    if (cp->prev && cp->prev->next != cp) mm_log((1, "%p = cp->prev->next != cp\n", cp->prev->next));
    if (cp->next && cp->next->prev != cp) mm_log((1, "%p cp->next->prev != cp\n", cp->next->prev));

    if (cp == ieb->cp) {
      cfl = 1;
      cpos += ieb->cpos;
    }

    if (!cfl) cpos += clen;

    csize += clen;
    cp     = cp->next;
  }
  if (( csize != ieb->length )) mm_log((1, "BAR : csize = %d, ieb->length = %d\n", csize, ieb->length));
  if (( cpos  != ieb->gpos   )) mm_log((1, "BAR : cpos  = %d, ieb->gpos   = %d\n", cpos,  ieb->gpos  ));
}


static
void
chaincert( io_glue *ig) {
  size_t csize   = 0;
  io_ex_bchain *ieb = ig->exdata;
  io_blink *cp   = ieb->head;
  
  mm_log((1, "Chain verification.\n"));

  mm_log((1, "  buf_chain_dump: ieb->offset = %d\n", ieb->offset));
  mm_log((1, "  buf_chain_dump: ieb->length = %d\n", ieb->length));
  mm_log((1, "  buf_chain_dump: ieb->head   = %p\n", ieb->head  ));
  mm_log((1, "  buf_chain_dump: ieb->tail   = %p\n", ieb->tail  ));
  mm_log((1, "  buf_chain_dump: ieb->tfill  = %d\n", ieb->tfill ));
  mm_log((1, "  buf_chain_dump: ieb->cp     = %p\n", ieb->cp    ));
  mm_log((1, "  buf_chain_dump: ieb->cpos   = %d\n", ieb->cpos  ));
  mm_log((1, "  buf_chain_dump: ieb->gpos   = %d\n", ieb->gpos  ));

  while(cp) {
    int clen = cp == ieb->tail ? ieb->tfill : cp->len;
    mm_log((1, "link: %p <- %p -> %p\n", cp->prev, cp, cp->next));
    if (ieb->head == cp && cp->prev) mm_log((1, "Head of chain has a non null prev\n"));
    if (ieb->tail == cp && cp->next) mm_log((1, "Tail of chain has a non null next\n"));
    
    if (ieb->head != cp && !cp->prev) mm_log((1, "Middle of chain has a null prev\n"));
    if (ieb->tail != cp && !cp->next) mm_log((1, "Middle of chain has a null next\n"));
    
    if (cp->prev && cp->prev->next != cp) mm_log((1, "%p = cp->prev->next != cp\n", cp->prev->next));
    if (cp->next && cp->next->prev != cp) mm_log((1, "%p cp->next->prev != cp\n", cp->next->prev));

    csize += clen;
    cp     = cp->next;
  }

  mm_log((1, "csize = %d %s ieb->length = %d\n", csize, csize == ieb->length ? "==" : "!=", ieb->length));
}
*/











/*
=item bufchain_read(ig, buf, count)

Does the reading from a source that can be seeked on

   ig    - io_glue object
   buf   - buffer to return data in
   count - number of bytes to read into buffer max

=cut
*/

static
ssize_t 
bufchain_read(io_glue *ig, void *buf, size_t count) {
  io_ex_bchain *ieb = ig->exdata;
  size_t     scount = count;
  char        *cbuf = buf;
  size_t         sk;

  mm_log((1, "bufchain_read(ig %p, buf %p, count %ld)\n", ig, buf, (long)count));

  while( scount ) {
    int clen = (ieb->cp == ieb->tail) ? ieb->tfill : ieb->cp->len;
    if (clen == ieb->cpos) {
      if (ieb->cp == ieb->tail) break; /* EOF */
      ieb->cp = ieb->cp->next;
      ieb->cpos = 0;
      clen = (ieb->cp == ieb->tail) ? ieb->tfill : ieb->cp->len;
    }

    sk = clen - ieb->cpos;
    sk = sk > scount ? scount : sk;

    memcpy(&cbuf[count-scount], &ieb->cp->buf[ieb->cpos], sk);
    scount    -= sk;
    ieb->cpos += sk;
    ieb->gpos += sk;
  }

  mm_log((1, "bufchain_read: returning %ld\n", (long)(count-scount)));
  return count-scount;
}





/*
=item bufchain_write(ig, buf, count)

Does the writing to a 'source' that can be seeked on

   ig    - io_glue object
   buf   - buffer that contains data
   count - number of bytes to write

=cut
*/

static
ssize_t
bufchain_write(io_glue *ig, const void *buf, size_t count) {
  char *cbuf = (char *)buf;
  io_ex_bchain *ieb = ig->exdata;
  size_t         ocount = count;
  size_t         sk;

  mm_log((1, "bufchain_write: ig = %p, buf = %p, count = %ld\n", ig, buf, (long)count));

  IOL_DEB( printf("bufchain_write: ig = %p, ieb->cpos = %ld, buf = %p, count = %ld\n", ig, (long) ieb->cpos, buf, (long)count) );
  
  while(count) {
    mm_log((2, "bufchain_write: - looping - count = %ld\n", (long)count));
    if (ieb->cp->len == ieb->cpos) {
      mm_log((1, "bufchain_write: cp->len == ieb->cpos = %ld - advancing chain\n", (long) ieb->cpos));
      io_bchain_advance(ieb);
    }

    sk = ieb->cp->len - ieb->cpos;
    sk = sk > count ? count : sk;
    memcpy(&ieb->cp->buf[ieb->cpos], &cbuf[ocount-count], sk);

    if (ieb->cp == ieb->tail) {
      int extend = ieb->cpos + sk - ieb->tfill;
      mm_log((2, "bufchain_write: extending tail by %d\n", extend));
      if (extend > 0) {
	ieb->length += extend;
	ieb->tfill  += extend;
      }
    }

    ieb->cpos += sk;
    ieb->gpos += sk;
    count     -= sk;
  }
  return ocount;
}

/*
=item bufchain_close(ig)

Closes a source that can be seeked on.  Not sure if this should be an actual close
or not.  Does nothing for now.  Should be fixed.

   ig - data source

=cut
*/

static
int
bufchain_close(io_glue *ig) {
  mm_log((1, "bufchain_close(ig %p)\n",ig));
  IOL_DEB( printf("bufchain_close(ig %p)\n", ig) );

  return 0;  
}


/* bufchain_seek(ig, offset, whence)

Implements seeking for a source that is seekable, the purpose of having this is to be able to
have an offset into a file that is different from what the underlying library thinks.

   ig     - data source
   offset - offset into stream
   whence - whence argument a la lseek

=cut
*/

static
off_t
bufchain_seek(io_glue *ig, off_t offset, int whence) {
  io_ex_bchain *ieb = ig->exdata;
  int wrlen;

  off_t scount = calc_seek_offset(ieb->gpos, ieb->length, offset, whence);
  off_t sk;

  mm_log((1, "bufchain_seek(ig %p, offset %ld, whence %d)\n", ig, (long)offset, whence));

  if (scount < 0) {
    i_push_error(0, "invalid whence supplied or seek before start of file");
    return (off_t)-1;
  }

  ieb->cp   = ieb->head;
  ieb->cpos = 0;
  ieb->gpos = 0;
  
  while( scount ) {
    int clen = (ieb->cp == ieb->tail) ? ieb->tfill : ieb->cp->len;
    if (clen == ieb->cpos) {
      if (ieb->cp == ieb->tail) break; /* EOF */
      ieb->cp = ieb->cp->next;
      ieb->cpos = 0;
      clen = (ieb->cp == ieb->tail) ? ieb->tfill : ieb->cp->len;
    }
    
    sk = clen - ieb->cpos;
    sk = sk > scount ? scount : sk;
    
    scount    -= sk;
    ieb->cpos += sk;
    ieb->gpos += sk;
  }
  
  wrlen = scount;

  if (wrlen > 0) { 
    /*
     * extending file - get ieb into consistent state and then
     * call write which will get it to the correct position 
     */
    char TB[BBSIZ];
    memset(TB, 0, BBSIZ);
    ieb->gpos = ieb->length;
    ieb->cpos = ieb->tfill;
    
    while(wrlen > 0) {
      ssize_t rc, wl = i_min(wrlen, BBSIZ);
      mm_log((1, "bufchain_seek: wrlen = %d, wl = %ld\n", wrlen, (long)wl));
      rc = bufchain_write( ig, TB, wl );
      if (rc != wl) i_fatal(0, "bufchain_seek: Unable to extend file\n");
      wrlen -= rc;
    }
  }

  mm_log((2, "bufchain_seek: returning ieb->gpos = %ld\n", (long)ieb->gpos));
  return ieb->gpos;
}

static
void
bufchain_destroy(io_glue *ig) {
  io_ex_bchain *ieb = ig->exdata;

  io_destroy_bufchain(ieb);

  myfree(ieb);
}

/*
 * Methods for setting up data source
 */

/*
=item io_obj_setp_buffer(io, p, len)

Sets an io_object for reading from a buffer source

   io  - io object that describes a source
   p   - pointer to buffer
   len - length of buffer

=cut
*/

static void
io_obj_setp_buffer(io_buffer *io, char *p, size_t len, i_io_closebufp_t closecb, 
		   void *closedata) {
  io->base.type      = BUFFER;
  io->data      = p;
  io->len       = len;
  io->closecb   = closecb;
  io->closedata = closedata;
}



/*
=item io_obj_setp_cb2(io, p, readcb, writecb, seekcb, closecb, destroycb)

Sets an io_object for reading from a source that uses callbacks

   io      - io object that describes a source
   p         - pointer to data for callbacks
   readcb    - read callback to read from source
   writecb   - write callback to write to source
   seekcb    - seek callback to seek on source
   closecb   - flush any pending data
   destroycb - release any extra resources

=cut
*/

static void
io_obj_setp_cb2(io_cb *io, void *p, i_io_readl_t readcb, i_io_writel_t writecb, i_io_seekl_t seekcb, i_io_closel_t closecb, i_io_destroyl_t destroycb) {
  io->base.type      = CBSEEK;
  io->p         = p;
  io->readcb    = readcb;
  io->writecb   = writecb;
  io->seekcb    = seekcb;
  io->closecb   = closecb;
  io->destroycb = destroycb;
}

/*
=item io_new_bufchain()

returns a new io_glue object that has the 'empty' source and but can
be written to and read from later (like a pseudo file).

=cut
*/

io_glue *
io_new_bufchain() {
  io_glue *ig;
  io_ex_bchain *ieb = mymalloc(sizeof(io_ex_bchain));

  mm_log((1, "io_new_bufchain()\n"));

  ig = mymalloc(sizeof(io_glue));
  memset(ig, 0, sizeof(*ig));
  ig->type = BUFCHAIN;

  ieb->offset = 0;
  ieb->length = 0;
  ieb->cpos   = 0;
  ieb->gpos   = 0;
  ieb->tfill  = 0;
  
  ieb->head   = io_blink_new();
  ieb->cp     = ieb->head;
  ieb->tail   = ieb->head;
  
  ig->exdata    = ieb;
  ig->readcb    = bufchain_read;
  ig->writecb   = bufchain_write;
  ig->seekcb    = bufchain_seek;
  ig->closecb   = bufchain_close;
  ig->destroycb = bufchain_destroy;

  return ig;
}

/*
=item io_new_buffer(data, len)

Returns a new io_glue object that has the source defined as reading
from specified buffer.  Note that the buffer is not copied.

   data - buffer to read from
   len - length of buffer

=cut
*/

io_glue *
io_new_buffer(char *data, size_t len, i_io_closebufp_t closecb, void *closedata) {
  io_buffer *ig;
  io_ex_buffer *ieb = mymalloc(sizeof(io_ex_buffer));
  
  mm_log((1, "io_new_buffer(data %p, len %ld, closecb %p, closedata %p)\n", data, (long)len, closecb, closedata));

  ig = mymalloc(sizeof(io_buffer));
  memset(ig, 0, sizeof(*ig));
  io_obj_setp_buffer(ig, data, len, closecb, closedata);
  ieb->offset = 0;
  ieb->cpos   = 0;
  
  ig->base.exdata    = ieb;
  ig->base.readcb    = buffer_read;
  ig->base.writecb   = buffer_write;
  ig->base.seekcb    = buffer_seek;
  ig->base.closecb   = buffer_close;
  ig->base.destroycb = buffer_destroy;

  return (io_glue *)ig;
}


/*
=item io_new_fd(fd)

returns a new io_glue object that has the source defined as reading
from specified filedescriptor.  Note that the the interface to recieving
data from the io_glue callbacks hasn't been done yet.

   fd - file descriptor to read/write from

=cut
*/

io_glue *
io_new_fd(int fd) {
  io_fdseek *ig;

  mm_log((1, "io_new_fd(fd %d)\n", fd));

  ig = mymalloc(sizeof(io_fdseek));
  memset(ig, 0, sizeof(*ig));
  ig->base.type = FDSEEK;
  ig->fd = fd;

  ig->base.exdata    = NULL;
  ig->base.readcb    = fd_read;
  ig->base.writecb   = fd_write;
  ig->base.seekcb    = fd_seek;
  ig->base.closecb   = fd_close;
  ig->base.sizecb    = fd_size;
  ig->base.destroycb = NULL;

  mm_log((1, "(%p) <- io_new_fd\n", ig));
  return (io_glue *)ig;
}

io_glue *io_new_cb(void *p, i_io_readl_t readcb, i_io_writel_t writecb, 
		   i_io_seekl_t seekcb, i_io_closel_t closecb, 
		   i_io_destroyl_t destroycb) {
  io_cb *ig;
  io_ex_rseek *ier = mymalloc(sizeof(io_ex_rseek));

  mm_log((1, "io_new_cb(p %p, readcb %p, writecb %p, seekcb %p, closecb %p, "
          "destroycb %p)\n", p, readcb, writecb, seekcb, closecb, destroycb));
  ig = mymalloc(sizeof(io_cb));
  memset(ig, 0, sizeof(*ig));
  io_obj_setp_cb2(ig, p, readcb, writecb, seekcb, closecb, destroycb);
  mm_log((1, "(%p) <- io_new_cb\n", ig));

  ier->offset = 0;
  ier->cpos   = 0;
  
  ig->base.exdata    = ier;
  ig->base.readcb    = realseek_read;
  ig->base.writecb   = realseek_write;
  ig->base.seekcb    = realseek_seek;
  ig->base.closecb   = realseek_close;
  ig->base.destroycb = realseek_destroy;

  return (io_glue *)ig;
}

/*
=item io_slurp(ig)

Takes the source that the io_glue is bound to and allocates space
for a return buffer and returns the entire content in a single buffer.
Note: This only works for io_glue objects that contain a bufchain.  It
is usefull for saving to scalars and such.

   ig - io_glue object
   c  - pointer to a pointer to where data should be copied to

=cut
*/

size_t
io_slurp(io_glue *ig, unsigned char **c) {
  ssize_t rc;
  off_t orgoff;
  io_ex_bchain *ieb;
  unsigned char *cc;
  io_type inn = ig->type;
  
  if ( inn != BUFCHAIN ) {
    i_fatal(0, "io_slurp: called on a source that is not from a bufchain\n");
  }

  ieb = ig->exdata;
  cc = *c = mymalloc( ieb->length );
  
  orgoff = ieb->gpos;
  
  bufchain_seek(ig, 0, SEEK_SET);
  
  rc = bufchain_read(ig, cc, ieb->length);

  if (rc != ieb->length)
    i_fatal(1, "io_slurp: bufchain_read returned an incomplete read: rc = %d, request was %d\n", rc, ieb->length);

  return rc;
}

/*
=item fd_read(ig, buf, count)

=cut
*/
static ssize_t fd_read(io_glue *igo, void *buf, size_t count) {
  io_fdseek *ig = (io_fdseek *)igo;
  ssize_t result;
#ifdef _MSC_VER
  result = _read(ig->fd, buf, count);
#else
  result = read(ig->fd, buf, count);
#endif

  /* 0 is valid - means EOF */
  if (result < 0) {
    i_push_errorf(0, "read() failure: %s (%d)", my_strerror(errno), errno);
  }

  return result;
}

static ssize_t fd_write(io_glue *igo, const void *buf, size_t count) {
  io_fdseek *ig = (io_fdseek *)igo;
  ssize_t result;
#ifdef _MSC_VER
  result = _write(ig->fd, buf, count);
#else
  result = write(ig->fd, buf, count);
#endif

  if (result <= 0) {
    i_push_errorf(errno, "write() failure: %s (%d)", my_strerror(errno), errno);
  }

  return result;
}

static off_t fd_seek(io_glue *igo, off_t offset, int whence) {
  io_fdseek *ig = (io_fdseek *)igo;
  off_t result;
#ifdef _MSC_VER
  result = _lseek(ig->fd, offset, whence);
#else
  result = lseek(ig->fd, offset, whence);
#endif

  if (result == (off_t)-1) {
    i_push_errorf(errno, "lseek() failure: %s (%d)", my_strerror(errno), errno);
  }

  return result;
}

static int fd_close(io_glue *ig) {
  /* no, we don't close it */
  return 0;
}

static ssize_t fd_size(io_glue *ig) {
  mm_log((1, "fd_size(ig %p) unimplemented\n", ig));
  
  return -1;
}

/*
=item io_glue_destroy(ig)

A destructor method for io_glue objects.  Should clean up all related buffers.
Might leave us with a dangling pointer issue on some buffers.

   ig - io_glue object to destroy.

=cut
*/

void
io_glue_destroy(io_glue *ig) {
  mm_log((1, "io_glue_DESTROY(ig %p)\n", ig));

  if (ig->destroycb)
    ig->destroycb(ig);
  
  myfree(ig);
}

/*
=back

=head1 INTERNAL FUNCTIONS

=over

=item my_strerror

Calls strerror() and ensures we don't return NULL.

On some platforms it's possible for strerror() to return NULL, this
wrapper ensures we only get non-NULL values.

=cut
*/

static
const char *my_strerror(int err) {
  const char *result = strerror(err);

  if (!result)
    result = "Unknown error";

  return result;
}

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>

=head1 SEE ALSO

Imager(3)

=cut
*/
