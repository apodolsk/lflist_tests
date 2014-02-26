/**
 * @file   nalloc.c
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * @date   Sun Jun 30 23:56:44 2013
 *
 * This is a simplified version of my 418 thread-caching allocator. The way I
 * use malloc makes it very unlikely for blocks to be thread-local, and I
 * think this will be true of most of the allocations that a kernel will have
 * to make. So, unlike my 418 allocator, this is CPU-local. Since I have just
 * one CPU now, I stripped out the bits related to inter-CPU/thread block
 * migrations.
 *
 * Going from per-thread to per-cpu requires some changes: mainly, you need to
 * protect per-CPU slabs from conflicting allocations on the same CPU. One
 * option is to remove a slab from the slab cache while allocating from it, in
 * order to guarantee that it contains a free block of a given size (it gets
 * complicated if you try to be fully lockfree within a CPU without this
 * guarantee). But then you may wind up with underutilization proportional to
 * NTHREADS. Another option is to keep per-CPU sets of blocks of a size class,
 * rather than sets of slabs of a size class. This should cause fragmentation,
 * since you're no longer exhausting slabs one by one, and my 418 experience
 * makes me worry about resulting poor locality. But I think that tcmalloc
 * does exactly this and gets away with it.
 *
 * I disable interrupts now, as the easiest option. To make a more defensible
 * decision, I'll have to see whether my understanding of tcmalloc is right,
 * whether interrupt latency is an issue, and how the cost of disabling
 * interrupts compares to the cost of cache line locking.
 *
 * Slabs might not be appropriate for string = malloc(rare_size)
 * allocations. You might want to coalesce those. I've done that lockfree too,
 * and I'm sure that it can work per-CPU and in the kernel.
 */

#define MODULE NALLOC

#include <stack.h>
#include <list.h>
#include <nalloc.h>
#include <atomics.h>
#include <global.h>

#include <sys/mman.h>

static int cacheidx_of(size_t size);
static void *cache_alloc(size_t bytes, simpstack *cache,
                         void (*slab_init)(slab_t *s, void *arg), void *arg,
                         void (*destructor)(slab_t *s));
static void cache_dealloc(block_t *b, slab_t *s, simpstack *cache);
static slab_t *slab_new(size_t size);
static void slab_free(slab_t *s);
static unsigned int slab_max_blocks(slab_t *s);
static block_t *alloc_from_slab(slab_t *s);
static void dealloc_from_slab(block_t *b, slab_t *s);
static bool slab_full(slab_t *s);
static bool slab_empty(slab_t *s);
static slab_t *slab_of(block_t *b);
static int write_block_magics(block_t *b, size_t bytes);
static int block_magics_valid(block_t *b, size_t bytes);

/* TODO */
void disable_interrupts(){}
void enable_interrupts(){}

static const int cache_sizes[] = {
    16, 32, 48, 64, 80, 96, 112, 128,
    192, 256,
    384, 512, 
    1024, 2048,
    4096, MAX_BLOCK
};

static __thread simpstack caches[ARR_LEN(cache_sizes)];
static stack hot_slabs = FRESH_STACK;

block_t *block_of(void *addr){
    slab_t *s = (slab_t *) align_down_pow2(addr, SLAB_SIZE);
    ptrdiff_t offset = (uint8_t *) addr - s->blocks;
    return (block_t *) s->blocks + align_down(offset, s->block_size);
}

void *_malloc(size_t size){
    if(size > MAX_BLOCK)
        return NULL;
    int cidx = cacheidx_of(size);
    block_t *b =
        cache_alloc(cache_sizes[cidx], &caches[cidx], no_op, NULL, &slab_free);
    if(b)
        assert(block_magics_valid(b, cache_sizes[cidx]));
    return b;
}

void _free(void *buf){
    if(!buf)
        return;
    block_t *b = (block_t *) buf;
    slab_t *s = slab_of(b);
    assert(write_block_magics(b, s->block_size));

    cache_dealloc(b, s, &caches[cacheidx_of(s->block_size)]);
}

static 
void *cache_alloc(size_t bytes, simpstack *cache,
                  void (*slab_init)(slab_t *s, void *arg), void *arg,
                  void (*slab_release)(slab_t *s))
{
    /* TODO: Could reduce the scope of disable_interrupts() here. */
    disable_interrupts();
    slab_t *s = cof(simpstack_peek(cache), slab_t, sanc);
    if(!s){
        s = slab_new(bytes);
        if(!s)
            return NULL;
        slab_init(s, arg);
        simpstack_push(&s->sanc, cache);
    } else if(slab_full(s) && cache->size > IDEAL_CACHED_SLABS){
        if(cof(simpstack_pop(cache), slab_t, sanc) != s)
            LOGIC_ERROR();
        slab_release(s);
    }
    
    block_t *found = alloc_from_slab(s);
    if(slab_empty(s))
        if(cof(simpstack_pop(cache), slab_t, sanc) != s)
            LOGIC_ERROR();

    enable_interrupts();
    
    assert(found);
    assert(slab_of(found)->block_size >= bytes);
    assert(aligned(found, MIN_ALIGNMENT));
    return found;
}

static inline
void cache_dealloc(block_t *b, slab_t *s, simpstack *cache){
    *b = (block_t) FRESH_BLOCK;

    disable_interrupts();
    if(slab_empty(s))
        simpstack_push(&s->sanc, cache);
    dealloc_from_slab(b, s);
    enable_interrupts();
}

