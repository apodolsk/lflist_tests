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
            uptr helped:1;
            uptr pt:WORDBITS-3;
        };
    };
    uptr gen;
} __attribute__((__aligned__(sizeof(dptr))));
static inline uptr mpt(flanchor *f){ return (uptr) f >> 3; };

struct flanchor{
    volatile flx n;
    volatile flx p;
};
#define FLANCHOR {}

typedef struct lflist{
    flanchor nil;
} lflist;
#define LFLIST(l) {{.n = {.nil=1, .pt=mpt(&(l)->nil)},          \
                    .p = {.nil=1, .pt=mpt(&(l)->nil)}}}

pudef(flx, (), "{%:%:%:%, %}", (void *)(a->pt << 3), a->nil,
      a->locked, a->helped, a->gen);
pudef(flanchor, (flx), "n:%, p:%", a->n, a->p);
pudef(lflist, (flanchor), "LIST(%s)", a->nil);

#endif  /* FAKELOCKFREE */

flx flx_of(flanchor *a);
flanchor *flptr(flx a);

err lflist_del(flx a, type *t);

flx lflist_deq(type *t, lflist *l);
err lflist_enq(flx a, type *t, lflist *l);

#define LFLIST_TS flx, flanchor, lflist
#define lflist_trace(f, as...) trace(LFLISTM, (LFLIST_TS), f, as...)
/* #define lflist_del(as...) lflist_trace(lflist_del, as) */
#define lflist_del(as...) putrace(puprintf, (), lflist_del, as)
/* #define lflist_deq(as...) lflist_trace(lflist_deq, as) */
/* #define lflist_enq(as...) lflist_trace(lflist_enq, as) */
