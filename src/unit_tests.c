/**
 * @file   unitests.c
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * A lot of knick-knacks.
 */

#define MODULE UNITESTS

#include <list.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <nalloc.h>
#include <stdlib.h>
#include <time.h>
#include <atomics.h>
#include <unistd.h>
#include <timing.h>

typedef void *(entrypoint)(void *);

#define _yield(tid) do{ (void) tid; pthread_yield();} while(0)
#define exit(val) pthread_exit((void *) val)
#define kfork(entry, arg, flag)                 \
    pthread_create(&kids[i], NULL, entry, arg)  \

static uint numhreads = 12;
static uint num_allocations = 1000;
static uint ops_mult = 1000;
static uint max_writes = 0;
static uint print_profile = 0;

#define NUM_OPS ((ops_mult * num_allocations) / numhreads)

#define NUM_STACKS 32
#define NUM_LISTS 16

#define MAX_SIZE (128)
#define MIN_SIZE (sizeof(struct tblock))

#define CAVG_BIAS .05
static __thread double malloc_cavg;
static __thread double free_cavg;

struct tblock{
    union{
        lanchor lanc;
        sanchor sanc;
    };
    int size;
    int write_start_idx;
    int magics[];
};

struct child_args{
    int parentid;
    union {
         __attribute__ ((__aligned__(64)))
         lfstack s;
    } block_stacks[NUM_STACKS];
};


/* Fun fact: without the volatile, a test-yield loop on rdy will optimize out
   the test part and will just yield in a loop forever. Meanwhile
   if(0){expression;} generates an ASM branch that's never called. Nice job,
   GCC! */
static volatile int rdy;

static __thread unsigned int seed;
void prand_init(void){
    assert(read(open("/dev/urandom", O_RDONLY), &seed, sizeof(seed)) ==
           sizeof(seed));
}
long int prand(void){
    return rand_r(&seed);
}
uint rand_percent(int per_centum){
    return (uint) prand() % 100 <= umin(per_centum, 100);
}

void write_magics(struct tblock *b, int tid){
    size max = umin(b->size - sizeof(*b), max_writes) / sizeof(b->magics[0]);
    b->write_start_idx = prand();
    for(uint i = 0; i < max; i++)
        b->magics[(b->write_start_idx + i) % max] = tid;
}

void check_magics(struct tblock *b, int tid){
    size max = umin(b->size - sizeof(*b), max_writes) / sizeof(b->magics[0]);
    for(uint i = 0; i < max; i++)
        rassert(b->magics[(b->write_start_idx + i) % max], ==, tid);
}

void *wsmalloc(size size){
    void *found;
    if(print_profile)
        malloc_cavg = CAVG_BIAS * GETTIME(found = malloc(size))
            + (1 - CAVG_BIAS) * malloc_cavg;
    else
        found = malloc(size);
    return found;
}

void wsfree(void *ptr, size ignored_future_proofing){
    (void)ignored_future_proofing;
    if(print_profile)
        free_cavg = CAVG_BIAS * GETTIME(free(ptr))
            + (1 - CAVG_BIAS) * free_cavg;
    else
        free(ptr);
}

void profile_init(void){
    void *ptr;
    if(!print_profile)
        return;
    malloc_cavg = GETTIME(ptr = malloc(42));
    assert(ptr);
    free(ptr);
}
        
void profile_report(void){}

void mt_child_rand(int parentid);

void mallocest_randsize(){
    pthread_t kids[numhreads];
    for(uint i = 0; i < numhreads; i++)
        assert(!kfork((entrypoint *) mt_child_rand,
                      (void *) itid(), KERN_ONLY));
    
    rdy = true;

    for(uint i = 0; i < numhreads; i++)
        assert(!pthread_join(kids[i], (void *[]){NULL}));
}

void mt_child_rand(int parentid){
    struct tblock *cur_block;
    list block_lists[NUM_LISTS];
    int tid = itid();

    prand_init();
    for(uint i = 0; i < NUM_LISTS; i++)
        block_lists[i] = (list) LIST(&block_lists[i]);
    
    while(rdy == false)
        _yield(parentid);

    for(uint i = 0; i < NUM_OPS; i++){
        int size;
        list *blocks = &block_lists[prand() % NUM_LISTS];
        int malloc_prob =
            blocks->size < num_allocations/2 ? 75 :
            blocks->size < num_allocations ? 50 :
            blocks->size < num_allocations * 2 ? 25 :
            0;
        
        if(rand_percent(malloc_prob)){
            size = umax(MIN_SIZE, prand() % (MAX_SIZE));
            cur_block = wsmalloc(size);
            if(!cur_block)
                continue;
            *cur_block = (struct tblock)
                { . size = size, .lanc = LANCHOR };
            write_magics(cur_block, tid);
            list_enq(&cur_block->lanc, blocks);
        }else if(blocks->size){
            cur_block = cof(list_deq(blocks), struct tblock, lanc);
            if(!cur_block)
                continue;
            check_magics(cur_block, tid);
            wsfree(cur_block, cur_block->size);
        }
    }

    for(uint i = 0; i < NUM_LISTS; i++){
        list *blocks = &block_lists[i];
        while((cur_block = cof(list_deq(blocks), struct tblock, lanc)))
            wsfree(cur_block, cur_block->size);
    }

    /* profile_report(); */
    /* exit(_gettid()); */
}

