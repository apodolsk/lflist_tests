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
#include <sys/mman.h>

uint nlists = 1;
uint nthreads = 2;
uint niter = 1000;
uint nalloc = 1000;
uint nwrites = 0;

#define MAXWRITE 100

static sem_t parent_done;

typedef union{
    lineage lin;
    struct{
        flanchor flanc;
        pthread_t magics[MAXWRITE];
    };
} node;

int lwrite_magics(node *b){
    assert(nwrites <= MAXWRITE);
    for(uint i = 0; i < nwrites; i++)
        b->magics[i] = pthread_self();
    return 1;
}

int lmagics_valid(node *b){
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

slab *mmap_slabs(cnt nslabs){
    slab *s = mmap(NULL, SLAB_SIZE * nslabs,
                   PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    return s == MAP_FAILED ? NULL : s;
}

void node_init(node *b){
    b->flanc = (flanchor) FLANCHOR;
    assert(lwrite_magics(b));
}

lfstack hot_slabs = LFSTACK;
type node_t = {sizeof(node), linref_up, linref_down};
__thread heritage node_h = HERITAGE(&node_t, &hot_slabs,
                                    (void (*)(void *))node_init, mmap_slabs);

static uptr nb;
static lflist *shared;

void *reinsert_kid(uint t){
    lflist priv = LFLIST(&priv);
    tid_ = t;

    rand_init();
    sem_wait(&parent_done);

    for(uint i = 0; i < niter; i++){
        if(randpcnt(10) && condxadd(&nb, nalloc) < nalloc){
            node *b = (node *)
                linalloc(&node_h);
            PPNT(b);
            assert(lmagics_valid(b));
            lflist_add_rear(flx_of(&b->flanc), &node_t, &priv);
        }

        lflist *l = &shared[rand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_pop_front(&node_t, &priv))){
            log("Pushing: %p", flptr(bx));
            assert(lmagics_valid(cof(flptr(bx), node, flanc)));
            lflist_add_rear(bx, &node_t, l);
        }else{
            bx = lflist_pop_front(&node_t, l);
            node *b = cof(flptr(bx), node, flanc);
            if(!b)
                continue;
            log("Popped: %p", flptr(bx));
            assert(lwrite_magics(b));
            lflist_add_rear(bx, &node_t, &priv);
        }

        PINT(i);
    }

    for(flx bx; flptr(bx = lflist_pop_front(&node_t, &priv));)
        lflist_add_rear(bx, &node_t, &shared[0]);

    return (void *) nb;
}

void test_reinsert(){
    sem_t parent_dead;
    if(sem_init(&parent_dead, 0, 0))
        EWTF();
    
    lflist lists[nlists];
    for(uint i = 0; i < nlists; i++)
        lists[i] = (lflist) LFLIST(&lists[i]);

    /* heritage hs[nthreads]; */
    /* reinsert_args args[nthreads]; */
    /* for(uint i = 0; i < nthreads; i++){ */
    /*     hs[i] = (heritage) HERITAGE(&node_t); */
    /*     args[i] = (reinsert_args){i, lists, &hs[i]}; */
    /* } */

    pthread_t tids[nthreads];
    for(uint i = 0; i < nthreads; i++)
        if(pthread_create(&tids[i], NULL,
                          (void *(*)(void*)) reinsert_kid, (void *) i))
            EWTF();
    
    for(uint i = 0; i < nthreads; i++)
        sem_post(&parent_done);
    for(uint i = 0; i < nthreads; i++)
        pthread_join(tids[i], NULL);
    for(uint i = 0; i < nlists; i++)
        for(flx b; flptr(b = lflist_pop_front(&node_t, &lists[i])); nb--)
            linfree(&cof(flptr(b), node, flanc)->lin);

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

    lflist lists[nlists];
    for(uint i = 0; i < nlists; i++)
        lists[i] = (lflist) LFLIST(&lists[i]);
    shared = lists;

    if(do_malloc){
        malloc_test_main(program);
        return 0;
    }
    
    test_reinsert();
    return 0;
}


