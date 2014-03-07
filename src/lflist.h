#pragma once

/* #define FAKELOCKFREE */

#ifndef FAKELOCKFREE

#include <nalloc.h>
#include <whtypes.h>

typedef struct flx flx;
typedef volatile struct flanchor flanchor;
typedef union mptr mptr;

typedef struct { uptr i:62; uint locked:1; uint unlocking:1; } flgen;
#define GEN_INTVL 4

struct flx{
    union mptr {
        struct{
            uptr ptr:63;
            uint is_nil:1;
        };
        flanchor *pt;
    } mp;
    flgen gen;
} __attribute__((__aligned__ (2 * sizeof(void *))));

#define mptr(p, n) ((mptr){(uptr) p, n})

struct flanchor{
    volatile flx n;
    volatile flx p;
};
#define FRESH_FLANCHOR {}

typedef struct lflist{
    flanchor nil;
} lflist;
#define FRESH_LFLIST(l) {{                      \
                .n = {mptr(&(l)->nil, 1)},      \
                .p = {mptr(&(l)->nil, 1)}       \
        }}                                      \

flx flx_of(flanchor *a);
flanchor *flptr(flx a);

void lflist_add_before(flx a, flx n, type *h);
int lflist_remove(flx a, type *h);

flx lflist_pop_front(type *h, lflist *l);
void lflist_add_rear(flx a, type *h, lflist *l);

#else  /* FAKELOCKFREE */

#include <list.h>
#include <pthread.h>
#include <nalloc.h>

typedef lanchor_t flanchor;
typedef flanchor *flx;

#define FRESH_FLANCHOR {}

typedef struct lflist{
    list_t l;
    pthread_mutex_t lock;
} lflist;


#define FRESH_LFLIST(_l) ({                          \
            lflist l = {.l = FRESH_LIST(&(_l)->l)};  \
            pthread_mutex_init(&(_l)->lock, NULL);   \
            l;                                       \
        })

flx flx_of(flanchor *a);
flanchor *flptr(flx a);

void lflist_add_before(flx a, flx n, type *h, lflist *l);
int lflist_remove(flx a, type *h, lflist *l);

flx lflist_pop_front(type *h, lflist *l);
void lflist_add_rear(flx a, type *h, lflist *l);

#endif
