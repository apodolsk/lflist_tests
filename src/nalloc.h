#pragma once

#include <list.h>
#include <stack.h>
#include <pthread.h>

void *smalloc(size size);
void sfree(void *b, size size);
void *malloc(size size);
void free(void *b);

typedef struct {
    sanchor sanc;
} block;
/* linref_up takes a lineage because it means all of the lineage will
   keep its type EXCEPT FOR the block header, which is undefined when the
   block is on a free list. I just set aside a header even when I have
   guarantees that the block isn't free, but that can be avoided. */
typedef block lineage;

/* Takes up space so that its address may be a unique key. */
typedef struct type{
    size size;
    err (*linref_up)(volatile void *l, struct type *t);
    void (*linref_down)(volatile void *l);
} type;

typedef struct slab slab;

typedef struct{
    lfstack slabs;
    lfstack *hot_slabs;
    type *t;
    void (*block_init)(void *b);
    slab *(*new_slabs)(cnt nslabs);
} heritage;
#define HERITAGE(t, hs, bi, ns) {LFSTACK, &hot_slabs, t, bi, ns}
#define KERN_HERITAGE(...) {}
/* #define KERN_HERITAGE(, bi) HERITAGE(, &hot_kern_slabs, bi, new_kern_slabs) */

typedef void (*linit)(void *);
void *linalloc(heritage *type);
void linfree(lineage *l);
err linref_up(volatile void *l, type *t);
void linref_down(volatile void *l);

#define SLAB_SIZE PAGE_SIZE
#define MAX_BLOCK (SLAB_SIZE - offsetof(slab, blocks))