void mt_sharing_child();

void mallocest_sharing(){
    struct child_args shared;
    shared.parentid =itid();
    for(uint i = 0; i < NUM_STACKS; i++)
        shared.block_stacks[i].s = (lfstack) LFSTACK;

    pthread_t kids[numhreads];
    for(uint i = 0; i < numhreads; i++)
        if(kfork((entrypoint *) mt_sharing_child, &shared, KERN_ONLY) < 0)
            EWTF("Failed to fork.");

    rdy = true;

    for(uint i = 0; i < numhreads; i++)
        assert(!pthread_join(kids[i], (void *[]){NULL}));
}

void mt_sharing_child(struct child_args *shared){
    int parentid = shared->parentid;
    int tid =itid();
    struct tblock *cur_block;
    prand_init();

    while(rdy == false)
        _yield(parentid);

    list priv_blocks[NUM_LISTS];
    for(uint i = 0; i < NUM_LISTS; i++)
        priv_blocks[i] = (list) LIST(&priv_blocks[i]);

    uint num_blocks = 0;
    for(uint i = 0; i < NUM_OPS; i++){
        int size;
        lfstack *blocks= &shared->block_stacks[prand() % NUM_STACKS].s;
        int malloc_prob =
            num_blocks < num_allocations/2 ? 75 :
            num_blocks < num_allocations ? 50 :
            num_blocks < 2 * num_allocations ? 25 :
            0;

        if(rand_percent(malloc_prob)){
            size = umax(MIN_SIZE, prand() % (MAX_SIZE));
            cur_block = wsmalloc(size);
            if(!cur_block)
                continue;
            log2("Allocated: %", cur_block);
            *cur_block = (struct tblock)
                { .size = size, .sanc = SANCHOR };
            /* Try to trigger false sharing. */
            write_magics(cur_block, tid);
            lfstack_push(&cur_block->sanc, blocks);
            num_blocks++;
        }else{
            cur_block =
                cof(lfstack_pop(blocks), struct tblock, sanc);
            if(!cur_block)
                continue;
            log2("Claiming: %", cur_block);
            write_magics(cur_block, tid);
            cur_block->lanc = (lanchor) LANCHOR;
            list_enq(&cur_block->lanc, &priv_blocks[prand() % NUM_LISTS]);
        }

        if(rand_percent(2 * (100 - malloc_prob))){
            cur_block = cof(list_deq(&priv_blocks[prand() % NUM_LISTS]),
                            struct tblock, lanc);
            if(!cur_block)
                continue;
            log2("Freeing priv: %", cur_block);
            check_magics(cur_block, tid);
            wsfree(cur_block, cur_block->size);
            num_blocks--;
        }
    }

    profile_report();
}

void producer_child(struct child_args *shared);
void produce(struct child_args *shared);

void producerest(void){
    struct child_args shared;
    shared.parentid =itid();
    for(uint i = 0; i < NUM_STACKS; i++)
        shared.block_stacks[i].s = (lfstack) LFSTACK;

    pthread_t kids[numhreads];
    for(uint i = 0; i < numhreads; i++)
        if(kfork((entrypoint *) mt_sharing_child, &shared, KERN_ONLY) < 0)
            EWTF("Failed to fork.");

    rdy = true;

    produce(&shared);

    for(uint i = 0; i < numhreads; i++)
        assert(!pthread_join(kids[i], (void *[]){NULL}));

    for(uint i = 0; i < NUM_STACKS; i++){
        lfstack *blocks = &shared.block_stacks[i].s;
        struct tblock *cur_block;
        while((cur_block = cof(lfstack_pop(blocks), struct tblock, sanc)))
            wsfree(cur_block, cur_block->size);
    }
}

