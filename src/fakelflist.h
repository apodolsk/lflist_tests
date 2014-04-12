#pragma once
#include <list.h>
#include <nalloc.h>
#include <pthread.h>

typedef struct{
    lanchor lanc;
    uptr gen;
    struct lflist *host;
} flanchor;
#define FLANCHOR {LANCHOR}

typedef struct {
    flanchor *a;
    uptr gen;
}flx;

typedef struct lflist{
    list l;
    pthread_mutex_t mut;
} lflist;
#define LFLIST(self) {.l = LIST(&(self)->l),    \
            .mut = PTHREAD_MUTEX_INITIALIZER}

pudef(flx, "{%p, %u}", a->a, a->gen);
pudef(flanchor, "l:%s, g:%d", pustr(a->lanc, lanchor), a->gen);
pudef(lflist, "LIST(%s)", pustr(a->l, list));
