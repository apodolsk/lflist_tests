#define MODULE LIST_TESTS

#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <nalloc.h>
#include <lflist.h>
#include <getopt.h>
#include <prand.h>
#include <atomics.h>
#include <global.h>

uint nlists = 1;
uint nthreads = 2;
uint niter = 1000;
uint nalloc = 1000;
uint nwrites = 0;

#define MAXWRITE 100

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
    assert(nwrites <= MAXWRITE);
    for(uint i = 0; i < nwrites; i++)
        b->magics[i] = pthread_self();
    return 1;
}

int lmagics_valid(block *b){
    for(uint i = 0; i < nwrites; i++)
        assert(b->magics[i] == pthread_self());
    return 1;
}

uptr condxadd(uptr *d, uptr max){
    uptr r;
    do{
        r = *d;
        if(r >= max)
            return r;
    }while(!cas_ok(r + 1, d, r));
    return r;
}

void init_block(block *b){
    b->flanc = (flanchor) FRESH_FLANCHOR;
    assert(lwrite_magics(b));
}

typedef struct {
    int tid;
    lflist *lists;
    heritage *heritage;
} reinsert_args;

static uptr nb;

void *reinsert_kid(reinsert_args *a){
    lflist priv = FRESH_LFLIST(&priv);
    lflist *shared = a->lists;
    heritage *h = a->heritage;

    tid_ = a->tid;

    rand_init();
    sem_wait(&parent_done);

    for(uint i = 0; i < niter; i++){
        if(randpcnt(10) && condxadd(&nb, nalloc) < nalloc){
            block *b = (block *)
                linalloc(h, (void (*)(void*)) init_block);
            PPNT(b);
            assert(lmagics_valid(b));
            lflist_add_rear(flx_of(&b->flanc), t_block, &priv);
        }

        lflist *l = &shared[rand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_pop_front(t_block, &priv))){
            log("Pushing: %p", flptr(bx));
            assert(lmagics_valid(cof(flptr(bx), block, flanc)));
            lflist_add_rear(bx, t_block, l);
        }else{
            bx = lflist_pop_front(t_block, l);
            block *b = cof(flptr(bx), block, flanc);
            if(!b)
                continue;
            log("Popped: %p", flptr(bx));
            assert(lwrite_magics(b));
            lflist_add_rear(bx, t_block, &priv);
        }

        PINT(i);
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
    for(uint i = 0; i < nlists; i++)
        lists[i] = (lflist) FRESH_LFLIST(&lists[i]);

    heritage hs[nthreads];
    reinsert_args args[nthreads];
    for(uint i = 0; i < nthreads; i++){
        hs[i] = (heritage) FRESH_HERITAGE(t_block);
        args[i] = (reinsert_args){i, lists, &hs[i]};
    }

    pthread_t tids[nthreads];
    for(uint i = 0; i < nthreads; i++)
        if(pthread_create(&tids[i], NULL,
                          (void *(*)(void*)) reinsert_kid, &args[i]))
            LOGIC_ERROR();
    
    for(uint i = 0; i < nthreads; i++)
        sem_post(&parent_done);
    for(uint i = 0; i < nthreads; i++)
        pthread_join(tids[i], NULL);
    for(uint i = 0; i < nlists; i++)
        for(flx b; flptr(b = lflist_pop_front(t_block, &lists[i])); nb--)
            linfree(&cof(flptr(b), block, flanc)->lin);

    assert(!nb);
}

void test_basic(){

}

int malloc_test_main(int program);

int main(int argc, char **argv){
    int program = 1, opt, do_malloc = 0;
    while( (opt = getopt(argc, argv, "t:l:a:i:o:p:w:m")) != -1 ){
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


