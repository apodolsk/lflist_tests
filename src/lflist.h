#pragma once
/* #define FAKELOCKFREE */
#ifdef FAKELOCKFREE
#include <fakelflist.h>
#else

#include <nalloc.h>

typedef struct flx flx;
typedef volatile struct flanchor flanchor;
typedef union mptr mptr;

struct flx{
    union {
        flanchor *mp;
        struct{
            uptr nil:1;
            uptr locked:1;
            uptr pt:WORDBITS-2;
        };
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
#define LFLIST(l) {{.n = {.nil=1, .pt=(uptr)&(l)->nil << 2},    \
                    .p = {.nil=1, .pt=(uptr)&(l)->nil << 2}}}

pudef(flx, "{%p:%d:%d, %ud}", (void *)(a->pt << 1), a->nil, a->locked, a->gen);
pudef(flanchor, "n:%s, p:%s", pustr(a->n, flx), pustr(a->p, flx));
pudef(lflist, "LIST(%s)", pustr(a->nil, flanchor));

#endif

flx flx_of(flanchor *a);
flanchor *flptr(flx a);

err lflist_del(flx a, type *t);

flx lflist_deq(type *t, lflist *l);
err lflist_enq(flx a, type *t, lflist *l);

#define LFLIST_TS flx, flanchor, lflist
#define lflist_trace(f, as...) putrace(llprintf1, (flx, flanchor, lflist), f, as)
/* #define lflist_add_before(a...) lflist_trace(lflist_add_before, a) */
#define lflist_del(a...) lflist_trace(lflist_del, a)
/* #define lflist_deq(a...) lflist_trace(lflist_deq, a) */
#define lflist_enq(a...) lflist_trace(lflist_enq, a)
