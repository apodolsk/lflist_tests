#define MODULE LIST_TESTSM

#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <nalloc.h>
#include <lflist.h>
#include <list.h>
#include <getopt.h>
#include <wrand.h>
#include <atomics.h>
#include <global.h>
#include <asm.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <timing.h>
#include <signal.h>

#define MAXWRITE 8
typedef align(sizeof(dptr)) struct{
    lflist *p;
    uptr gen;
} pgen;
#define pgen(as...) ((pgen){as})

typedef struct{
    union{
        lineage lin;
        lanchor lanc;
    };
    flanchor flanc;
    dbg_id owner;
    pgen last_priv;
} node;

uint nlists = 1;
uint nthreads = 2;
uint niter = 1000;
uint nblocks = 100;
uint nwrites = 8;

/* GDB starts counting threads at 1, so the first child is 2. Urgh. */
const uptr firstborn = 2;

static sem_t parent_done;

static err succeed();
static void node_init(node *b);
extern void set_dbg_id(uint id);
static err perm_ref_up();
static void perm_ref_down();
static void thr_setup(uint id);
static void thr_destroy(uint id);

type *perm_node_t = &(type)TYPE(node,
                                (err (*)(volatile void *, type *)) perm_ref_up,
                                (void (*)(volatile void *)) perm_ref_down,
                                (void (*)(lineage *)) node_init);

volatile uptr nb;
static lflist *shared;
static list all = LIST(&all, NULL);
static pthread_mutex_t all_lock = PTHREAD_MUTEX_INITIALIZER;

static void node_init(node *n){
    *n = (node){.lanc = LANCHOR(NULL), .flanc = FLANCHOR(NULL)};
}

#include <libthread.h>
static err perm_ref_up(){
    T->nallocin.linrefs_held++;
    return 0;
}

static void perm_ref_down(){
    T->nallocin.linrefs_held--;
}

static void *test_reinsert(uint id){
    thr_setup(id);
    lflist priv = LFLIST(&priv, NULL);
    list perm = LIST(&perm, NULL);
    heritage *node_h =
        &(heritage)POSIX_HERITAGE(perm_node_t);
    type *t = perm_node_t;

    sem_wait(&parent_done);

    for(uint i = 0; i < niter; i++){
        ppl(1, i);
        if(randpcnt(10) && condxadd(1, &nb, nblocks) < nblocks){
            node *b = (node *) linalloc(node_h);
            lflist_enq(flx_of(&b->flanc), t, &priv);
            list_enq(&b->lanc, &perm);
        }
        
        lflist *l = &shared[wrand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_deq(t, &priv))){
            t->linref_down(flptr(bx));
            muste(lflist_enq(bx, t, l));
        }else if(flptr(bx = lflist_deq(t, l))){
            node *b = cof(flptr(bx), node, flanc);
            t->linref_down(b);
            muste(lflist_enq(bx, t, &priv));
        }
    }

    lprintf("done!");

    for(flx bx; flptr(bx = lflist_deq(t, &priv));)
        lflist_enq(bx, t, &shared[0]), t->linref_down(flptr(bx));

    for(node *b; (b = cof(list_deq(&perm), node, lanc));){
        pthread_mutex_lock(&all_lock);
        list_enq(&b->lanc, &all);
        pthread_mutex_unlock(&all_lock);
    }

    thr_destroy(id);

    return NULL;
}

