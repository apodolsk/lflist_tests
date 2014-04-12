#define MODULE NALLOC

#include <stack.h>
#include <list.h>
#include <nalloc.h>
#include <atomics.h>
#include <global.h>

#define IDEAL_CACHED_SLABS 16
#define MIN_ALIGNMENT (2 * sizeof(uptr))
#define NALLOC_MAGIC_INT 0x01FA110C
#define ALLOC_SLAB_BATCH 16

typedef struct tyx tyx;

struct __attribute__((__aligned__(SLAB_SIZE))) slab{
    sanchor sanc;
    stack free_blocks;
    struct tyx {
        heritage *her;
        uptr linrefs;
    } tx __attribute__((__aligned__(sizeof(struct tyx))));
    cnt cold_blocks;
    pthread_t owner;
    __attribute__((__aligned__(CACHE_SIZE)))
    lfstack wayward_blocks;
    __attribute__((__aligned__(MIN_ALIGNMENT)))
    u8 blocks[];
};
#define SLAB {.free_blocks = STACK}
CASSERT(sizeof(slab) == SLAB_SIZE);

static slab *slab_new(heritage *h);
static block *alloc_from_slab(slab *s, heritage *h);
static void dealloc_from_slab(block *b, slab *s);
static unsigned int slab_max_blocks(slab *s);
static cnt slab_num_priv_blocks(slab *s);
static slab *slab_of(block *b);
static err write_block_magics(block *b, size bytes);         
static err block_magics_valid(block *b, size bytes);
static void (slab_ref_down)(slab *s);

lfstack hot_kslabs = LFSTACK;

#define PTYPE(s, ...) {.size=s, .linref_up=NULL, .linref_down=NULL} 
static const type polytypes[] = { MAP(PTYPE, _,
        16, 32, 48, 64, 80, 96, 112, 128,
        192, 256, 384, 512, 1024, MAX_BLOCK)
};

#define PHERITAGE(i, ...) POSIX_POLY_HERITAGE(&polytypes[i], no_op) , 
static __thread heritage poly_heritages[] = {
    ITERATE(PHERITAGE, _, 14)
};
CASSERT(ARR_LEN(polytypes) == 14);

#include <sys/mman.h>
slab *mmap_slabs(cnt nslabs){
    slab *s = mmap(NULL, SLAB_SIZE * nslabs, PROT_WRITE | PROT_WRITE,
                   MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    return s == MAP_FAILED ? EWTF(), NULL : s;
}

/* extern void *edata; */
/* static slab* next_cold = (slab *) const_align_up_pow2(edata, PAGE_SIZE); */
/* static const slab* max_cold = USER_MEM_START; */

/* static slab* alloc_kslabs(int nslabs){ */
/*     slab *s = condxadd(next_cold, nslabs * PAGE_SIZE, less, max_cold); */
/*     return s == max_cold ? NULL : s */
/* } */

static
heritage *poly_heritage_of(size size){
    for(uint i = 0; i < ARR_LEN(polytypes); i++)
        if(polytypes[i].size >= size)
            return &poly_heritages[i];
    EWTF();
    /* TODO: rip out */
    return NULL;
}

void *malloc(size size){
    if(size > MAX_BLOCK)
        return RARITY(), NULL;
    block *b = linalloc(poly_heritage_of(size));
    if(b)
        assert(block_magics_valid(b, poly_heritage_of(size)->t->size));
    return b;
}

void *linalloc(heritage *h){
    slab *s = cof(lfstack_pop(&h->slabs), slab, sanc);
    if(!s && !(s = slab_new(h)))
        return NULL;
    
    block *b = alloc_from_slab(s, h);
    if(slab_num_priv_blocks(s))
        lfstack_push(&s->sanc, &h->slabs);
    else
        s->owner = C;
    
    assert(b);   
    /* assert(aligned(b, MIN_ALIGNMENT)); */
    return b;
}

static
block *alloc_from_slab(slab *s, heritage *h){
    block *b;
    if(s->cold_blocks){
        b = (block *) &s->blocks[s->tx.her->t->size * --s->cold_blocks];
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
slab *slab_new(heritage *h){
    slab *s = cof(lfstack_pop(h->hot_slabs), slab, sanc);
    if(!s){
        s = h->new_slabs(ALLOC_SLAB_BATCH);
        if(!s)
            return NULL;
        for(slab *si = s; si != &s[ALLOC_SLAB_BATCH]; si++){
            *si = (slab) SLAB;
            if(si != s)
                lfstack_push(&si->sanc, h->hot_slabs);
        }
    }
    s->tx = (tyx){h, 1};
    s->cold_blocks = slab_max_blocks(s);

    size sz = h->t->size;
    for(uint i = 0; i < s->cold_blocks; i++)
        assert(write_block_magics((block *) &s->blocks[i * sz], sz));
    return s;
}

void free(void *ptr){ linfree(ptr); }
void linfree(lineage *ptr){
    block *b = (block*) ptr;
    slab *s = slab_of(b);
    heritage *h = s->tx.her;
    *b = (block){SANCHOR};
    
    if(s->owner == C && cas_ok(NULL, s->owner, C)){
        dealloc_from_slab(b, s);
        lfstack_push(&s->sanc, &h->slabs);
    }else{
        int nwb = lfstack_push(&b->sanc, &s->wayward_blocks);
        if(&s->blocks[s->tx.her->t->size * nwb] == (void *) &s[1])
            slab_ref_down(s);
    }
}

static
void dealloc_from_slab(block *b, slab *s){
    stack_push(&b->sanc, &s->free_blocks);
}

static
void (slab_ref_down)(slab *s){
    assert(s->tx.linrefs);
    if(xadd((uptr) -1, &s->tx.linrefs) == 1){
        s->tx.her = NULL;
        lfstack_push(&s->sanc, s->tx.her->hot_slabs);
    }
}

err (linref_up)(volatile void *l, type *t){
    slab *s = slab_of((void *) l);
    while(1){
        tyx tx = s->tx;
        if(tx.her->t != t)
            return EARG("Wrong type.");
        assert(tx.linrefs);
        if(cas2_ok(((tyx){tx.her, tx.linrefs + 1}), &s->tx, tx))
            return 0;
    }
}

void (linref_down)(volatile void *l){
    slab_ref_down(slab_of((void *) l));
}

err (heritage_destroy)(heritage *h){
    
}

static
unsigned int slab_max_blocks(slab *s){
    return (SLAB_SIZE - offsetof(slab, blocks)) / s->tx.her->t->size;
}

static
cnt slab_num_priv_blocks(slab *s){
    return s->free_blocks.size + s->cold_blocks;
}

static
slab *slab_of(block *b){
    return (slab *) align_down_pow2(b, SLAB_SIZE);
}

void *smalloc(size size){
    return malloc(size);
};

void sfree(void *b, size size){
    assert(slab_of(b)->tx.her->t->size >= size);
    free(b);
}

static
int write_block_magics(block *b, size bytes){
    if(!HEAP_DBG)
        return 1;
    ETODO("Gotta rethink this now that it's lineage by default.");
    int *magics = (int *) (b + 1);
    for(size i = 0; i < (bytes - sizeof(*b))/sizeof(*magics); i++)
        magics[i] = NALLOC_MAGIC_INT;
    return 1;
}

static
int block_magics_valid(block *b, size bytes){
    if(!HEAP_DBG)
        return 1;
    int *magics = (int *) (b + 1);
    for(size i = 0; i < (bytes - sizeof(*b))/sizeof(*magics); i++)
        rassert(magics[i], ==, NALLOC_MAGIC_INT);
    return 1;
}

