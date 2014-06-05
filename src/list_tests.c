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
#include <timing.h>

#undef TS
#define TS (LFLIST_TS)

uint nlists = 1;
uint nthreads = 2;
uint niter = 1000;
uint nalloc = 100;
uint nwrites = 8;

/* GDB starts counting threads at 1, so the first child is 2. Urgh. */
const uint firstborn = 2;

#define MAXWRITE 8

static sem_t parent_done;

static err succeed();

typedef union{
    lineage lin;
    struct{
        lanchor lanc;
        flanchor flanc;
    };
} node;
type *node_t = &(type){sizeof(node), linref_up, linref_down};
type *perm_node_t = &(type){sizeof(node),
                    (err (*)(volatile void *, const type *)) succeed,
                    (void (*)(volatile void *)) succeed};

volatile uptr nb;
static lflist *shared;
static list all = LIST(&all);
static pthread_mutex_t all_lock = PTHREAD_MUTEX_INITIALIZER;

static uptr condxadd(volatile uptr *d, uptr max){
    uptr r, f = *d;
    do{
        r = f;
        if(r >= max)
            return r;
        f = cas(r + 1, d, r);
    }while(f != r);
    return r;
}

static void node_init(node *b){
    b->flanc = (flanchor) FLANCHOR;
    b->lanc = (lanchor) LANCHOR;
}

static err succeed(){
    return 0;
}

static void *test_reinsert(uint t){
    lflist priv = LFLIST(&priv);
    list perm = LIST(&perm);
    tid_ = t + firstborn;
    heritage *node_h =
        &(heritage)POSIX_HERITAGE(node_t, 16, 16, (void (*)(void *)) node_init);

    rand_init();
    sem_wait(&parent_done);

    for(uint i = 0; i < niter; i++){
        if(randpcnt(10) && condxadd(&nb, nalloc) < nalloc){
            node *b = (node *) linalloc(node_h);
            pp((void *) b);
            lflist_enq(flx_of(&b->flanc), perm_node_t, &priv);
            list_enq(&b->lanc, &perm);
        }

        lflist *l = &shared[rand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_deq(perm_node_t, &priv))){
            log("Pushing %", flptr(bx));
            lflist_enq(bx, perm_node_t, l);
        }else{
            bx = lflist_deq(perm_node_t, l);
            node *b = cof(flptr(bx), node, flanc);
            if(!b)
                continue;
            log("Popped %", flptr(bx));
            lflist_enq(bx, perm_node_t, &priv);
        }
    }

    lprintf("done!");

    for(flx bx; flptr(bx = lflist_deq(perm_node_t, &priv));)
        lflist_enq(bx, perm_node_t, &shared[0]);

    pthread_mutex_lock(&all_lock);
    for(node *b; (b = cof(list_deq(&perm), node, lanc));)
        list_enq(&b->lanc, &all);
    pthread_mutex_unlock(&all_lock);

    return NULL;
}

static void *test_del(uint t){
    lflist semipriv = LFLIST(&semipriv);
    list perm = LIST(&perm);
    tid_ = t + firstborn;
    heritage *node_h =
        &(heritage)POSIX_HERITAGE(node_t, 16, 16, (void (*)(void *)) node_init);

    rand_init();
    sem_wait(&parent_done);

    for(uint i = 0; i < niter; i++){
        if(randpcnt(10) && condxadd(&nb, nalloc) < nalloc){
            node *b = (node *) linalloc(node_h);
            pp((void *) b);
            lflist_enq(flx_of(&b->flanc), perm_node_t, &semipriv);
            list_enq(&b->lanc, &perm);
        }

        lflist *l = &shared[rand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_deq(perm_node_t, &semipriv))){
            log("Pushing", flptr(bx));
            lflist_enq(bx, perm_node_t, l);
        }else{
            node *b = cof(list_deq(&perm), node, lanc);
            if(!b)
                continue;
            list_enq(&b->lanc, &perm);
            bx = flx_of(&b->flanc);
            if(lflist_del(bx, perm_node_t))
                continue;
            log("Popped", (void *) flptr(bx));
            lflist_enq(bx, perm_node_t, &semipriv);
        }
    }

    lprintf("done!");

    for(flx bx; flptr(bx = lflist_deq(perm_node_t, &semipriv));)
        lflist_enq(bx, perm_node_t, &shared[0]);

    pthread_mutex_lock(&all_lock);
    for(node *b; (b = cof(list_deq(&perm), node, lanc));)
        list_enq(&b->lanc, &all);
    pthread_mutex_unlock(&all_lock);

    return NULL;
}

