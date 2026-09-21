#define IMAGER_NO_CONTEXT
#include "imager.h"
#include "imageri.h"
#include <stdlib.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

/*
=item mymalloc(size)
=category Memory Management

Allocate a block of C<size> bytes of memory.

exit()s on failure.

Always uses im_get_context() to fetch the current Imager context.

=cut
*/

void *
mymalloc(size_t size) {
  dIMCTX;
  return i_malloc(size);
}

/*
=item im_malloc(aIMCTX, size)
=category Memory Management
X<i_malloc>

Allocate a block of C<size> bytes of memory.

exit()s on failure.

Callable as C<i_malloc(size)>, and will use the local IMCTX under
IMAGER_NO_CONTEXT.

=cut
*/

void*
im_malloc(pIMCTX, size_t size) {
  void *buf;

  if ( (buf = malloc(size)) == NULL ) {
    im_log((aIMCTX, 1, "mymalloc: unable to malloc %ld\n", (long)size));
    fprintf(stderr,"Unable to malloc %ld.\n", (long)size); exit(3);
  }
  im_log((aIMCTX, 1, "mymalloc(size %ld) -> %p\n", (long)size, buf));
  return buf;
}

/*
=item myfree(p)
=category Memory Management

Release the memory block C<p> points at.

This is not suitable for memory allocated by perl itself.

Always uses im_get_context() to fetch the current Imager context.

=cut
*/

void
myfree(void *p) {
  dIMCTX;
  i_free(p);
}

/*
=item im_free(aIMCTX, p)
=category Memory Management

Release the memory block C<p> points at.

This is not suitable for memory allocated by perl itself.

Callable as i_free(p), and will use the local IMCTX under
IMAGER_NO_CONTEXT.

=cut
*/

void
im_free(pIMCTX, void *p) {
  im_log((aIMCTX, 1, "myfree(p %p)\n", p));
  free(p);
}

/*
=item myrealloc(p, size)
=category Memory Management

Resize the block C<p> to C<size> bytes of memory.

exit()s on failure.

Always uses im_get_context() to fetch the current Imager context.

=cut
*/

void *
myrealloc(void *block, size_t size) {
  dIMCTX;
  return i_realloc(block, size);
}

/*
=item im_realloc(aIMCTX, p, size)
=category Memory Management

Resize the block C<p> to C<size> bytes of memory.

exit()s on failure.

Callable as C<i_realloc(p, size)>, and will use the local IMCTX under
IMAGER_NO_CONTEXT.

=cut
*/

void *
im_realloc(pIMCTX, void *block, size_t size) {
  void *result;

  im_log((aIMCTX, 1, "myrealloc(block %p, size %ld)\n", block, (long)size));
  if ((result = realloc(block, size)) == NULL) {
    im_log((aIMCTX, 1, "myrealloc: out of memory\n"));
    fprintf(stderr, "Out of memory.\n");
    exit(3);
  }
  return result;
}

/* memory pool implementation */

void
i_mempool_init(i_mempool *mp) {
  mp->alloc = 10;
  mp->used  = 0;
  mp->p = mymalloc(sizeof(void*)*mp->alloc);
}

void
i_mempool_extend(i_mempool *mp) {
  mp->p = myrealloc(mp->p, mp->alloc * 2);
  mp->alloc *=2;
}

void *
i_mempool_alloc(i_mempool *mp, size_t size) {
  if (mp->used == mp->alloc) i_mempool_extend(mp);
  mp->p[mp->used] = mymalloc(size);
  mp->used++;
  return mp->p[mp->used-1];
}


void
i_mempool_destroy(i_mempool *mp) {
  unsigned int i;
  for(i=0; i<mp->used; i++) myfree(mp->p[i]);
  myfree(mp->p);
}



/* Should these really be here? */

#undef min
#undef max

i_img_dim
i_minx(i_img_dim a, i_img_dim b) {
  if (a<b) return a; else return b;
}

i_img_dim
i_maxx(i_img_dim a, i_img_dim b) {
  if (a>b) return a; else return b;
}


struct utf8_size {
  int mask, expect;
  int size;
};

struct utf8_size utf8_sizes[] =
{
  { 0x80, 0x00, 1 },
  { 0xE0, 0xC0, 2 },
  { 0xF0, 0xE0, 3 },
  { 0xF8, 0xF0, 4 },
};

/*
=item i_utf8_advance(char **p, size_t *len)

Retrieve a C<UTF-8> character from the stream.

Modifies *p and *len to indicate the consumed characters.

This doesn't support the extended C<UTF-8> encoding used by later
versions of Perl.  Since this is typically used to implement text
output by font drivers, the strings supplied shouldn't have such out
of range characters.

This doesn't check that the C<UTF-8> character is using the shortest
possible representation.

Returns ~0UL on failure.

=cut
*/

unsigned long 
i_utf8_advance(char const **p, size_t *len) {
  unsigned char c;
  unsigned i;
  int ci;
  unsigned clen = 0;
  unsigned char codes[3];
  if (*len == 0)
    return ~0UL;
  c = *(*p)++; --*len;

  for (i = 0; i < sizeof(utf8_sizes)/sizeof(*utf8_sizes); ++i) {
    if ((c & utf8_sizes[i].mask) == utf8_sizes[i].expect) {
      clen = utf8_sizes[i].size;
      break;
    }
  }
  if (clen == 0 || *len < clen-1) {
    --*p; ++*len;
    return ~0UL;
  }

  /* check that each character is well formed */
  i = 1;
  ci = 0;
  while (i < clen) {
    if (((*p)[ci] & 0xC0) != 0x80) {
      --*p; ++*len;
      return ~0UL;
    }
    codes[ci] = (*p)[ci];
    ++ci; ++i;
  }
  *p += clen-1; *len -= clen-1;
  if (c & 0x80) {
    if ((c & 0xE0) == 0xC0) {
      return ((c & 0x1F) << 6) + (codes[0] & 0x3F);
    }
    else if ((c & 0xF0) == 0xE0) {
      return ((c & 0x0F) << 12) | ((codes[0] & 0x3F) << 6) | (codes[1] & 0x3f);
    }
    else if ((c & 0xF8) == 0xF0) {
      return ((c & 0x07) << 18) | ((codes[0] & 0x3F) << 12) 
              | ((codes[1] & 0x3F) << 6) | (codes[2] & 0x3F);
    }
    else {
      *p -= clen; *len += clen;
      return ~0UL;
    }
  }
  else {
    return c;
  }
}

