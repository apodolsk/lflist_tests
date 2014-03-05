#pragma once

#include <nalloc.h>

typedef volatile struct flanchor flanchor;

typedef union { uintptr_t igen:62; uint locked:1; uint nil:1;} flgen;
typedef volatile struct { flanchor *pt; flgen gen; } flx;

struct flanchor{
    flx n;
    flx p;
};
#define FRESH_FLANCHOR {}

typedef struct lflist{
    flanchor nil;
} lflist;
#define FRESH_LFLIST(l) {.nil = {.n = {l->nil, 0}, .p = {l->nil, 0}}}

int lflist_remove_any(flanchor *a, heritage *h);
int lflist_remove(flanchor *a, heritage *h, lflist *l);

int lflist_add_before(flanchor *a, flanchor *n, heritage *h, lflist *l);
int lflist_add_rear(flanchor *a, heritage *h, lflist *l);

/* At any time, only one thread can be calling a priv function on a given
   node in a list, where l->nil is the node in question for
   pop_front_priv. */
int lflist_add_after_priv(flanchor *a, flanchor *p,
                          heritage *h, lflist *l);
int lflist_add_front_priv(flanchor *a, heritage *h, lflist *l);

flanchor *lflist_pop_front_priv(heritage *h, lflist *l);
flanchor *lflist_pop_rear(heritage *h, lflist *l);
