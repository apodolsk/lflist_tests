/**
 * @file   unit_tests.c
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * A lot of knick-knacks.
 */

#define MODULE UNIT_TESTS

#include <list.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <nalloc.h>
#include <stdlib.h>
#include <time.h>
#include <asm_util.h>
#include <global.h>
#include <unistd.h>

typedef void *(entrypoint_t)(void *);

#define _yield(tid) do{ (void) tid; pthread_yield();} while(0)
#define exit(val) pthread_exit((void *) val)
#define kfork(entry, arg, flag)                 \
    pthread_create(&kids[i], NULL, entry, arg)  \

static int num_threads = 12;
static int num_allocations = 1000;
static int ops_mult = 1000;
static int max_writes = 0;
static int print_profile = 0;

#define NUM_OPS ((ops_mult * num_allocations) / num_threads)

#define NUM_STACKS 32
#define NUM_LISTS 16

#define MAX_SIZE (128)
#define MIN_SIZE (sizeof(struct tblock_t))

#define CAVG_BIAS .05
static __thread double malloc_cavg;
static __thread double free_cavg;

struct tblock_t{
    union{
        lanchor_t lanc;
        sanchor_t sanc;
    };
    int size;
    int write_start_idx;
    int magics[];
};

struct child_args{
    int parent_tid;
    union {
         __attribute__ ((__aligned__(64)))
        lfstack_t s;
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
int rand_percent(int per_centum){
    return prand() % 100 <= umin(per_centum, 100);
}

void write_magics(struct tblock_t *b, int tid){
    size_t max = umin(b->size - sizeof(*b), max_writes) / sizeof(b->magics[0]);
    b->write_start_idx = prand();
    for(int i = 0; i < max; i++)
        b->magics[(b->write_start_idx + i) % max] = tid;
}

void check_magics(struct tblock_t *b, int tid){
    size_t max = umin(b->size - sizeof(*b), max_writes) / sizeof(b->magics[0]);
    for(int i = 0; i < max; i++)
        rassert(b->magics[(b->write_start_idx + i) % max], ==, tid);
}

void *wsmalloc(size_t size){
    void *found;
    if(print_profile)
        malloc_cavg = CAVG_BIAS * GETTIME(found = malloc(size))
            + (1 - CAVG_BIAS) * malloc_cavg;
    else
        found = malloc(size);
    return found;
}

void wsfree(void *ptr, size_t ignored_future_proofing){
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

/* Avoiding IFDEF catastrophe with the magic of weak symbols. */
__attribute__ ((weak))
nalloc_profile_t *get_profile();
        
void profile_report(void){
    static pthread_mutex_t report_lock;

    if(!print_profile)
        return;

    pthread_mutex_lock(&report_lock);
        
    PFLT(malloc_cavg);
    PFLT(free_cavg);
    
    if(get_profile){
        nalloc_profile_t *prof = get_profile();
        PLUNT(prof->num_bytes_highwater);
        PLUNT(prof->num_slabs_highwater);
    }

    pthread_mutex_unlock(&report_lock);
}

void mt_child_rand(int parent_tid);

void malloc_test_randsize(){
    trace();

    pthread_t kids[num_threads];
    for(int i = 0; i < num_threads; i++)
        assert(!kfork((entrypoint_t *) mt_child_rand,
                      (void *) _gettid(), KERN_ONLY));
    
    rdy = TRUE;

    for(int i = 0; i < num_threads; i++)
        assert(!pthread_join(kids[i], (void *[]){NULL}));
}

void mt_child_rand(int parent_tid){
    trace2(parent_tid, d);

    struct tblock_t *cur_block;
    list_t block_lists[NUM_LISTS];
    int tid = _gettid();

    prand_init();
    for(int i = 0; i < NUM_LISTS; i++)
        block_lists[i] = (list_t) FRESH_LIST;
    
    while(rdy == FALSE)
        _yield(parent_tid);

    for(int i = 0; i < NUM_OPS; i++){
        int size;
        list_t *blocks = &block_lists[prand() % NUM_LISTS];
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
            *cur_block = (struct tblock_t)
                { . size = size, .lanc = FRESH_LANCHOR };
            write_magics(cur_block, tid);
            list_add_rear(&cur_block->lanc, blocks);
        }else if(blocks->size){
            cur_block = list_pop_lookup(struct tblock_t, lanc, blocks);
            if(!cur_block)
                continue;
            check_magics(cur_block, tid);
            wsfree(cur_block, cur_block->size);
        }
    }

    for(int i = 0; i < NUM_LISTS; i++){
        list_t *blocks = &block_lists[i];
        while((cur_block = list_pop_lookup(struct tblock_t, lanc, blocks)))
            wsfree(cur_block, cur_block->size);
    }

    profile_report();
    /* exit(_gettid()); */
}

void mt_sharing_child();

void malloc_test_sharing(){
    trace();

    struct child_args shared;
    shared.parent_tid = _gettid();
    for(int i = 0; i < NUM_STACKS; i++)
        shared.block_stacks[i].s = (lfstack_t) FRESH_STACK;

    pthread_t kids[num_threads];
    for(int i = 0; i < num_threads; i++)
        if(kfork((entrypoint_t *) mt_sharing_child, &shared, KERN_ONLY) < 0)
            LOGIC_ERROR("Failed to fork.");

    rdy = TRUE;

    for(int i = 0; i < num_threads; i++)
        assert(!pthread_join(kids[i], (void *[]){NULL}));
}

void mt_sharing_child(struct child_args *shared){
    int parent_tid = shared->parent_tid;
    int tid = _gettid();
    struct tblock_t *cur_block;
    prand_init();

    while(rdy == FALSE)
        _yield(parent_tid);

    list_t priv_blocks[NUM_LISTS];
    for(int i = 0; i < NUM_LISTS; i++)
        priv_blocks[i] = (list_t) FRESH_LIST;

    int num_blocks = 0;
    for(int i = 0; i < NUM_OPS; i++){
        int size;
        lfstack_t *blocks= &shared->block_stacks[prand() % NUM_STACKS].s;
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
            log2("Allocated: %p", cur_block);
            *cur_block = (struct tblock_t)
                { .size = size, .sanc = FRESH_SANCHOR };
            /* Try to trigger false sharing. */
            write_magics(cur_block, tid);
            stack_push(&cur_block->sanc, blocks);
            num_blocks++;
        }else{
            cur_block =
                stack_pop_lookup(struct tblock_t, sanc, blocks);
            if(!cur_block)
                continue;
            log2("Claiming: %p", cur_block);
            write_magics(cur_block, tid);
            cur_block->lanc = (lanchor_t) FRESH_LANCHOR;
            list_add_front(&cur_block->lanc, &priv_blocks[prand() % NUM_LISTS]);
        }

        if(rand_percent(2 * (100 - malloc_prob))){
            cur_block = list_pop_lookup(struct tblock_t, lanc,
                                        &priv_blocks[prand() % NUM_LISTS]);
            if(!cur_block)
                continue;
            log2("Freeing priv: %p", cur_block);
            check_magics(cur_block, tid);
            wsfree(cur_block, cur_block->size);
            num_blocks--;
        }
    }

