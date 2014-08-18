#pragma once

#include <pthread.h>
#include <list.h>
#include <semaphore.h>

typedef void * targ;
typedef uptr tid;
typedef pthread_mutex_t mutex;
typedef sem_t sem;

typedef struct{
    list threads;
    pthread_mutex_t l;
    tid next_id;
} tgrp;
#define TGRP(self)                              \
    {.threads = LIST(&(self)->threads, NULL), .l=PTHREAD_MUTEX_INITIALIZER}

typedef struct{
    pthread_t ptid;
    tid tgid;
    targ (*main)(targ a);
    targ arg;
    sem rdy;
    lanchor lanc;
} tctxt;

