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
        /* Standard C (6.6) doesn't support casting addresses in constant
           expressions. GCC/CLANG do as an undocumented extension, but no
           computation from a cast may be truncated (as it would be if
           writing .pt). The only documentation of this is a mailing list
           post
           (http://lists.cs.uiuc.edu/pipermail/cfe-dev/2013-May/029450.html)
           attributing this to relocation troubles. Nevertheless, an
           untruncated cast can be masked to get the same effect, as in
           LFLIST(). */
        uptr constexp;
    };
    uptr gen;
} align(sizeof(dptr));
#define mpt(flanc) ((uptr) (flanc) >> 3)

struct flanchor{
    volatile flx n;
    volatile flx p;
};
#define FLANCHOR(list)                                  \
    {.n.constexp = (list) ? 1 + (uptr) (list) : 0,      \
     .p.constexp = (list) ? 1 + (uptr) (list) : 0}
CASSERT(offsetof(list, nil) == 0);

typedef volatile struct lflist{
    flanchor nil;
}lflist;
#define LFLIST(l, elem)                                                 \
    {{.n = {.constexp = (elem) ? (uptr) (elem) : 1 + (uptr) &(l)->nil}, \
      .p = {.constexp = (elem) ? (uptr) (elem) : 1 + (uptr) &(l)->nil}}}

#endif  /* FAKELOCKFREE */

flx flx_of(flanchor *a);
flanchor *flptr(flx a);

/* Iff !ret and no subseqent call to lflist_enq(a, ..) has been made, then
   lflist_enq(a, t, ..) == 0, and for all flx x | flptr(x) == flptr(a), 0
   != lflist_del(x, ..), and x != lflist_deq(..).

   If ret, then 0 != lflist_del(a, ..) and a != lflist_deq(..). */
err lflist_del(flx a, type *t);

/* TODO: fuck this */
/* Iff !ret and lflist_del(a, t) hasn't been called and lflist_deq hasn't
   returned a, then a == lflist_deq(t, l)) for the n'th subsequent call to
   lflist_deq where n > the number of times lflist_enq(.., l) had been
   called and 0 == lflist_del(a, t). */
err lflist_enq(flx a, type *t, lflist *l);
flx lflist_deq(type *t, lflist *l);

flx lflist_peek(lflist *l);
flx lflist_next(flx p, lflist *l);

#define pudef (flx, "{%:%:%:%, %}", (void *)(a->pt << 3), (uptr) a->nil, \
               (uptr) a->locked, (uptr) a->helped, a->gen)
#include <pudef.h>
#define pudef (flanchor, "n:%, p:%", a->n, a->p)
#include <pudef.h>
#define pudef (lflist, "LIST(%)", a->nil)
#include <pudef.h>

#define _lflist_del(as...) linref_account(0, lflist_del(as))
#define _lflist_enq(as...) linref_account(0, lflist_enq(as))
#define _lflist_deq(as...) linref_account(flptr(account_expr) ? 1 : 0,  \
                                          lflist_deq(as))

#define lflist_del(as...) trace(LFLISTM, 1, _lflist_del, as)
#define lflist_deq(as...) trace(LFLISTM, 1, _lflist_deq, as)
#define lflist_enq(as...) trace(LFLISTM, 1, _lflist_enq, as)
#define lflist_peek(as...) trace(LFLISTM, 2, lflist_peek, as)
#define lflist_next(as...) trace(LFLISTM, 2, lflist_next, as)