static
int cacheidx_of(size_t size){
    for(int i = 0; i < ARR_LEN(cache_sizes); i++)
        if(cache_sizes[i] >= size)
            return i;
    LOGIC_ERROR();
    return -1;
}

static
slab_t *slab_new(size_t size){
    slab_t *s = cof(stack_pop(&hot_slabs), slab_t, sanc);
    if(s)
        return s;
    
    s = mmap(NULL, SLAB_SIZE, PROT_WRITE,
             MAP_SHARED | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if(s == MAP_FAILED)
        return NULL;
    
    /* if(!s && lowest_cold_slab < (slab_t *) HEAP_HIGH){ */
    /*     static slab_t *lowest_cold_slab = (slab_t *) HEAP_LOW; */
    /*     s = (slab_t *) xadd(SLAB_SIZE, (int*) &lowest_cold_slab); */
    /*     if(s >= (slab_t *) HEAP_HIGH){ */
    /*         lowest_cold_slab = (slab_t *) HEAP_HIGH; */
    /*         return NULL; */
    /*     } */
    /* } */

    *s = (slab_t) FRESH_SLAB;
    s->block_size = size;
    s->nblocks_contig = slab_max_blocks(s);
    for(int i = 0; i < slab_max_blocks(s); i++)
        assert(write_block_magics((block_t *) &s->blocks[i * size], size));
    
    return s;
}

static
void slab_free(slab_t *s){
    stack_push(&s->sanc, &hot_slabs);
}

static 
unsigned int slab_max_blocks(slab_t *s){
    return (SLAB_SIZE - sizeof(slab_t)) / s->block_size;
}

static
block_t *alloc_from_slab(slab_t *s){
    if(s->nblocks_contig)
        return (block_t *) &s->blocks[s->block_size * --s->nblocks_contig];
    block_t *found = cof(simpstack_pop(&s->free_blocks), block_t, sanc);
    return found;
}

static
void dealloc_from_slab(block_t *b, slab_t *s){
    if(b == (block_t *) &s->blocks[s->nblocks_contig * s->block_size])
        s->nblocks_contig++;
    else
        simpstack_push(&b->sanc, &s->free_blocks);
}

static
bool slab_full(slab_t *s){
    return
        s->free_blocks.size + s->nblocks_contig == slab_max_blocks(s);
}

static
bool slab_empty(slab_t *s){
    return
        !s->free_blocks.size && !s->nblocks_contig;
}

static
slab_t *slab_of(block_t *b){
    return (slab_t *) align_down_pow2(b, SLAB_SIZE);
}

static
int write_block_magics(block_t *b, size_t bytes){
    if(!HEAP_DBG)
        return 1;
    int *magics = (int *) (b + 1);
    for(int i = 0; i < (bytes - sizeof(*b))/sizeof(*magics); i++)
        magics[i] = NALLOC_MAGIC_INT;
    return 1;
}

static
int block_magics_valid(block_t *b, size_t bytes){
    if(!HEAP_DBG)
        return 1;
    int *magics = (int *) (b + 1);
    for(int i = 0; i < (bytes - sizeof(*b))/sizeof(*magics); i++)
        rassert(magics[i], ==, NALLOC_MAGIC_INT);
    return 1;
}

void linslab_ref_up(slab_t *s, void *t){
    s->type = (heritage_t *) t;
    xadd(1, &s->linrefs);
}

void linslab_release(slab_t *s){
    if(xadd(-1, &s->linrefs) == 1){
        s->type = NULL;
        stack_push(&s->sanc, &hot_slabs);
    }
}

lineage_t *linalloc(heritage_t *t){
    return
        cache_alloc(t->size_of, &t->slabs,
                    linslab_ref_up, t,
                    linslab_release);
}

void linfree(lineage_t *l){
    block_t *b = (block_t *) l;
    slab_t *s = slab_of(b);
    /* TODO: should be deleted for non-reserved header case. */
    assert(sanchor_unused(&b->sanc));
    cache_dealloc(b, s, &s->type->slabs);
}

int linref_up(lineage_t *l, heritage_t *h){
    slab_t *s = slab_of(l);
    hxchg_t old, new;
    do{
        new.hx = old.hx = s->hx;
        if(old.type->key == h->key)
            return INPUT_ERROR("Wrong type.");
        assert(old.linrefs);
        new.linrefs++;
    }while(!cas_ok(new.hx, &s->hx, old.hx));
    return 0;
}

void linref_down(lineage_t *l){
    linslab_release(slab_of(l));
}

void *_smemalign(size_t alignment, size_t size){
    if(alignment == PAGE_SIZE && size == PAGE_SIZE)
        return (void *) align_up_pow2(_malloc(MAX_BLOCK), PAGE_SIZE);
    UNIMPLEMENTED();
    return NULL;
}

void *_smalloc(size_t size){
    return _malloc(size);
};

void _sfree(void *b, size_t size){
    if(size == PAGE_SIZE){
        slab_t *s = slab_of(b);
        assert(s->block_size == MAX_BLOCK);
        _free(&s->blocks[0]);
        return;
    }
    
    assert(slab_of(b)->block_size >= size);
    _free(b);
}
