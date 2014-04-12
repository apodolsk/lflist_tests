#define MODULE LIST_TESTSM

#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <nalloc.h>
#include <lflist.h>
#include <list.h>
#include <getopt.h>
#include <prand.h>
#include <atomics.h>
#include <global.h>
#include <sys/mman.h>
#include <unistd.h>

#define TS LFLIST_TS

uint nlists = 2;
uint nthreads = 2;
uint niter = 1000;
uint nalloc = 100;
uint nwrites = 0;

/* GDB starts counting child threads at 2. Urgh. */
const uint firstborn = 2;

#define MAXWRITE 100

static sem_t parent_done;

typedef union{
    lineage lin;
    struct{
        lanchor lanc;
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

dptr condxadd(volatile dptr *d, dptr max){
    dptr r, f = *d;
    do{
        r = f;
        if(r >= max)
            return r;
        f = cas2(r + 1, d, r);
    }while(f != r);
    return r;
}

void node_init(node *b){
    b->flanc = (flanchor) FLANCHOR;
    b->lanc = (lanchor) LANCHOR;
    assert(lwrite_magics(b));
}

lfstack hot_slabs = LFSTACK;
type node_t = {sizeof(node), linref_up, linref_down};
heritage *node_hs;

volatile dptr nb;
static lflist *shared;
static list all = LIST(&all);
static pthread_mutex_t all_lock = PTHREAD_MUTEX_INITIALIZER;

void *reinsert_kid(uint t){
    lflist priv = LFLIST(&priv);
    list perm = LIST(&perm);
    tid_ = t + firstborn;
    heritage *node_h = &node_hs[t];

    rand_init();
    sem_wait(&parent_done);

    for(uint i = 0; i < niter; i++){
        if(randpcnt(10)){
            uptr r = condxadd(&nb, nalloc);
            if(r < nalloc){
                node *b = (node *) linalloc(node_h);
                log((void *) b, r, nb, nalloc);
                lflist_enq(flx_of(&b->flanc), &node_t, &priv);
                list_add_rear(&b->lanc, &perm);
            }
        }

        lflist *l = &shared[rand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_deq(&node_t, &priv))){
            log("Pushing", flptr(bx));
            assert(lmagics_valid(cof(flptr(bx), node, flanc)));
            lflist_enq(bx, &node_t, l);
        }else{
            bx = lflist_deq(&node_t, l);
            node *b = cof(flptr(bx), node, flanc);
            if(!b)
                continue;
            log("Popped", flptr(bx));
            assert(lwrite_magics(b));
            lflist_enq(bx, &node_t, &priv);
        }
    }

    lprintf("done!");

    for(flx bx; flptr(bx = lflist_deq(&node_t, &priv));)
        lflist_enq(bx, &node_t, &shared[0]);

    pthread_mutex_lock(&all_lock);
    for(node *b; (b = cof(list_pop(&perm), node, lanc));)
        list_add_rear(&b->lanc, &all);
    pthread_mutex_unlock(&all_lock);

    return NULL;
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
    list done = LIST(&done);
    for(uint i = 0; i < nlists; i++)
        for(node *b; (b = cof(flptr(lflist_deq(&node_t, &shared[i])),
                              node, flanc)); nb--)
        {
            list_remove(&b->lanc, &all);
            list_add_rear(&b->lanc, &done);
        }

    assert(!list_size(&all));
    assert(!nb);

    for(node *b; (b = cof(list_pop(&done), node, lanc));)
        linfree(&b->lin);
}

int malloc_test_main(int program);

int main(int argc, char **argv){
    int program = 1, opt, do_malloc = 0;
    while( (opt = getopt(argc, argv, "t:l:a:i:p:w:m")) != -1 ){
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
        case 'p':
            program = atoi(optarg);
            break;
        case 'w':
            nwrites = atoi(optarg);
            break;
        case 'm':
            do_malloc = 1;
            break;
        }
    }

    assert(mmap(NULL, SLAB_SIZE * 8, PROT_WRITE | PROT_WRITE,
                MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0)
           != MAP_FAILED);


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


    if(do_malloc)
        return malloc_test_main(program);
    
    TIME(test_reinsert());

    assert(!nb);
    return 0;
}


