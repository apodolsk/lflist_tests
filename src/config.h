#pragma once

#include <pthread.h>

#define PAGE_SHIFT  12
#define PAGE_SIZE   (1 << PAGE_SHIFT)
#define CACHELINE_SIZE 64

struct thread;
struct cpu;

#include <stdio.h>

#define BREAK() ({fflush(stdout); abort();})
#define C ((struct cpu *) pthread_self())

struct slab *new_slabs(cnt batch);
extern uint itid(void);
extern __thread uint tid_;
