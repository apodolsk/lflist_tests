#pragma once

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
#define FRESH_LFLIST(l) {.nil = {.n = {l->nil, 0}, .p = {l->nil, 0}}}

flx flx_of(flanchor *a);
flanchor *ptr_of(flx a);

void lflist_add_before(flx a, flx n, heritage *h, lflist *l);
int lflist_remove(flx a, heritage *h, lflist *l);

flx lflist_pop_front(heritage *h, lflist *l);
void lflist_add_rear(flx a, heritage *h, lflist *l);
