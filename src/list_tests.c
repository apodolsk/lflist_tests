#define MODULE LIST_TESTSM

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

/* GDB starts counting child threads at 2. Urgh. */
const uint firstborn = 2;

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

void node_init(node *b){
    b->flanc = (flanchor) FLANCHOR;
    assert(lwrite_magics(b));
}

lfstack hot_slabs = LFSTACK;
type node_t = {sizeof(node), linref_up, linref_down};
heritage *node_hs;
    

static uptr nb;
static lflist *shared;

void *reinsert_kid(uint t){
    lflist priv = LFLIST(&priv);
    tid_ = t + firstborn;
    heritage *node_h = &node_hs[t];

    rand_init();
    sem_wait(&parent_done);

    for(uint i = 0; i < niter; i++){
        log(i);
        
        if(randpcnt(10) && condxadd(&nb, nalloc) < nalloc){
            node *b = (node *) linalloc(node_h);
            assert(lmagics_valid(b));
            lflist_add_rear(flx_of(&b->flanc), &node_t, &priv);
        }

        lflist *l = &shared[rand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_pop_front(&node_t, &priv))){
            log("Pushing: ", flptr(bx));
            assert(lmagics_valid(cof(flptr(bx), node, flanc)));
            lflist_add_rear(bx, &node_t, l);
        }else{
            bx = lflist_pop_front(&node_t, l);
            node *b = cof(flptr(bx), node, flanc);
            if(!b)
                continue;
            log("Popped: ", flptr(bx));
            assert(lwrite_magics(b));
            lflist_add_rear(bx, &node_t, &priv);
        }

        log(i);
    }

    for(flx bx; flptr(bx = lflist_pop_front(&node_t, &priv));)
        lflist_add_rear(bx, &node_t, &shared[0]);

    return (void *) nb;
}

void test_reinsert(){
    sem_t parent_dead;
    if(sem_init(&parent_dead, 0, 0))
        EWTF();
    
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
        for(flx b; flptr(b = lflist_pop_front(&node_t, &shared[i])); nb--)
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

    assert(mmap(NULL, SLAB_SIZE * 8, PROT_WRITE | PROT_WRITE,
                MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0)
           != MAP_FAILED);
        
    uptr a = 1;
    assert(cas_ok((uptr) 2, &a, (uptr) 1));
    assert(a == 2);
    assert(condxadd(&a, 3) == 2);
    assert(a == 3);

    heritage hs[nthreads];
    for(uint i = 0; i < nthreads; i++)
        hs[i] = (heritage) POSIX_HERITAGE(&node_t, &hot_slabs,
                                          (void (*)(void *)) node_init);
    node_hs = hs;

    lflist lists[nlists];
    for(uint i = 0; i < nlists; i++)
        lists[i] = (lflist) LFLIST(&lists[i]);
    shared = lists;
    log(shared);

    if(do_malloc){
        malloc_test_main(program);
        return 0;
    }
    
    test_reinsert();
    return 0;
}