typedef union{
    lineage lin;
    struct{
        lanchor lanc;
        pthread_t magics[MAXWRITE];
    };
} notnode;

static int write_magics(notnode *b){
    assert(nwrites <= MAXWRITE);
    for(uint i = 0; i < nwrites; i++)
        b->magics[i] = pthread_self();
    return 1;
}

static int magics_valid(notnode *b){
    for(uint i = 0; i < nwrites; i++)
        assert(b->magics[i] == pthread_self());
    return 1;
}


static void *test_lin_reinsert(uint t){
    lflist priv = LFLIST(&priv);
    list perm = LIST(&perm);
    tid_ = t + firstborn;
    heritage *node_h =
        &(heritage)POSIX_HERITAGE(node_t, 1, 1,
                                  (void (*)(void *)) node_init);
    
    list privnn = LIST(&privnn);

    rand_init();
    sem_wait(&parent_done);

    for(uint i = 0; i < niter; i++){
        if(randpcnt(20) && (condxadd(&nb, nalloc) < nalloc)){
            notnode *b = (notnode *) malloc(sizeof(notnode));
            write_magics(b);
            list_enq(&b->lanc, &privnn);
        }

        notnode *nn;
        if(randpcnt(10) && (nn = cof(list_deq(&privnn), notnode, lanc))){
            assert(magics_valid(nn));
            free(nn);
            xadd(-1, &nb);
        }
        
        if(randpcnt(10) && condxadd(&nb, nalloc) < nalloc){
            node *b = (node *) linalloc(node_h);
            pp((void *) b);
            lflist_enq(flx_of(&b->flanc), perm_node_t, &priv);
            list_enq(&b->lanc, &perm);
        }

        lflist *l = &shared[rand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_deq(perm_node_t, &priv))){
            lflist_enq(bx, perm_node_t, l);
        }else{
            bx = lflist_deq(perm_node_t, l);
            node *b = cof(flptr(bx), node, flanc);
            if(!b)
                continue;
            lflist_enq(bx, perm_node_t, &priv);
        }
    }

    lprintf("done!");

    for(flx bx; flptr(bx = lflist_deq(perm_node_t, &priv));)
        lflist_enq(bx, perm_node_t, &shared[0]);

    for(notnode *nn; (nn = cof(list_deq(&privnn), notnode, lanc));)
        xadd(-1, &nb);

    pthread_mutex_lock(&all_lock);
    for(node *b; (b = cof(list_deq(&perm), node, lanc));)
        list_enq(&b->lanc, &all);
    pthread_mutex_unlock(&all_lock);

    return NULL;
}


static void launch_test(void *test(void *)){
    sem_t parent_dead;
    if(sem_init(&parent_dead, 0, 0))
        EWTF();

    pthread_t tids[nthreads];
    for(uint i = 0; i < nthreads; i++)
        if(pthread_create(&tids[i], NULL, (void *(*)(void*))test, (void *)i))
            EWTF();
    
    for(uint i = 0; i < nthreads; i++)
        sem_post(&parent_done);
    for(uint i = 0; i < nthreads; i++)
        pthread_join(tids[i], NULL);
    list done = LIST(&done);
    for(uint i = 0; i < nlists; i++)
        for(node *b; (b = cof(flptr(lflist_deq(node_t, &shared[i])),
                              node, flanc)); nb--)
        {
            list_remove(&b->lanc, &all);
            list_enq(&b->lanc, &done);
        }

    cnt all_size = list_size(&all);
    for(node *lost; (lost = cof(list_deq(&all), node, lanc));)
        pp(&lost->flanc);
    assert(!all_size);
    assert(!nb);

    for(node *b; (b = cof(list_deq(&done), node, lanc));)
        linfree(&b->lin);
}

int main(int argc, char **argv){
    int malloc_test_main(int program);
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

    lflist lists[nlists];
    for(uint i = 0; i < nlists; i++)
        lists[i] = (lflist) LFLIST(&lists[i]);
    shared = lists;
    pp(shared);

    if(do_malloc)
        return malloc_test_main(program);

    switch(program){
    case 1:
        TIME(launch_test((void *(*)(void *))test_reinsert));
        break;
    case 2:
        TIME(launch_test((void *(*)(void *))test_lin_reinsert));
        break;
    case 3:
        TIME(launch_test((void *(*)(void *))test_del));
        break;
    }

    assert(!nb);
    return 0;
}


