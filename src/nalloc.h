#pragma once

#include <list.h>
#include <stack.h>

typedef struct slab slab;

typedef struct {
    sanchor sanc;
} block;
/* linref_up takes a lineage as a reminder that all members of the lineage
   will keep their type EXCEPT FOR the lineage header, which is undefined
   when the block is free. I just set aside a header even when I have
   guarantees that the block isn't free, but that can be avoided. */
typedef block lineage;

/* Takes up space so that its address may be a unique key. */
typedef const struct type{
    const char *name;
    size size;
    checked err (*linref_up)(volatile void *l, const struct type *t);
    void (*linref_down)(volatile void *l);
} type;
#define TYPE(t, lu, ld) {#t, sizeof(t), lu, ld}

typedef struct heritage{
    lfstack slabs;
    lfstack *hot_slabs;
    cnt max_slabs;
    cnt slab_alloc_batch;
    type *t;
    void (*block_init)(void *b);
    slab *(*new_slabs)(cnt nslabs);
} heritage;
#define HERITAGE(t, ms, sab, bi, ns)                    \
    {LFSTACK, &hot_slabs, ms, sab, t, bi, ns}
#define KERN_HERITAGE(t, bi) HERITAGE(t, 16, 4, bi, new_slabs)
#define POSIX_HERITAGE KERN_HERITAGE

extern lfstack hot_slabs;

void *smalloc(size size);
void sfree(void *b, size size);
void *malloc(size size);
void free(void *b);

typedef void (*linit)(void *);

checked
void *linalloc(heritage *h);
void linfree(lineage *l);
/* ret == EOK iff there's a set of heritages H where, if no corresponding
   call to linref_down(l) has been made, then for all *h in H:
   - Only linalloc(h) can return l.
   - h->t == t.
   - Except for its block header, no part of l will be written to by a
   nalloc function, even if linfree(l) is called. */
err dflt_linref_up(volatile void *l, type *t);
err linref_up(volatile void *l, type *t);
/* If EOK == linref_up(l, t) and no correspending call to linref_down(l)
   had been made, then l might no longer be restricted to any set of
   heritages, but might be returned by any call to linalloc(). */
void dflt_linref_down(volatile void *l);
void linref_down(volatile void *l);

#define SLAB_SIZE (2 * PAGE_SIZE)
#define MIN_ALIGNMENT (sizeof(dptr))

typedef struct __attribute__((__aligned__(SLAB_SIZE))) slab{
    sanchor sanc;
    stack free_blocks;
    struct tyx {
        const struct type *t;
        uptr linrefs;
    } tx __attribute__((__aligned__(sizeof(struct tyx))));
    cnt cold_blocks;
    struct cpu *owner;
    struct heritage *her;
    __attribute__((__aligned__(CACHELINE_SIZE)))
    lfstack wayward_blocks;
    __attribute__((__aligned__(MIN_ALIGNMENT)))
    u8 blocks[];
}slab;
#define SLAB {.free_blocks = STACK}
CASSERT(sizeof(slab) == SLAB_SIZE);
#define MAX_BLOCK (SLAB_SIZE - offsetof(slab, blocks))

#define pudef (type, "(typ){%}", a->name)
#include <pudef.h>
#define pudef (heritage, "(her){%, nslabs:%}", a->t, a->slabs.size)
#include <pudef.h>
#define pudef (slab, "(slab){%, o:%, nfree:%}",                 \
               a->tx.t, (void *) a->owner, a->free_blocks.size)
#include <pudef.h>

#define smalloc(as...) trace(NALLOC, 1, smalloc, as)
#define sfree(as...) trace(NALLOC, 1, sfree, as)
#define malloc(as...) trace(NALLOC, 1, malloc, as)
#define free(as...) trace(NALLOC, 1, free, as)
#define linalloc(as...) trace(NALLOC, 1, linalloc, as)
#define linfree(as...) trace(NALLOC, 1, linfree, as)
/* #define linref_up(as...) nalloc_trace(linref_up, as) */
/* #define linref_down(as...) nalloc_trace(linref_down, as) */

#define NALLOC_TS
