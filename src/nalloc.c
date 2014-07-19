/**
 * Lockfree slab allocator that can provide type-stable memory and
 * customizably local caching of free blocks, or just do malloc.
 *
 * A lineage L is a block of memory s.t there's a type T s.t. if a call to
 * linref_up(&L, &T) == 0 and no corresponding call to linref_down has yet
 * been made, then:
 * - There's a heritage H s.t. only linalloc(&H) may return &L, even if
 * linfree(&L) is called.
 * - T is unqiue.
 * - No function in this file will ever write to any part of L except for
 * its sizeof(lineage)-byte header.
 *
 * A heritage H is a cache of lineages with associated type T s.t., if
 * linalloc(&H) == L and no call to linfree(L) has been made, then
 * linref_up(&L, &T) == 0. Also, H has a function block_init s.t. for all
 * L, block_init(L) is only called the first time that linalloc(&H)
 * returns L.
 *
 * Lineages provide "type-stable" memory because, if you get a linref on a
 * lineage L, then you've associated L with a type T s.t. only calls to
 * linalloc on heritages also associated with T could return L. Combined
 * with the no-write promise, this lets you safely free and reallocate L
 * without losing guarantees about the validity of its contents, but only
 * into the limited subset of free blocks wich are associated with T.
 *
 * malloc() and co are wrappers around linalloc and linfree, using
 * heritages keyed to "polymorphic types" of fixed sizes (bullshit) and
 * no-op block_init functions.
 *
 * TODO: some heritage_destroy function is needed. Doing nothing upon
 * thread death doesn't break anything, but it leaks the _full_ slabs in
 * all thread-local heritages.
 *
 * TODO!!!: blocks must be linited before linref_up can return true. Then
 * linref_up provides guarantee that the only writes that occurred in
 * nalloc.c were to the block header and via linit.
 */

#define MODULE NALLOC

#include <stack.h>
#include <list.h>
#include <nalloc.h>
#include <config.h>
#include <atomics.h>

#define HEAP_DBG 1
#define CACHE_MAX SLAB_ALLOC_BATCH
#define NALLOC_MAGIC_INT 0x01FA110C
#define SLAB_ALLOC_BATCH 16

typedef struct tyx tyx;

static slab *slab_new(heritage *h);
static block *alloc_from_slab(slab *s, heritage *h);
static void dealloc_from_slab(block *b, slab *s);
static unsigned int slab_max_blocks(slab *s);
static cnt slab_num_priv_blocks(slab *s);
static slab *slab_of(block *b);
static err write_magics(block *b, size bytes);         
static err magics_valid(block *b, size bytes);
static void (slab_ref_down)(slab *s, lfstack *hot_slabs);

lfstack hot_slabs = LFSTACK;

#define PTYPE(s, ...) {.name=#s, .size=s, .linref_up=NULL, .linref_down=NULL} 
static const type polytypes[] = { MAP(PTYPE, _,
        16, 32, 48, 64, 80, 96, 112, 128,
        192, 256, 384, 512, 1024, MAX_BLOCK)
};

#define PHERITAGE(i, ...)                                           \
    HERITAGE(&polytypes[i], CACHE_MAX, SLAB_ALLOC_BATCH, no_op, new_slabs) ,
static heritage poly_heritages[] = {
    ITERATE(PHERITAGE, _, 14)
};

static
heritage *poly_heritage_of(size size){
    for(uint i = 0; i < ARR_LEN(polytypes); i++)
        if(polytypes[i].size >= size)
            return &poly_heritages[i];
    EWTF();
}

void *(malloc)(size size){
    if(!size)
        return NULL;
    if(size > MAX_BLOCK)
        return RARITY(), NULL;
    block *b = linalloc(poly_heritage_of(size));
    if(b)
        assert(magics_valid(b, poly_heritage_of(size)->t->size));
    return b;
}

void *(linalloc)(heritage *h){
    slab *s = cof(lfstack_pop(&h->slabs), slab, sanc);
    if(!s && !(s = slab_new(h)))
        return NULL;
    
    block *b = alloc_from_slab(s, h);
    if(slab_num_priv_blocks(s))
        lfstack_push(&s->sanc, &h->slabs);
    else{
        s->owner = C;
        s->her = h;
    }
    
    assert(b);
    assert(aligned_pow2(b, MIN_ALIGNMENT));
    return b;
}

static
block *(alloc_from_slab)(slab *s, heritage *h){
    block *b;
    if(s->cold_blocks){
        b = (block *) &s->blocks[h->t->size * --s->cold_blocks];
        assert(magics_valid(b, h->t->size));
        h->block_init(b);
        return b;
    }
    b = cof(stack_pop(&s->free_blocks), block, sanc);
    if(b)
        return b;
    s->free_blocks = lfstack_pop_all(&s->wayward_blocks);
    return cof(stack_pop(&s->free_blocks), block, sanc);
}