void produce(struct child_args *shared){
    int tid =itid();
    prand_init();

    stack priv_blocks = (stack) STACK;
    struct tblock *cur_block;
    uint num_blocks = 0;
    for(uint i = 0; i < NUM_OPS; i++){
        int size;
        lfstack *blocks= &shared->block_stacks[prand() % NUM_STACKS].s;
        int malloc_prob =
            num_blocks < numhreads * num_allocations/2 ? 90 :
            num_blocks < numhreads * num_allocations ? 70 :
            num_blocks < numhreads * num_allocations * 2 ? 25 :
            0;

        if(rand_percent(malloc_prob)){
            size = umax(MIN_SIZE, prand() % (MAX_SIZE));
            cur_block = wsmalloc(size);
            if(!cur_block)
                continue;
            log2("Allocated: %p", cur_block);
            *cur_block = (struct tblock)
                { .size = size, .sanc = SANCHOR };
            /* Try to trigger false sharing. */
            write_magics(cur_block, tid);
            lfstack_push(&cur_block->sanc, blocks);
            num_blocks++;
        }else {
            cur_block =
                cof(lfstack_pop(blocks), struct tblock, sanc);
            if(!cur_block)
                continue;
            log2("Claiming: %p", cur_block);
            write_magics(cur_block, tid);
            stack_push(&cur_block->sanc, &priv_blocks);
            num_blocks--;
        }

        if(rand_percent(100 - malloc_prob)){
            cur_block =
                cof(stack_pop(&priv_blocks), struct tblock, sanc);
            if(!cur_block)
                continue;
            log2("Freeing priv: %p", cur_block);
            check_magics(cur_block, tid);
            wsfree(cur_block, cur_block->size);
        }
    }
    
}

void consumer_child(struct child_args *shared){
    int parentid = shared->parentid;
    int tid =itid();
    struct tblock *cur_block;
    prand_init();

    while(rdy == false)
        _yield(parentid);

    stack priv_blocks = STACK;
    uint num_blocks = 0;
    for(uint i = 0; i < NUM_OPS; i++){
        lfstack *blocks= &shared->block_stacks[prand() % NUM_STACKS].s;
        int free_prob = 
            num_blocks < num_allocations/2 ? 25 :
            num_blocks < num_allocations ? 50 :
            num_blocks < num_allocations * 2 ? 75 :
            100;

        if(rand_percent(free_prob)){
            cur_block =
                cof(stack_pop(&priv_blocks), struct tblock, sanc);
            if(!cur_block)
                continue;
            log2("Freeing priv: %", cur_block);
            check_magics(cur_block, tid);
            wsfree(cur_block, cur_block->size);
            num_blocks--;
        } else {
            cur_block =
                cof(lfstack_pop(blocks), struct tblock, sanc);
            if(!cur_block)
                continue;
            log2("Claiming: %", cur_block);
            write_magics(cur_block, tid);
            stack_push(&cur_block->sanc, &priv_blocks);
            num_blocks++;
        }


    }

    profile_report();
}

#define NBYTES (64000 * PAGE_SIZE)
/* #define NBYTES 128 */
#define REPS 10000
volatile uptr update_mem[NBYTES];

void plain_update_kid(void){
    for(int r = 0; r < REPS; r++)
        for(uint i = 0; i < NBYTES/sizeof(*update_mem); i++)
            if(!update_mem[i])
                update_mem[i] = 1;
}

void plain_update(void){
    pthread_t kids[numhreads];
    for(uint i = 0; i < numhreads; i++)
        if(kfork((entrypoint *) plain_update_kid, NULL, KERN_ONLY) < 0)
            EWTF("Failed to fork.");
}

void cas_update_kid(void){
    for(int r = 0; r < REPS; r++)
        for(uint i = 0; i < NBYTES/sizeof(*update_mem); i++)
            (void) cas((uptr) 1, &update_mem[i], (uptr) 0);
}

void cas_update(void){
    pthread_t kids[numhreads];
    for(uint i = 0; i < numhreads; i++)
        if(kfork((entrypoint *) cas_update_kid, NULL, KERN_ONLY) < 0)
            EWTF("Failed to fork.");
}

int malloc_test_main(int program){
    unmute_log();

    extern int nthreads;
    extern int nalloc;
    extern int nwrites;
    extern int niter;

    numhreads = nthreads;
    num_allocations = nalloc;
    max_writes = nwrites;
    ops_mult = niter;

    profile_init();

    switch(program){
    case 1:
        TIME(mallocest_randsize());
        break;
    case 2:
        TIME(mallocest_sharing());
        break;
    case 3:
        TIME(producerest());
        break;
    case 4:
        TIME(plain_update());
        break;
    case 5:
        TIME(cas_update());
        break;
    }

    return 0;
}

