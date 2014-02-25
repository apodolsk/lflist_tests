/**
 * @file   nalloc.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * @date   Sun Oct 28 16:19:37 2012
 * 
 * @brief  
 * 
 */

#ifndef NALLOC_H
#define NALLOC_H

#include <list.h>
#include <peb_macros.h>
#include <peb_util.h>
#include <pthread.h>
#include <stack.h>

#ifdef HIDE_NALLOC
#define malloc nmalloc
#define free nfree
#define calloc ncalloc
#define realloc nrealloc
#endif

#define NALLOC_MAGIC_INT 0x999A110C

/* TODO: I know this is non-portable. I just don't want to bother generating
   my own tids.  */
#define INVALID_TID 0

#define CACHELINE_SHIFT 6
#define CACHELINE_SIZE (1 << 6)

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define SLAB_SIZE (PAGE_SIZE)

#define MIN_ALIGNMENT 16

#define SLAB_ALLOC_BATCH 16
#define IDEAL_FULL_SLABS 5

#define MIN_BLOCK 16
/* TODO: this is 512, aka way too small. Need bigger slabs. */
#define MAX_BLOCK (SLAB_SIZE / 8)

static const int bcache_size_lookup[] = {
    16, 32, 48, 64, 80, 96, 112, 128,
    256, 512, 
};
#define NBSTACKS ARR_LEN(bcache_size_lookup)

typedef struct{
    size_t size;
} large_block_t;

typedef struct{
    sanchor_t sanc;
} block_t;
COMPILE_ASSERT(sizeof(block_t) <= MIN_BLOCK);

#define FRESH_BLOCK {.sanc = FRESH_SANCHOR }

typedef struct{
    lfstack_t wayward_blocks;
    simpstack_t priv_blocks;
    pthread_t host_tid;
    sanchor_t sanc;
    unsigned int nblocks_contig;
    size_t block_size;
    uint8_t blocks[];
} slab_t __attribute__((__aligned__(MIN_ALIGNMENT)));

/* Note that the refcnt here is a dummy val. It should never reach zero. */
/* Ignore emacs' crazy indentation. There's no simple way to fix it. */
#define FRESH_SLAB {                                            \
        .wayward_blocks = FRESH_STACK,                          \
            .priv_blocks = FRESH_SIMPSTACK,                     \
            .host_tid = INVALID_TID,                            \
            .sanc = FRESH_SANCHOR,                              \
            .nblocks_contig = 0,                                \
            .block_size = 0,                                    \
                 }

COMPILE_ASSERT(sizeof(large_block_t) != sizeof(slab_t));
COMPILE_ASSERT(aligned_pow2(sizeof(slab_t), MIN_ALIGNMENT));

typedef struct {
    size_t num_bytes;
    size_t num_slabs;
    size_t num_bytes_highwater;
    size_t num_slabs_highwater;
} nalloc_profile_t;

void *malloc(size_t size);
void free(void *buf);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

nalloc_profile_t *get_profile(void);

#endif
