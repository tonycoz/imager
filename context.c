#include "imageri.h"
#include <stdio.h>

static im_slot_t slot_count;
static im_slot_destroy_t *slot_destructors;
static i_mutex_t slot_mutex;

/*
=item im_context_new()

Create a new Imager context object.

=cut
*/

im_context_t
im_context_new(void) {
  im_context_t ctx = malloc(sizeof(im_context_struct));
  int i;

  if (!slot_mutex)
    slot_mutex = i_mutex_new();

  if (!ctx)
    return NULL;
  
  ctx->error_sp = IM_ERROR_COUNT-1;
  for (i = 0; i < IM_ERROR_COUNT; ++i) {
    ctx->error_alloc[i] = 0;
    ctx->error_stack[i].msg = NULL;
    ctx->error_stack[i].code = 0;
  }
#ifdef IMAGER_LOG
  ctx->log_level = 0;
  ctx->lg_file = NULL;
#endif
  ctx->max_width = 0;
  ctx->max_height = 0;
  ctx->max_bytes = DEF_BYTES_LIMIT;

  ctx->slot_alloc = slot_count;
  ctx->slots = calloc(sizeof(void *), ctx->slot_alloc);
  if (!ctx->slots) {
    free(ctx);
    return NULL;
  }

  ctx->refcount = 1;

#ifdef IMAGER_TRACE_CONTEXT
  fprintf(stderr, "im_context: created %p\n", ctx);
#endif


  return ctx;
}

/*
=item im_context_refinc(ctx, where)
X<im_context_refinc API>
=section Context objects
=synopsis im_context_refinc(aIMCTX, "a description");

Add a new reference to the context.

=cut
*/

void
im_context_refinc(im_context_t ctx, const char *where) {
  ++ctx->refcount;

#ifdef IMAGER_TRACE_CONTEXT
  fprintf(stderr, "im_context:%s: refinc %p (count now %lu)\n", where,
	  ctx, (unsigned long)ctx->refcount);
#endif
}

/*
=item im_context_refdec(ctx, where)
X<im_context_refdec API>
=section Context objects
=synopsis im_context_refdec(aIMCTX, "a description");

Remove a reference to the context, releasing it if all references have
been removed.

=cut
*/

void
im_context_refdec(im_context_t ctx, const char *where) {
  int i;
  im_slot_t slot;

  im_assert(ctx->refcount > 0);

  --ctx->refcount;

#ifdef IMAGER_TRACE_CONTEXT
  fprintf(stderr, "im_context:%s: delete %p (count now %lu)\n", where,
	  ctx, (unsigned long)ctx->refcount);
#endif

  if (ctx->refcount != 0)
    return;

  for (slot = 0; slot < ctx->slot_alloc; ++slot) {
    if (ctx->slots[slot] && slot_destructors[slot])
      slot_destructors[slot](ctx->slots[slot]);
  }

  free(ctx->slots);

  for (i = 0; i < IM_ERROR_COUNT; ++i) {
    if (ctx->error_stack[i].msg)
      myfree(ctx->error_stack[i].msg);
  }
#ifdef IMAGER_LOG
  if (ctx->lg_file)
    fclose(ctx->lg_file);
#endif

  free(ctx);
}

/*
=item im_context_clone(ctx)

Clone an Imager context object, returning the result.

=cut
*/

