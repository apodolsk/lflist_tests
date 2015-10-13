#pragma once

#include <nalloc.h>
#include <wrand.h>

typedef struct thread{
    nalloc_info nallocin;
    rand_info randin;
    bool racing;
} thread;

extern __thread thread manual_tls;
#define T (&manual_tls)
