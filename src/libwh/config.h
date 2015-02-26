#pragma once

#define PAGE_SHIFT  12
#define PAGE_SIZE   (1 << PAGE_SHIFT)
#define CACHELINE_SIZE 64

#define SLAB_SIZE (4 * PAGE_SIZE)

#include <tid.h>

void breakpoint(void);

bool interrupts_enabled(void);

struct cpu;
struct thread;

struct thread *this_thread(void);
void slabs_init(void);
struct slab *new_slabs(cnt batch);

dbg_id get_dbg_id(void);

err pause_universe(void);
void resume_universe(void);

void *heap_start(void);
void *heap_end(void);

bool poisoned(void);
