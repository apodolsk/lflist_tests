/**
 * @file   nalloc.c
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * @date   Sat Oct 27 19:16:19 2012
 * 
 * @brief A lockfree memory allocator.
 *
 * nalloc's main features are thread-local segregated list subheaps, a
 * lockfree global slab allocator to manage subheaps, and a lockfree
 * inter-thread memory transfer mechanism to return freed blocks to their
 * original subheap. Nah lock. 
 *
 * Why are the stack reads safe:
 * - wayward_blocks. Only 1 thread pops. No one else can pop the top and then
 * force it to segfault.
 * - free_slabs. Safe simply because I never return to the system. Was going
 * to say it's because unmaps happen before insertion to stack, but it's still
 * possible to read top, then someone pops top, uses it, and unmaps it before
 * reinserting. Could honestly just do a simple array. 
 *
 */

#define MODULE NALLOC

#include <list.h>
#include <stack.h>
#include <sys/mman.h>

#include <nalloc.h>
#include <global.h>

static void *large_alloc(size_t size);
static void large_dealloc(large_block_t *block);

static int bcacheidx_of(size_t size);

static void *alloc(size_t size);

static int slab_is_full(slab_t *s);
static int slab_num_priv_blocks(slab_t *s);
static int slab_is_on_priv_stack(slab_t *s);
static unsigned int slab_max_size(slab_t *s);
static block_t *alloc_from_slab(slab_t *slab);
static void dealloc_from_slab(block_t *b, slab_t *s);
static slab_t *slab_install_new(size_t size, int sizeidx);
/* static void slab_free(slab_t *freed, int cidx); */
static void steal_or_dealloc_slab(slab_t *s, int cidx);

static void dealloc(block_t *b);
static void return_wayward_block(block_t *b, slab_t *slab, int cidx);

static slab_t *slab_of(block_t *b);

static void free_slabs_atexit();
static void first_time_init(void);
static int dynamic_init(void);

static void profile_bytes(size_t nbytes);
static void profile_slabs(size_t nslabs);
static int write_block_magics(block_t *b, slab_t *s);
static int block_magics_valid(block_t *b, slab_t *s);


/* DANGER: It happens to be the case that an uninitialized lfstack is all
   0. If that's not true, then you need a barrier during first time init
   (pthread_once won't suffice). */
__attribute__ ((__aligned__(CACHELINE_SIZE)))
static lfstack_t dirty_slabs[NBSTACKS];

__attribute__ ((__aligned__(CACHELINE_SIZE)))
static lfstack_t clean_slabs = FRESH_STACK;

static __thread simpstack_t priv_slabs[NBSTACKS];

__attribute__ ((__aligned__(CACHELINE_SIZE)))
static __thread nalloc_profile_t profile;

/* Accounting info for pthread's wonky thread destructor setup. */
static pthread_once_t key_rdy = PTHREAD_ONCE_INIT;
static pthread_key_t destructor_key = 0;

void *malloc(size_t size){
    trace(size, lu);
    void *out;
    if(size == 0)
        return NULL;
    if(size > MAX_BLOCK){
        large_block_t *large_found = large_alloc(size);
        /* profile_bytes(large_found->size); */
        out = large_found ? large_found + 1 : NULL;
    } else
        out = alloc(size);
    PPNT(out);
    return out;
}

void free(void *buf){
    trace(buf, p);
    if(!buf)
        return;
    /* TODO: Assumes no one generates pointers near page boundaries. Fine now,
       but won't cut it for large slabs. */
    if(aligned_pow2((large_block_t *) buf - 1, PAGE_SIZE)){
        large_block_t *large = (large_block_t *) buf - 1;
        profile_bytes(0 - large->size);
        large_dealloc(large);
        return;
    }
    
    dealloc(buf);
}

void *calloc(size_t nmemb, size_t size){
    void *found = malloc(nmemb * size);
    if(!found)
        return NULL;
    memset(found, 0, nmemb * size);
    return found;
}

void *realloc(void *ptr, size_t size){
    void *new = malloc(size);
    if(new){
        size_t old_size = slab_of(ptr)->block_size;
        memcpy(new, ptr, min(size, old_size));
    }
    free(ptr);
    return ptr;
}

static
void *large_alloc(size_t size){
    size = align_up_pow2(size, PAGE_SIZE);
    large_block_t *found =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
             -1, 0);
    if(!found)
        return NULL;
    found->size = size;
    return found;
}

