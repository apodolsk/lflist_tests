#pragma once
#include <list.h>
#include <nalloc.h>
#include <pthread.h>

typedef struct{
    lanchor lanc;
    uptr gen;
    struct lflist *host;
} flanchor;
#define FLANCHOR(list)                          \
    {LANCHOR(list), .host = list}

typedef struct {
    flanchor *a;
    uptr gen;
}flx;

typedef struct lflist{
    list l;
    pthread_mutex mut;
} lflist;
#define LFLIST(self, elem)                      \
    {.l = LIST(&(self)->l, elem),               \
            .mut = PTHREAD_MUTEX_INITIALIZER}

pudef(flx, (), "{%, %}", a->a, a->gen);
pudef(flanchor, (lanchor), "l:%, g:%", a->lanc, a->gen);
pudef(lflist, (list), "list{%}", a->l);