static
slab *(slab_new)(heritage *h){
    slab *s = cof(lfstack_pop(h->hot_slabs), slab, sanc);
    if(!s){
        s = h->new_slabs(h->slab_alloc_batch);
        if(!s)
            return NULL;
        for(slab *si = s; si != &s[h->slab_alloc_batch]; si++){
            *si = (slab) SLAB;
            if(si != s)
                lfstack_push(&si->sanc, h->hot_slabs);
        }
    }
    s->tx = (tyx){h->t, 1};
    s->her = h;
    s->cold_blocks = slab_max_blocks(s);

    size sz = h->t->size;
    for(uint i = 0; i < s->cold_blocks; i++)
        assert(write_magics((block *) &s->blocks[i * sz], sz));
    return s;
}

void (free)(void *b){
    lineage *l = (lineage *) b;
    if(!b)
        return;
    linfree(l);
}
void (linfree)(lineage *l){
    block *b = (block*) l;
    slab *s = slab_of(b);
    *b = (block){SANCHOR};

    assert(write_magics(l, s->tx.t->size));

    if(s->owner == C){
        /* TODO: yeah, you own it till you free 1 block. Not so
           great. More complicated scheme could ensure that it's not lost
           if you don't push it upon first. Consider
           lfstack_push_if(size_is,..)*/
        s->owner = NULL;
        dealloc_from_slab(b, s);
        heritage *h = s->her;
        lfstack_push(&s->sanc, &h->slabs);
    }else{
        int nwb = lfstack_push(&b->sanc, &s->wayward_blocks);
        if(&s->blocks[s->tx.t->size * (nwb + 1)] >= (u8 *) &s[1]){
            s->owner = NULL;
            slab_ref_down(s, &hot_slabs);
        }
    }
}

static
void (dealloc_from_slab)(block *b, slab *s){
    stack_push(&b->sanc, &s->free_blocks);
}

static
void (slab_ref_down)(slab *s, lfstack *hot_slabs){
    assert(s->tx.linrefs);
    if(xadd((uptr) -1, &s->tx.linrefs) == 1){
        s->owner == NULL;
        s->tx.t = NULL;
        s->her = NULL;
        lfstack_push(&s->sanc, hot_slabs);
    }
}

err (linref_up)(volatile void *l, type *t){
    slab *s = slab_of((void *) l);
    pp(t);
    for(tyx tx = s->tx;;){
        if(tx.t != t)
            return EARG("Wrong type.");
        assert(tx.linrefs);
        if(cas2_won(((tyx){t, tx.linrefs + 1}), &s->tx, &tx))
            return 0;
    }
}

void (linref_down)(volatile void *l){
    slab_ref_down(slab_of((void *) l), &hot_slabs);
}

static
unsigned int slab_max_blocks(slab *s){
    return (SLAB_SIZE - offsetof(slab, blocks)) / s->tx.t->size;
}

static
cnt slab_num_priv_blocks(slab *s){
    return s->free_blocks.size + s->cold_blocks;
}

static
slab *(slab_of)(block *b){
    return (slab *) align_down_pow2(b, SLAB_SIZE);
}

void *(smalloc)(size size){
    return malloc(size);
};

void (sfree)(void *b, size size){
    assert(slab_of(b)->tx.t->size >= size);
    free(b);
}

void *calloc(size nb, size bs){
    u8 *b = malloc(nb * bs);
    if(b)
        memset(b, 0, nb * bs);
    return b;
}

void *realloc(void *o, size size){
    u8 *b = malloc(size);
    if(b && o)
        memcpy(b, o, size);
    free(o);
    return b;
}

static
int write_magics(block *b, size bytes){
    if(!HEAP_DBG)
        return 1;
    int *magics = (int *) (b + 1);
    for(size i = 0; i < (bytes - sizeof(*b))/sizeof(*magics); i++)
        magics[i] = NALLOC_MAGIC_INT;
    return 1;
}

static
int magics_valid(block *b, size bytes){
    if(!HEAP_DBG)
        return 1;
    int *magics = (int *) (b + 1);
    for(size i = 0; i < (bytes - sizeof(*b))/sizeof(*magics); i++)
        rassert(magics[i], ==, NALLOC_MAGIC_INT);
    return 1;
}

int posix_memalign(void **mptr, size align, size sz){return -1;}
void *memalign(size align, size sz){return NULL;}
void *pvalloc(size sz){return NULL;}
void *aligned_alloc(size align, size sz){return NULL;}
void *valloc(size sz){ return NULL; }

