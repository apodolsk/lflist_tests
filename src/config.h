#pragma once
#define KERNEL

#define PAGE_SHIFT  12
#define PAGE_SIZE   (1 << PAGE_SHIFT)
#define CACHELINE_SIZE 64

struct thread;

#define BREAK() abort()

struct slab *new_slabs(cnt batch);
extern uint itid(void);
extern uint tid_;
