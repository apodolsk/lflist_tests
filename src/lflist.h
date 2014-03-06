#pragma once

#define FAKELOCKFREE

#ifndef FAKELOCKFREE

#include <nalloc.h>

typedef volatile struct flanchor flanchor;

typedef struct { uintptr_t i:62; uint locked:1; uint unlocking:1 ;} flgen;

typedef volatile struct {
    flanchor *pt;
    flgen gen;
} flx;

struct flanchor{
    flx n;
    flx p;
};
#define FRESH_FLANCHOR {}

typedef struct lflist{
    flanchor nil;
} lflist;
#define FRESH_LFLIST(l) {{.n = {&(l)->nil}, .p = {&(l)->nil}}}

flx flx_of(flanchor *a);
flanchor *flptr(flx a);

void lflist_add_before(flx a, flx n, heritage *h, lflist *l);
int lflist_remove(flx a, heritage *h, lflist *l);

flx lflist_pop_front(heritage *h, lflist *l);
void lflist_add_rear(flx a, heritage *h, lflist *l);

#else  /* FAKELOCKFREE */

#include <list.h>
#include <pthreads.h>

typedef flanchor flx;
typedef lanchor flanchor;

#define FRESH_FLANCHOR {}

typedef struct lflist{
    flanchor nil;
    pthread_mutex_t lock;
} lflist;


#define FRESH_LFLIST(l) ({                          \
            pthread_mutex_init(&(l)->lock, NULL);   \
            {.nil = {&(l)->nil, &(l)->nil} }        \
        })

flx flx_of(flanchor *a);
flanchor *flptr(flx a);

void lflist_add_before(flx a, flx n, heritage *h, lflist *l);
int lflist_remove(flx a, heritage *h, lflist *l);

flx lflist_pop_front(heritage *h, lflist *l);
void lflist_add_rear(flx a, heritage *h, lflist *l);

#endif