static
void large_dealloc(large_block_t *block){
    assert(aligned(block, PAGE_SIZE));
    munmap(block, block->size);
}

static
int bcacheidx_of(size_t size){
    assert(size);
    /* Yes, this should be turned into a non-brittle thing. Whatever. It's 5AM. */
    if(size <= 128)
        return (size - 1) >> 4;
    if(size <= 256)
        return 8;
    return 9;
        
    /* for(int i = 0; i < NBSTACKS; i++) */
    /*     if(size <= bcache_size_lookup[i]) */
    /*         return i; */
    LOGIC_ERROR("Little allocator vs big request: %lu.", size);
}

static
void *alloc(size_t size){
    trace2(size, lu);

    if(dynamic_init())
        return NULL;

    block_t *found;
    int cidx = bcacheidx_of(size);
    size_t rounded = bcache_size_lookup[cidx];
    
    slab_t *slab = lookup_sanchor(simpstack_peek(&priv_slabs[cidx]),
                                  slab_t, sanc);
    if(!slab){
        slab = slab_install_new(rounded, cidx);
        if(!slab)
            return NULL;
    }

    PINT(slab->nblocks_contig);
    found = alloc_from_slab(slab);
    if(!found)
        return NULL;
    PINT(slab->nblocks_contig);
    
    if(slab_num_priv_blocks(slab) == 0){
        simpstack_pop(&priv_slabs[cidx]);
        assert(!slab_is_on_priv_stack(slab));
    }

    assert(found);
    rassert(slab_of(found)->block_size, >=, size);
    assert(aligned(found, MIN_ALIGNMENT));
    return found;
}

static
int slab_num_priv_blocks(slab_t *s){
    return s->priv_blocks.size + s->nblocks_contig;
}
 
static
int slab_is_full(slab_t *s){
    int npriv = slab_num_priv_blocks(s);
    int max = slab_max_size(s);
    assert(npriv + stack_size(&s->wayward_blocks) <= max);
    return npriv == max
        || npriv + stack_size(&s->wayward_blocks) == max;
}
 
static
int slab_is_on_priv_stack(slab_t *s){
    return slab_num_priv_blocks(s) != 0;
}

static 
unsigned int slab_max_size(slab_t *s){
    return (SLAB_SIZE - sizeof(slab_t)) / s->block_size;
}

static
block_t *alloc_from_slab(slab_t *s){
    trace3(s, p);
    
    if(s->nblocks_contig)
        return (block_t *) &s->blocks[s->block_size * --s->nblocks_contig];
    block_t *found = simpstack_pop_lookup(block_t, sanc, &s->priv_blocks);
    if(!found){
        int size;
        found = lookup_sanchor(stack_pop_all(&s->wayward_blocks, &size),
                               block_t, sanc);
        if(found)
            simpstack_replace(found->sanc.next, &s->priv_blocks, size);
    }
    assert(block_magics_valid(found, s));
    return found;
}

static
void dealloc_from_slab(block_t *b, slab_t *s){
    trace3(b, p, s, p);
    
    if(b == (block_t *) &s->blocks[s->nblocks_contig * s->block_size])
        s->nblocks_contig++;
    else
        simpstack_push(&b->sanc, &s->priv_blocks);
}

static
slab_t *slab_install_new(size_t size, int cidx){
    trace3(size, lu, cidx, d);
    
    slab_t *found = stack_pop_lookup(slab_t, sanc, &clean_slabs);
    if(found){
        log("clean");
        found->block_size = size;
        found->nblocks_contig = slab_max_size(found);
    } else {
        found = stack_pop_lookup(slab_t, sanc, &dirty_slabs[cidx]);
        if(!found){
            /* Note the use of MAP_POPULATE to escape page faults that *will*
               otherwise happen due to overcommit. */
            found = mmap(NULL, SLAB_SIZE * SLAB_ALLOC_BATCH,
                         PROT_READ | PROT_WRITE,
                         MAP_POPULATE | MAP_PRIVATE | MAP_ANONYMOUS,
                         -1, 0);
            if(!found)
                return NULL;
            
            /* Note that we start from 1. */
            for(int i = 1; i < SLAB_ALLOC_BATCH; i++){
                slab_t *extra = (slab_t*) ((uptr_t) found + i * SLAB_SIZE);
                *extra = (slab_t) FRESH_SLAB;
                stack_push(&extra->sanc, &clean_slabs);
            }
            *found = (slab_t) FRESH_SLAB;
            found->block_size = size;
            found->nblocks_contig = slab_max_size(found);

            log("new clean");
        }
    }

    assert(found->host_tid == INVALID_TID);
    assert(found->block_size == size);

    found->host_tid = pthread_self();
    simpstack_push(&found->sanc, &priv_slabs[cidx]);

    PPNT(found);
    PLUNT(found->block_size);
    
    profile_slabs(1);

    assert(aligned(found, SLAB_SIZE));
    
    return found;
}

