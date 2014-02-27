#pragma once

#include <nalloc.h>

struct lflist;
typedef volatile struct flanchor flanchor;

struct flanchor{
    struct pxchg{
        union genptr {
            flanchor *p;
            struct { int64_t ngen:63; int locked:1; };
        };
        int gen;
    };
    struct nxchg{
        flanchor *n;
        union markptr {
            flanchor *pat;
            struct { int64_t patint:63; int read:1; };
        };
    };
    struct lflist *host;
};

#define FRESH_FLANCHOR {}

typedef struct lflist{
    flanchor nil;
} lflist;

/* #define FRESH_LFLIST {} */

#define REMOVING ((void *) 0x1)
#define ADDING ((void *) 0x2)

typedef union markptr markptr;
typedef union genptr genptr;
typedef struct nxchg nxchg;
typedef struct pxchg pxchg;

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
