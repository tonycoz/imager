/*
  dummy mutexes, for non-threaded builds
*/

#include "imageri.h"

#include <pthread.h>

/* documented in mutexwin.c */

struct i_mutex_tag {
  int dummy;
};

i_mutex_t
i_mutex_create(void) {
  i_mutex_t m;

  m = mymalloc(sizeof(*m));

  return m;
}

void
i_mutex_destroy(i_mutex_t m) {
  myfree(m);
}

void
i_mutex_lock(i_mutex_t m) {
  (void)m;
}

void
i_mutex_unlock(i_mutex_t m) {
  (void)m;
}