/* static */
/* void try_freeing_head_slab(int cidx){ */
/*     sanchor_t top = simpstack_peek(&priv_slabs[cidx]); */
/*     if(priv_slabs) */
/*     s->host_tid = INVALID_TID; */
/*     stack_push(&s->sanc, &clean_slabs); */
/*     profile_slabs(-1); */
/* } */

static
void dealloc(block_t *b){
    trace2(b, p);

    slab_t *s = slab_of(b);
    int cidx = bcacheidx_of(s->block_size);

    *b = (block_t) FRESH_BLOCK;
    assert(write_block_magics(b, s));

    if(s->host_tid == pthread_self()){
        if(!slab_is_on_priv_stack(s))
            simpstack_push(&s->sanc, &priv_slabs[cidx]);
        
        dealloc_from_slab(b, s);
        if(slab_is_full(s) && priv_slabs[cidx].size >= IDEAL_FULL_SLABS)
            (void) 1;
            /* try_freeing_head_slab(cidx); */
            /* Consider freeing. */
    }
    else
        return_wayward_block(b, s, cidx);
}

static
void return_wayward_block(block_t *b, slab_t *s, int cidx){
    if(slab_is_full(s) && !slab_is_on_priv_stack(s))
        steal_or_dealloc_slab(s, cidx);
    else
        stack_push(&b->sanc, &s->wayward_blocks);
        
}

static
void steal_or_dealloc_slab(slab_t *s, int cidx){
    if(priv_slabs[cidx].size < IDEAL_FULL_SLABS){
        s->host_tid = pthread_self();
        simpstack_push(&s->sanc, &priv_slabs[cidx]);
    }
    else{
        s->host_tid = INVALID_TID;
        stack_push(&s->sanc, &clean_slabs);
    }
}

static
slab_t *slab_of(block_t *b){
    return (slab_t *) align_down_pow2(b, PAGE_SIZE);
}

/* Prototyped without args for compatibility with pthread_key_create. */
static
void free_slabs_atexit(){
    /* TODO: Obvious placeholder. */
}

static
void first_time_init(void){
    pthread_key_create(&destructor_key, free_slabs_atexit);
};

static
int dynamic_init(void){
    static __thread int init_done = 0;
    if(!init_done){
        pthread_once(&key_rdy, first_time_init);
        if(pthread_setspecific(destructor_key, (void*) !NULL))
            return -1;
        init_done = 1;
    }
    return 0;
}

static
void profile_bytes(size_t nbytes){
    return;
    profile.num_bytes_highwater =
        max(profile.num_bytes += nbytes, profile.num_bytes_highwater);
}

static
void profile_slabs(size_t nslabs){
    return;
    profile.num_slabs_highwater =
        max(profile.num_slabs += nslabs, profile.num_slabs_highwater);
}

nalloc_profile_t *get_profile(void){
    return &profile;
}

static
int write_block_magics(block_t *b, slab_t *s){
    if(!HEAP_DBG)
        return 1;
    int *magics = (int *) (b + 1);
    for(int i = 0; i < (s->block_size - sizeof(*b)) / sizeof(*magics); i++)
        magics[i] = NALLOC_MAGIC_INT;
    return 1;
}

static
int block_magics_valid(block_t *b, slab_t *s){
    if(!HEAP_DBG)
        return 1;
    int *magics = (int *) (b + 1);
    for(int i = 0; i < (s->block_size - sizeof(*b)) / sizeof(*magics); i++)
        rassert(magics[i], ==, NALLOC_MAGIC_INT);
    return 1;
}

void posix_memalign(){
    UNIMPLEMENTED();
}

void aligned_alloc(){
    UNIMPLEMENTED();
}

void *valloc(){
    UNIMPLEMENTED();
}

void *memalign(){
    UNIMPLEMENTED();
}

void *pvalloc(){
    UNIMPLEMENTED();
}
