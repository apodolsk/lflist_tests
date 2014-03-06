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

type *t_block = &(type){sizeof(block)};

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

typedef struct {
    int tid;
    lflist *lists;
    heritage *heritages;
} reinsert_args;

void *reinsert_kid(reinsert_args *a){
    lflist priv = FRESH_LFLIST(&priv);
    lflist *shared = a->lists;
    heritage *h = &a->heritages[a->tid];

    rand_init();
    sem_wait(&parent_done);

    int nb = 0;
    for(int i = 0; i < niter; i++){
        if(nb < nalloc && randpcnt(10)){
            nb++;
            block *b = (block *)
                linalloc(h, (void (*)(void*)) init_block);
            assert(lmagics_valid(b));
            lflist_add_rear(flx_of(&b->flanc), t_block, &priv);
        }

        lflist *l = &shared[rand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_pop_front(t_block, &priv))){
            assert(lmagics_valid(cof(flptr(bx), block, flanc)));
            lflist_add_rear(bx, t_block, l);
        }else{
            bx = lflist_pop_front(t_block, l);
            block *b = cof(flptr(bx), block, flanc);
            if(!b)
                continue;
            assert(lwrite_magics(b));
            lflist_add_rear(bx, t_block, &priv);
        }
    }

    for(flx bx; flptr(bx = lflist_pop_front(t_block, &priv));)
        lflist_add_rear(bx, t_block, &shared[0]);

    return (void *) nb;
}

void test_reinsert(){
    sem_t parent_dead;
    if(sem_init(&parent_dead, 0, 0))
        LOGIC_ERROR();
    
    lflist lists[nlists];
    for(int i = 0; i < nlists; i++)
        lists[i] = (lflist) FRESH_LFLIST(&lists[i]);

    heritage hs[nthreads];
    for(int i = 0; i < nlists; i++)
        hs[i] = (heritage) FRESH_HERITAGE(t_block);

    pthread_t tids[nthreads];
    for(int i = 0; i < nthreads; i++)
        if(pthread_create(&tids[i], NULL,
                          (void *(*)(void*)) reinsert_kid,
                          &(reinsert_args){i, lists, hs}))
            LOGIC_ERROR();
    
    for(int i = 0; i < nthreads; i++)
        sem_post(&parent_done);

    int garbage = 0;
    for(int i = 0; i < nthreads; i++){
        void *nb;
        pthread_join(tids[i], &nb);
        garbage += (uintptr_t) nb;
    }

    for(int i = 0; i < nlists; i++)
        for(flx b; flptr(b = lflist_pop_front(t_block, &lists[i]));
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