static void *test_del(dbg_id id){
    thr_setup(id);
    sem_wait(&parent_done);

    lflist priv = LFLIST(&priv, NULL);
    list perm = LIST(&perm, NULL);
    heritage *node_h = &(heritage)POSIX_HERITAGE(perm_node_t);
    type *t = perm_node_t;

    pp(0, getpgid(syscall(SYS_gettid)));

    for(uint i = 0; i < niter; i++){
        ppl(1, i);
        if(randpcnt(10) && condxadd(1, &nb, nblocks) < nblocks){
            node *b = (node *) linalloc(node_h);
            pp(b);
            b->last_priv = (pgen){&priv, flx_of(&b->flanc).gen + 1};
            lflist_enq(flx_of(&b->flanc), t, &priv);
            b->owner = id;
            list_enq(&b->lanc, &perm);
        }

        lflist *l = &shared[wrand() % nlists];
        bool del_failed = false;
        lflist *last_priv = &priv;
        flx bx;
        node *b;
        if(randpcnt(33) && (b = cof(flptr(bx = lflist_deq(t, &priv)),
                                    node, flanc)))
            /* assert((b->last_priv.p == &priv && b->last_priv.gen == bx.gen) || */
            /*        (b->owner != id && bx.gen != flx_of(&b->flanc).gen)); */
            ;
        else if(randpcnt(50) && (b = cof(flptr(bx = lflist_deq(t, l)),
                                         node, flanc)))
            last_priv = b->last_priv.p;
        else{
            b = cof(list_deq(&perm), node, lanc);
            if(!b)
                continue;
            muste(t->linref_up(b, t));
            /* assert(b->owner == id); */
            list_enq(&b->lanc, &perm);
            bx = flx_of(&b->flanc);
            if(lflist_del(bx, t))
                del_failed = true;
            last_priv = b->last_priv.p;
        }
        lflist *nl = randpcnt(30) ? &priv : l;
        if(!lflist_enq(bx, t, nl)){
            /* if(nl == &priv) */
            /*     assert(cas2_won(pgen(&priv, bx.gen + 1), &b->last_priv, */
            /*                     &pgen(last_priv, b->last_priv.gen)) */
            /*            || (b->owner != id && flx_of(&b->flanc).gen != bx.gen)); */
            ;
        }else
            /* TODO: this'll test lflist_del(x,..) returns =>
               lflist_enq(x,..) == 0, which isn't true yet/ever. */
            /* assert((del_failed || b->owner != id) && */
            /*        flx_of(&b->flanc).gen != bx.gen); */
            /* assert(del_failed || (b->owner != id && */
            /*                       flx_of(&b->flanc).gen != bx.gen)); */
            (void) last_priv, (void) del_failed;

        t->linref_down(flptr(bx));
    }

    lprintf("done!");

    for(flx bx; flptr(bx = lflist_deq(t, &priv));)
        lflist_enq(bx, t, &shared[0]), t->linref_down(flptr(bx));
        

    for(node *b; (b = cof(list_deq(&perm), node, lanc));){
        pthread_mutex_lock(&all_lock);
        list_enq(&b->lanc, &all);
        pthread_mutex_unlock(&all_lock);
    }

    thr_destroy(id);

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

type *node_t = &(type)TYPE(node, linref_up, linref_down,
                           (void (*)(lineage *))node_init);

static void *test_lin_reinsert(uint id){
    lflist priv = LFLIST(&priv, NULL);
    list perm = LIST(&perm, NULL);
    heritage *node_h = &(heritage)HERITAGE(node_t, 1, 1, new_slabs);
    TODO("");
    type *t = NULL;
    
    list privnn = LIST(&privnn, NULL);

    thr_setup(id);

    sem_wait(&parent_done);

    for(uint i = 0; i < niter; i++){
        if(randpcnt(20) && (condxadd(1, &nb, nblocks) < nblocks)){
            notnode *nn = malloc(sizeof(notnode));
            write_magics(nn);
            list_enq(&nn->lanc, &privnn);
        }

        notnode *nn;
        if(randpcnt(10) && (nn = cof(list_deq(&privnn), notnode, lanc))){
            assert(magics_valid(nn));
            free(nn);
            xadd(-1, &nb);
        }
        
        if(randpcnt(10) && condxadd(1, &nb, nblocks) < nblocks){
            node *b = (node *) linalloc(node_h);
            must(lflist_enq(flx_of(&b->flanc), perm_node_t, &priv));
            list_enq(&b->lanc, &perm);
        }

        lflist *l = &shared[wrand() % nlists];
        flx bx;
        if(randpcnt(50) && flptr(bx = lflist_deq(perm_node_t, &priv))){
            muste(lflist_enq(bx, node_t, l));
        }else{
            bx = lflist_deq(perm_node_t, l);
            node *b = cof(flptr(bx), node, flanc);
            if(!b)
                continue;
            linref_down(b);
            lflist_enq(bx, node_t, &priv);
        }
    }

    lprintf("done!");

    for(flx bx; flptr(bx = lflist_deq(perm_node_t, &priv));)
        lflist_enq(bx, perm_node_t, &shared[0]), perm_ref_down(flptr(bx), t);

    for(notnode *nn; (nn = cof(list_deq(&privnn), notnode, lanc));)
        xadd(-1, &nb);

    pthread_mutex_lock(&all_lock);
    for(node *b; (b = cof(list_deq(&perm), node, lanc));)
        list_enq(&b->lanc, &all);
    pthread_mutex_unlock(&all_lock);

    thr_destroy(id);

    return NULL;
}

static struct tctxt{
    pthread_t id;
    bool dead;
} *threads;
static sem_t unpauses;
static sem_t pauses;
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;

static void thr_setup(uint id){
    set_dbg_id(id);
    wsrand(GETTIME());
    muste(pthread_mutex_lock(&state_lock));
    threads[id - firstborn] = (struct tctxt) {pthread_self()};
    muste(pthread_mutex_unlock(&state_lock));
}

static void thr_destroy(uint id){
    muste(pthread_mutex_lock(&state_lock));
    threads[id - firstborn].dead = true;
    muste(pthread_mutex_unlock(&state_lock));    
}

iptr waiters;
err pause_universe(void){
    assert(waiters >= 0);
    if(!cas_won(1, &waiters, (iptr[]){0}))
        return -1;
    muste(pthread_mutex_lock(&state_lock));
    cnt live = 0;
    for(struct tctxt *c = &threads[0]; c != &threads[nthreads]; c++)
        if(!c->dead && c->id != pthread_self()){
            live++;
            pthread_kill(c->id, SIGUSR1);
        }
    waiters += live;
    muste(pthread_mutex_unlock(&state_lock));
    for(uint i = 0; i < live; i++)
        muste(sem_wait(&pauses));
    return 0;
}

void resume_universe(void){
    cnt live = 0;
    for(struct tctxt *c = &threads[0]; c != &threads[nthreads]; c++)
        if(!c->dead && c->id != pthread_self())
            live++;
    assert(live == (cnt) waiters - 1);
    for(cnt i = 0; i < live; i++)
        muste(sem_post(&unpauses));
    _xadd(-1, (uptr *) &waiters);
}

void wait_for_universe(){
    muste(sem_post(&pauses));
    muste(sem_wait(&unpauses));
    _xadd(-1, (uptr *) &waiters);
}

static void launch_test(void *test(void *)){
    muste(sem_init(&pauses, 0, 0));
    muste(sem_init(&unpauses, 0, 0));
    muste(sigaction(SIGUSR1,
                    &(struct sigaction){.sa_handler=wait_for_universe,
                            .sa_flags=SA_RESTART | SA_NODEFER}, NULL));

    struct tctxt threadscope[nthreads];
    threads = threadscope;
    for(uint i = 0; i < nthreads; i++)
        if(pthread_create(&threads[i].id, NULL, (void *(*)(void*))test,
                          (void *) (firstborn + i)))
            EWTF();
    
    for(uint i = 0; i < nthreads; i++)
        sem_post(&parent_done);
    
    for(uint i = 0; i < nthreads; i++)
        pthread_join(threads[i].id, NULL);
    list done = LIST(&done, NULL);
    /* TODO: should use node_t for those tests which require it */
    for(uint i = 0; i < nlists; i++)
        for(node *b; (b = cof(flptr(lflist_deq(perm_node_t, &shared[i])),
                              node, flanc)); nb--)
        {
            list_remove(&b->lanc, &all);
            list_enq(&b->lanc, &done);
        }

    cnt all_size = list_size(&all);
    for(node *lost; (lost = cof(list_deq(&all), node, lanc));)
        pp(1, &lost->flanc);
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
            nblocks = atoi(optarg);
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

    lflist lists[nlists];
    for(uint i = 0; i < nlists; i++)
        lists[i] = (lflist) LFLIST(&lists[i], NULL);
    shared = lists;
    pp(1, shared);

    if(do_malloc)
        return malloc_test_main(program);

    switch(program){
    case 1:
        TIME(launch_test((void *(*)(void *))test_reinsert));
        break;
    case 2:
        TIME(launch_test((void *(*)(void *))test_del));
        break;
    case 3:
        TIME(launch_test((void *(*)(void *))test_lin_reinsert));
        break;
    }

    assert(!nb);
    return 0;
}