im_context_t
im_context_clone(im_context_t ctx, const char *where) {
  im_context_t nctx = malloc(sizeof(im_context_struct));
  int i;

  if (!nctx)
    return NULL;

  nctx->slot_alloc = slot_count;
  nctx->slots = calloc(sizeof(void *), ctx->slot_alloc);
  if (!nctx->slots) {
    free(nctx);
    return NULL;
  }

  nctx->error_sp = ctx->error_sp;
  for (i = 0; i < IM_ERROR_COUNT; ++i) {
    if (ctx->error_stack[i].msg) {
      size_t sz = ctx->error_alloc[i];
      nctx->error_alloc[i] = sz;
      nctx->error_stack[i].msg = mymalloc(sz);
      memcpy(nctx->error_stack[i].msg, ctx->error_stack[i].msg, sz);
    }
    else {
      nctx->error_alloc[i] = 0;
      nctx->error_stack[i].msg = NULL;
    }
    nctx->error_stack[i].code = ctx->error_stack[i].code;
  }
#ifdef IMAGER_LOG
  nctx->log_level = ctx->log_level;
  if (ctx->lg_file) {
    /* disable buffering, this isn't perfect */
    setvbuf(ctx->lg_file, NULL, _IONBF, 0);

    /* clone that and disable buffering some more */
    nctx->lg_file = fdopen(fileno(ctx->lg_file), "a");
    if (nctx->lg_file)
      setvbuf(nctx->lg_file, NULL, _IONBF, 0);
  }
  else {
    nctx->lg_file = NULL;
  }
#endif
  nctx->max_width = ctx->max_width;
  nctx->max_height = ctx->max_height;
  nctx->max_bytes = ctx->max_bytes;

  nctx->refcount = 1;

#ifdef IMAGER_TRACE_CONTEXT
  fprintf(stderr, "im_context:%s: cloned %p to %p\n", where, ctx, nctx);
#endif

  return nctx;
}

/*
=item im_context_slot_new(destructor, where)

Allocate a new context-local-storage slot.

=cut
*/

im_slot_t
im_context_slot_new(im_slot_destroy_t destructor, const char *where) {
  im_slot_t new_slot;
  im_slot_destroy_t *new_destructors;
  if (!slot_mutex)
    slot_mutex = i_mutex_new();

  i_mutex_lock(slot_mutex);

  new_slot = slot_count++;
  new_destructors = realloc(slot_destructors, sizeof(void *) * slot_count);
  if (!new_destructors)
    i_fatal(1, "Cannot allocate memory for slot destructors");
  slot_destructors = new_destructors;

  slot_destructors[new_slot] = destructor;

#ifdef IMAGER_TRACE_CONTEXT
  fprintf(stderr, "im_context: slot %d allocated for %s\n",
	  (int)new_slot, where);
#endif

  i_mutex_unlock(slot_mutex);

  return new_slot;
}

/*
=item im_context_slot_set(slot, value)

Set the value of a slot.

Returns true on success.

Aborts if the slot supplied is invalid.

If reallocation of slot storage fails, returns false.

=cut
*/

int
im_context_slot_set(im_context_t ctx, im_slot_t slot, void *value) {
  if (slot < 0 || slot >= slot_count) {
    fprintf(stderr, "Invalid slot %d (valid 0 - %d)\n",
	    (int)slot, (int)slot_count-1);
    abort();
  }

  if (slot >= ctx->slot_alloc) {
    ssize_t i;
    size_t new_alloc = slot_count;
    void **new_slots = realloc(ctx->slots, sizeof(void *) * new_alloc);

    if (!new_slots)
      return 0;

    for (i = ctx->slot_alloc; i < new_alloc; ++i)
      new_slots[i] = NULL;

    ctx->slots = new_slots;
    ctx->slot_alloc = new_alloc;
  }

  ctx->slots[slot] = value;

#ifdef IMAGER_TRACE_CONTEXT
  fprintf(stderr, "im_context: ctx %p slot %d set to %p\n",
	  ctx, (int)slot, value);
#endif

  return 1;
}

/*
=item im_context_slot_get(ctx, slot)

Retrieve the value previously stored in the given slot of the context
object.

=cut
*/

void *
im_context_slot_get(im_context_t ctx, im_slot_t slot) {
  if (slot < 0 || slot >= slot_count) {
    fprintf(stderr, "Invalid slot %d (valid 0 - %d)\n",
	    (int)slot, (int)slot_count-1);
    abort();
  }

  if (slot >= ctx->slot_alloc)
    return NULL;

#ifdef IMAGER_TRACE_CONTEXT
  fprintf(stderr, "im_context: ctx %p slot %d retrieved as %p\n",
	  ctx, (int)slot, ctx->slots[slot]);
#endif

  return ctx->slots[slot];
}
