#define MODULE LIST_TESTS

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <nalloc.h>
#include <lflist.h>
#include <global.h>

int nlists = 1;
int nthreads = 2;
int niter = 1000;
int nalloc = 1000;
int nwrites = 0;

#define MAXWRITE 64

static sem_t parent_done;

typedef union{
    lineage_t lin;
    struct{
        flanchor flanc;
        pthread_t magics[MAXWRITE];
    };
} block;

type_key block_key;

int lwrite_magics(block *b){
    assert(nwrites < MAXWRITE);
    for(int i = 0; i < nwrites; i++)
        b->magics[i] = pthread_self();
    return 1;
}

int lmagics_valid(block *b){
    for(int i = 0; i < nwrites; i++)
        assert(b->magics[i] == pthread_self());
    return 1;
}

void init_block(block *b){
    b->flanc = (flanchor) FRESH_FLANCHOR;
    assert(lwrite_magics(b));
}

static __thread uint seed;
void rand_init(void){
    assert(read(open("/dev/urandom", O_RDONLY), &seed, sizeof(seed)) ==
           sizeof(seed));
}

uint randpcnt(uint per_centum){
    return (uint) rand_r(&seed) % 100 <= umin(per_centum, 100);
}

void *reinsert_kid(lflist *lists){
    lflist privs = FRESH_LFLIST(&privs);
    heritage block_heritage = (heritage)
        FRESH_HERITAGE(sizeof(block), &block_key);

    rand_init();
    sem_wait(&parent_done);

    int nb = 0;
    for(int i = 0; i < niter; i++){
        if(nb < nalloc && randpcnt(10)){
            nb++;
            block *b = (block *)
                linalloc(&block_heritage, (void (*)(void*)) init_block);
            assert(lmagics_valid(b));
            lflist_add_rear(flx_of(&b->flanc), &block_heritage, &privs);
        }

        lflist *l = &lists[rand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_pop_front(&block_heritage, &privs))){
            assert(lmagics_valid(cof(flptr(bx), block, flanc)));
            lflist_add_rear(bx, &block_heritage, l);
        }else{
            bx = lflist_pop_front(&block_heritage, l);
            block *b = cof(flptr(bx), block, flanc);
            if(!b)
                continue;
            assert(lwrite_magics(b));
            lflist_add_rear(bx, &block_heritage, &privs);
        }
    }

    for(flx bx; flptr(bx = lflist_pop_front(&block_heritage, &privs));)
        lflist_add_rear(bx, &block_heritage, &lists[0]);

    return (void *) nb;
}

void test_reinsert(){
    sem_t parent_dead;
    if(sem_init(&parent_dead, 0, 0))
        LOGIC_ERROR();
    
    lflist lists[nlists];
    for(int i = 0; i < nlists; i++)
        lists[i] = (lflist) FRESH_LFLIST(&lists[i]);

    pthread_t tids[nthreads];
    for(int i = 0; i < nthreads; i++)
        if(pthread_create(&tids[i], NULL,
                          (void *(*)(void*)) reinsert_kid, lists))
            LOGIC_ERROR();
    
    for(int i = 0; i < nthreads; i++)
        sem_post(&parent_done);

    int garbage = 0;
    for(int i = 0; i < nthreads; i++){
        void *nb;
        pthread_join(tids[i], &nb);
        garbage += (uintptr_t) nb;
    }

    heritage block_heritage = (heritage)
        FRESH_HERITAGE(sizeof(block_key), &block_key);
    for(int i = 0; i < nlists; i++)
        for(flx b; flptr(b = lflist_pop_front(&block_heritage, &lists[i]));
            garbage--){
            block *bp = cof(flptr(b), block, flanc);
            linfree(&bp->lin);
        }

    assert(!garbage);
}

void test_basic(){

}

int malloc_test_main(int program);

int main(int argc, char **argv){
    int program = 1, opt, do_malloc = 0;
    while( (opt = getopt(argc, argv, "t:l:a:o:p:w:m")) != -1 ){
        switch (opt){
        case 't':
            nthreads = atoi(optarg);
            break;
        case 'l':
            nlists = atoi(optarg);
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


