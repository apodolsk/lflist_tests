#pragma once

/* #define FAKELOCKFREE */

#ifndef FAKELOCKFREE

#include <nalloc.h>

typedef struct flx flx;
typedef volatile struct flanchor flanchor;
typedef union mptr mptr;

struct flx{
    union mptr {
        struct{
            uptr nil:1;
            uptr locked:1;
            uptr pt:WORDBITS-2;
        };
        /* For debugging. */
        flanchor *mp;
    };
    uptr gen;
} __attribute__((__aligned__(sizeof(dptr))));

struct flanchor{
    volatile flx n;
    volatile flx p;
};
#define FLANCHOR {}

typedef struct lflist{
    flanchor nil;
} lflist;
#define LFLIST(l) {{                            \
                .n = {mptr(&(l)->nil, 1)},      \
                .p = {mptr(&(l)->nil, 1)}       \
        }}                                      


flx flx_of(flanchor *a);
flanchor *flptr(flx a);

err lflist_add_before(flx a, flx n, type *h);
err lflist_remove(flx a, type *h);

flx lflist_pop_front(type *h, lflist *l);
err lflist_add_rear(flx a, type *h, lflist *l);

pudef(flgen, "gen:%d %d/%d", a->i, a->locked, a->unlocking);
pudef(flx, "{%p, %s}", (flanchor *) (a->mp.ptr << 1),
      pustr(a->gen, flgen));
pudef(flanchor, "n:%s, p:%s", pustr(a->n, flx), pustr(a->p, flx));
pudef(lflist, "LIST(%s)", pustr(a->nil, flanchor));

#else  /* FAKELOCKFREE */

#include <list.h>
#include <nalloc.h>

typedef struct{
    lanchor lanc;
    uptr gen;
} flanchor;
#define FLANCHOR {LANCHOR}

typedef struct {
    flanchor *a;
    uptr gen;
}flx;

typedef struct lflist{
    list l;
} lflist;
#define LFLIST(self) {.l = LIST(&(self)->l)}

flx flx_of(flanchor *a);
flanchor *flptr(flx a);

err lflist_add_before(flx a, flx n, type *h, lflist *l);
err lflist_remove(flx a, type *h, lflist *l);

flx lflist_pop_front(type *h, lflist *l);
err lflist_add_rear(flx a, type *h, lflist *l);

#endif

#define LFLIST_TS flx, flanchor, lflist
#define lflist_trace(f, as...) putrace(llprintf1, (flx, flanchor, lflist), f, as)
/* #define lflist_add_before(a...) lflist_trace(lflist_add_before, a) */
#define lflist_remove(a...) lflist_trace(lflist_remove, a)
/* #define lflist_pop_front(a...) lflist_trace(lflist_pop_front, a) */
#define lflist_add_rear(a...) lflist_trace(lflist_add_rear, a)
