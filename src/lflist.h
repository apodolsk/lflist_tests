#pragma once

/* #define FAKELOCKFREE */

#ifndef FAKELOCKFREE

#include <nalloc.h>

typedef struct flx flx;
typedef volatile struct flanchor flanchor;
typedef union mptr mptr;

typedef struct { uptr i:62; uint locked:1; uint unlocking:1; } flgen;

struct flx{
    union mptr {
        struct{
            uint is_nil:1;
            uptr ptr:WORDBITS-1;
        };
        /* For debugging. */
        flanchor *pt;
    } mp;
    flgen gen;
} __attribute__((__aligned__(sizeof(dptr))));

#define mptr(p, nil) ((mptr){.is_nil=nil, .ptr=(uptr)(p) >> 1})

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
        }}                                      \

flx flx_of(flanchor *a);
flanchor *flptr(flx a);

err lflist_add_before(flx a, flx n, type *h);
err lflist_remove(flx a, type *h);

flx lflist_pop_front(type *h, lflist *l);
err lflist_add_rear(flx a, type *h, lflist *l);









#else  /* FAKELOCKFREE */

#include <list.h>
#include <nalloc.h>

typedef struct{
    lanchor lanc;
    int gen;
} flanchor;
#define FLANCHOR {LANCHOR}

typedef struct {
    flanchor *a;
    int gen;
}flx;

typedef struct lflist{
    list l;
} lflist;

#define LFLIST(self) {LIST}

flx flx_of(flanchor *a);
flanchor *flptr(flx a);

err lflist_add_before(flx a, flx n, type *h, lflist *l);
err lflist_remove(flx a, type *h, lflist *l);

flx lflist_pop_front(type *h, lflist *l);
err lflist_add_rear(flx a, type *h, lflist *l);

#endif
