#pragma once

#include <list.h>
#include <pthread.h>
#include <stack.h>
#include <stdbool.h>
#include <whtypes.h>

typedef struct {
    sanchor_t sanc;
} block_t;
#define FRESH_BLOCK {}

block_t *block_of(void *addr);

#ifdef HIDE_NALLOC
#define smalloc _smalloc
#define sfree _sfree
#define malloc _malloc
#define free _free
#endif

void *_smalloc(size_t size);
void _sfree(void *b, size_t size);
void *_malloc(size_t size);
void _free(void *b);

void *_smemalign(size_t alignment, size_t size);

/* linref_up takes a lineage_t because it means all of the lineage will
   keep its type EXCEPT FOR the block header, which is undefined when the
   block is on a free list. I just set aside a header even when I have
   guarantees that the block isn't free, but that can be avoided. */
typedef block_t lineage_t;

/* Takes up space so that its address may be a unique key. */
typedef struct{
    char _;
} type_key;

typedef struct{
    simpstack slabs;
    size_t size_of;
    type_key *key;
} heritage;
#define FRESH_HERITAGE(s, k)                        \
    {.slabs = FRESH_SIMPSTACK,                      \
            .size_of = align_up_pow2(s, MIN_ALIGNMENT), \
            .key = k}

lineage_t *linalloc(heritage *type, void (*block_init)(void *));
void linfree(lineage_t *l);
int linref_up(volatile void *l, heritage *wanted);
void linref_down(volatile void *l);

/* TODO: delete */
#define PAGE_SIZE 4096

#define HEAP_LOW 0x00300000
#define HEAP_HIGH 0x01000000
#define IDEAL_CACHED_SLABS 16
#define MAX_BLOCK (SLAB_SIZE - offsetof(slab_t, blocks))
#define MIN_ALIGNMENT 16
#define SLAB_SIZE (PAGE_SIZE)
#define NALLOC_MAGIC_INT 0x01FA110C
#define MMAP_BATCH 16

typedef struct __attribute__((__aligned__(SLAB_SIZE))){
    sanchor_t sanc;
    size_t block_size;
    simpstack free_blocks;
    union hxchg_t{
        struct{
            uptr linrefs;
            heritage *type;
        };
        dblptr hx;
    };
    int nblocks_contig;
    stack wayward_blocks;
    pthread_t owner;
    __attribute__((__aligned__(MIN_ALIGNMENT)))
    uint8_t blocks[];
} slab_t;

typedef union hxchg_t hxchg_t;
#define FRESH_SLAB {.free_blocks = FRESH_SIMPSTACK}
