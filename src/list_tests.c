#define MODULE LIST_TESTS

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <nalloc.h>
#include <list.h>
#include <lflist.h>
#include <global.h>

int nlists = 1;
int nthreads = 2;
int niter = 1000;
int nalloc = 1000;
int nwrites = 0;

static sem_t parent_dead;

typedef union{
    lineage_t lin;
    struct{
        lanchor_t lanc;
        flanchor flanc;
    };
} block;

type_key block_key;

void init_block(block *b){
    b->lanc = (lanchor_t) FRESH_LANCHOR;
    b->flanc = (flanchor) FRESH_FLANCHOR;
}

static __thread unsigned int seed;
void rand_init(void){
    assert(read(open("/dev/urandom", O_RDONLY), &seed, sizeof(seed)) ==
           sizeof(seed));
}

int randpcnt(int per_centum){
    return rand_r(&seed) % 100 <= umin(per_centum, 100);
}

void *reinsert_kid(lflist *lists){
    list_t privs = FRESH_LIST(privs);
    heritage block_heritage = (heritage)
        FRESH_HERITAGE(sizeof(block_key), &block_key);

    rand_init();
    sem_wait(&parent_dead);

    int nb = 0;
    for(int i = 0; i < niter; i++){
        block *b;
        if(nb < nalloc && randpcnt(10)){
            nb++;
            b = (block *)
                linalloc(&block_heritage, (void (*)(void*)) init_block);
            list_add_rear(&b->lanc, &privs);
        }

        lflist *l = &lists[rand() % nlists];
        if(randpcnt(50) && (b = cof(list_pop(&privs), block, lanc))){
            lflist_add_rear(&b->flanc, &block_heritage, l);
        }else{
            b = cof(lflist_pop_rear(&block_heritage, l), block, flanc);
            if(!b)
                continue;
            if(randpcnt(50))
                list_add_rear(&b->lanc, &privs);
            else
                list_add_front(&b->lanc, &privs);
        }
    }

    for(block *b; (b = cof(list_pop(&privs), block, lanc));)
        lflist_add_rear(&b->flanc, &block_heritage, &lists[0]);

    return (void *) nb;
}

void test_reinsert(){
    sem_t parent_dead;
    if(!sem_init(&parent_dead, 0, 0))
        LOGIC_ERROR();
    
    lflist lists[nlists];
    for(int i = 0; i < nlists; i++)
        lists[i] = (lflist) FRESH_LFLIST(lists[i]);

    pthread_t tids[nthreads];
    for(int i = 0; i < nthreads; i++)
        if(!pthread_create(&tids[i], NULL,
                           (void *(*)(void*)) reinsert_kid, lists))
            LOGIC_ERROR();
    
    for(int i = 0; i < nthreads; i++)
        sem_post(&parent_dead);

    int garbage = 0;
    for(int i = 0; i < nthreads; i++){
        void *nb;
        pthread_join(tids[i], &nb);
        garbage += (uintptr_t) nb;
    }

    heritage block_heritage = (heritage)
        FRESH_HERITAGE(sizeof(block_key), &block_key);
    for(int i = 0; i < nlists; i++)
        for(block *b; (b = cof(lflist_pop_rear(&block_heritage, &lists[i]),
                               block, flanc));
            garbage--)
            linfree(&b->lin);

    assert(!garbage);
}

int malloc_test_main(int program);

int main(int argc, char **argv){
    int program = 1, opt, do_malloc = 0;
    while( (opt = getopt(argc, argv, "t:a:o:p:w:m")) != -1 ){
        switch (opt){
        case 't':
            nthreads = atoi(optarg);
            break;
        case 'a':
            nalloc = atoi(optarg);
            break;
        case 'i':
            niter = atoi(optarg);
            break;
        case 'w':
            nwrites = atoi(optarg);
            break;
        case 'p':
            program = atoi(optarg);
            break;
        case 'm':
            do_malloc = 1;
            break;
        }
    }

    if(do_malloc){
        malloc_test_main(program);
        return 0;
    }
    
    test_reinsert();
    return 0;
}