    profile_report();
}

void producer_child(struct child_args *shared);
void produce(struct child_args *shared);

void producer_test(void){
    trace();

    struct child_args shared;
    shared.parent_tid = _gettid();
    for(int i = 0; i < NUM_STACKS; i++)
        shared.block_stacks[i].s = (lfstack_t) FRESH_STACK;

    pthread_t kids[num_threads];
    for(int i = 0; i < num_threads; i++)
        if(kfork((entrypoint_t *) mt_sharing_child, &shared, KERN_ONLY) < 0)
            LOGIC_ERROR("Failed to fork.");

    rdy = TRUE;

    produce(&shared);

    for(int i = 0; i < num_threads; i++)
        assert(!pthread_join(kids[i], (void *[]){NULL}));

    for(int i = 0; i < NUM_STACKS; i++){
        lfstack_t *blocks = &shared.block_stacks[i].s;
        struct tblock_t *cur_block;
        FOR_EACH_SPOP_LOOKUP(cur_block, struct tblock_t, sanc, blocks)
            wsfree(cur_block, cur_block->size);
    }
}

void produce(struct child_args *shared){
    int tid = _gettid();
    prand_init();

    simpstack_t priv_blocks = (simpstack_t) FRESH_SIMPSTACK;
    struct tblock_t *cur_block;
    int num_blocks = 0;
    for(int i = 0; i < NUM_OPS; i++){
        int size;
        lfstack_t *blocks= &shared->block_stacks[prand() % NUM_STACKS].s;
        int malloc_prob =
            num_blocks < num_threads * num_allocations/2 ? 90 :
            num_blocks < num_threads * num_allocations ? 70 :
            num_blocks < num_threads * num_allocations * 2 ? 25 :
            0;

        if(rand_percent(malloc_prob)){
            size = umax(MIN_SIZE, prand() % (MAX_SIZE));
            cur_block = wsmalloc(size);
            if(!cur_block)
                continue;
            log2("Allocated: %p", cur_block);
            *cur_block = (struct tblock_t)
                { .size = size, .sanc = FRESH_SANCHOR };
            /* Try to trigger false sharing. */
            write_magics(cur_block, tid);
            stack_push(&cur_block->sanc, blocks);
            num_blocks++;
        }else {
            cur_block =
                stack_pop_lookup(struct tblock_t, sanc, blocks);
            if(!cur_block)
                continue;
            log2("Claiming: %p", cur_block);
            write_magics(cur_block, tid);
            simpstack_push(&cur_block->sanc, &priv_blocks);
            num_blocks--;
        }

        if(rand_percent(100 - malloc_prob)){
            cur_block =
                simpstack_pop_lookup(struct tblock_t, sanc, &priv_blocks);
            if(!cur_block)
                continue;
            log2("Freeing priv: %p", cur_block);
            check_magics(cur_block, tid);
            wsfree(cur_block, cur_block->size);
        }
    }
    
}

void consumer_child(struct child_args *shared){
    int parent_tid = shared->parent_tid;
    int tid = _gettid();
    struct tblock_t *cur_block;
    prand_init();

    while(rdy == FALSE)
        _yield(parent_tid);

    lfstack_t priv_blocks = FRESH_STACK;
    int num_blocks = 0;
    for(int i = 0; i < NUM_OPS; i++){
        lfstack_t *blocks= &shared->block_stacks[prand() % NUM_STACKS].s;
        int free_prob = 
            num_blocks < num_allocations/2 ? 25 :
            num_blocks < num_allocations ? 50 :
            num_blocks < num_allocations * 2 ? 75 :
            100;

        if(rand_percent(free_prob)){
            cur_block =
                stack_pop_lookup(struct tblock_t, sanc, &priv_blocks);
            if(!cur_block)
                continue;
            log2("Freeing priv: %p", cur_block);
            check_magics(cur_block, tid);
            wsfree(cur_block, cur_block->size);
            num_blocks--;
        } else {
            cur_block =
                stack_pop_lookup(struct tblock_t, sanc, blocks);
            if(!cur_block)
                continue;
            log2("Claiming: %p", cur_block);
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
volatile int64_t update_mem[NBYTES];

void plain_update_kid(void){
    for(int r = 0; r < REPS; r++)
        for(int i = 0; i < NBYTES/sizeof(int64_t); i++)
            if(!update_mem[i])
                update_mem[i] = 1;
}

void plain_update(void){
    pthread_t kids[num_threads];
    for(int i = 0; i < num_threads; i++)
        if(kfork((entrypoint_t *) plain_update_kid, NULL, KERN_ONLY) < 0)
            LOGIC_ERROR("Failed to fork.");
}

void cas_update_kid(void){
    for(int r = 0; r < REPS; r++)
        for(int i = 0; i < NBYTES/sizeof(int64_t); i++)
            cmpxchg64b(1, &update_mem[i], 0);
}

void cas_update(void){
    pthread_t kids[num_threads];
    for(int i = 0; i < num_threads; i++)
        if(kfork((entrypoint_t *) cas_update_kid, NULL, KERN_ONLY) < 0)
            LOGIC_ERROR("Failed to fork.");
}

int main(int argc, char **argv){
    unmute_log();

    int program = 1;

    int opt;
    while( (opt = getopt(argc, argv, "t:a:o:p:w:l")) != -1 ){
        switch (opt){
        case 't':
            num_threads = atoi(optarg);
            break;
        case 'a':
            num_allocations = atoi(optarg);
            break;
        case 'o':
            ops_mult = atoi(optarg);
            break;
        case 'p':
            program = atoi(optarg);
            break;
        case 'w':
            max_writes = atoi(optarg);
            break;
        case 'l':
            print_profile = 1;
            break;
        }
    }

    PINT(print_profile);

    profile_init();

    switch(program){
    case 1:
        TIME(malloc_test_randsize());
        break;
    case 2:
        TIME(malloc_test_sharing());
        break;
    case 3:
        TIME(producer_test());
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

