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
#include <asm.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <timing.h>
#include <signal.h>
#include <test_framework.h>

#define MAXWRITE 8
typedef align(sizeof(dptr)) struct{
    dbg_id owner;
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
    volatile pgen last_priv;
} node;

cnt nlists = 1;
cnt niter = 1000;
cnt nallocs = 100;
cnt nwrites = 8;

static err succeed();
static void node_init(node *b);
static err perm_ref_up();
static void perm_ref_down();

type *perm_node_t = &(type)TYPE(node,
                                (err (*)(volatile void *, type *)) perm_ref_up,
                                (void (*)(volatile void *)) perm_ref_down,
                                (void (*)(lineage *)) node_init);

static lflist *shared;
static list all = LIST(&all, NULL);
static pthread_mutex_t all_lock = PTHREAD_MUTEX_INITIALIZER;

static void node_init(node *n){
    *n = (node){.lanc = LANCHOR(NULL), .flanc = FLANCHOR(NULL)};
}

#include <thread.h>
static err perm_ref_up(){
    T->nallocin.linrefs_held++;
    return 0;
}

static void perm_ref_down(){
    T->nallocin.linrefs_held--;
}

static void test_enq_deq(dbg_id id){
    lflist priv = LFLIST(&priv, NULL);
    list perm = LIST(&perm, NULL);
    heritage *node_h =
        &(heritage)POSIX_HERITAGE(perm_node_t);
    type *t = perm_node_t;

    for(cnt i = 0; i < nallocs; i++){
        node *b = (node *) mustp(linalloc(node_h));
        lflist_enq(flx_of(&b->flanc), t, &priv);
        list_enq(&b->lanc, &perm);
    }

    thr_sync();

    for(uint i = 0; i < niter; i++){
        ppl(2, i);
        lflist *l = &shared[rand() % nlists];
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

    log(2, "done!");

    for(flx bx; flptr(bx = lflist_deq(t, &priv));)
        lflist_enq(bx, t, &shared[0]), t->linref_down(flptr(bx));

    for(node *b; (b = cof(list_deq(&perm), node, lanc));){
        pthread_mutex_lock(&all_lock);
        list_enq(&b->lanc, &all);
        pthread_mutex_unlock(&all_lock);
    }
}

static void test_del(dbg_id id){
    lflist priv = LFLIST(&priv, NULL);
    list perm = LIST(&perm, NULL);
    heritage *node_h = &(heritage)POSIX_HERITAGE(perm_node_t);
    type *t = perm_node_t;

    for(cnt i = 0; i < nallocs; i++){
        node *b = (node *) mustp(linalloc(node_h));
        pp(&b->flanc);
        muste(lflist_enq(flx_of(&b->flanc), t, &priv));
        b->last_priv = (pgen){id, flx_of(&b->flanc).gen};
        b->owner = id;
        list_enq(&b->lanc, &perm);
    }

    thr_sync();

    pp(priv);
    for(uint i = 0; i < niter; i++){
        ppl(3, i);

        lflist *l = &shared[rand() % nlists];
        dbg bool del_failed = false;
        flx bx;
        node *b;
        if(randpcnt(33) && (b = cof(flptr(bx = lflist_deq(t, &priv)),
                                    node, flanc)))
            assert((b->last_priv.owner == id && b->last_priv.gen == bx.gen) ||
                   (b->owner != id && bx.gen != flx_of(&b->flanc).gen));
        else if(randpcnt(50) && (b = cof(flptr(bx = lflist_deq(t, l)),
                                         node, flanc))){
            assert(b->last_priv.gen != bx.gen);
        }else{
            b = cof(list_deq(&perm), node, lanc);
            if(!b)
                continue;
            muste(t->linref_up(b, t));
            assert(b->owner == id);
            list_enq(&b->lanc, &perm);
            bx = flx_of(&b->flanc);
            if(!lflist_del(bx, t))
                assert(flx_of(&b->flanc).gen == bx.gen);
            else
                del_failed = true;
        }
        pgen pg = b->last_priv;
        lflist *nl = randpcnt(30) ? &priv : l;
        pp(&b->flanc, pg.owner, pg.gen, bx);
        if(!lflist_enq(bx, t, nl)){
            if(nl == &priv){
                for(; bx.gen - (pg = b->last_priv).gen < (UPTR_MAX >> 1);)
                    if(cas2_won(pgen(id, bx.gen + 1), &b->last_priv, &pg))
                        break;
                pp(pg.gen, pg.owner);
                if(bx.gen - pg.gen >= (UPTR_MAX >> 1)){
                    assert(pg.gen != bx.gen + 1);
                    assert(b->owner != id && flx_of(&b->flanc).gen != bx.gen);
                }
            }
        }else
            assert(b->owner != id || del_failed);

        t->linref_down(flptr(bx));
    }

    log(1, "done!");

    for(flx bx; flptr(bx = lflist_deq(t, &priv));)
        lflist_enq(bx, t, &shared[0]), t->linref_down(flptr(bx));
        

    for(node *b; (b = cof(list_deq(&perm), node, lanc));){
        pthread_mutex_lock(&all_lock);
        list_enq(&b->lanc, &all);
        pthread_mutex_unlock(&all_lock);
    }
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

static void launch_list_test(void t(dbg_id), const char *name){
    launch_test((void *(*)(void *)) t, name);
    
    list done = LIST(&done, NULL);
    cnt nb = nthreads * nallocs;
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
        ppl(0, &lost->flanc);
    assert(!all_size);
    assert(!nb);

    for(node *b; (b = cof(list_deq(&done), node, lanc));)
        linfree(&b->lin);

    ppl(0, lflist_ops, naborts, paborts, pn_oks,
        helpful_enqs, cas_ops, atomic_read_ops);
}

int main(int argc, char **argv){
    int malloc_test_main(int program);
    int program = 1, opt, do_malloc = 0;
    while( (opt = getopt(argc, argv, "t:l:a:i:p:w")) != -1 ){
        switch (opt){
        case 't':
            nthreads = atoi(optarg);
            break;
        case 'l':
            nlists = atoi(optarg);
            break;
        case 'a':
            nallocs = atoi(optarg);
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
        }
    }

    lflist lists[nlists];
    for(uint i = 0; i < nlists; i++)
        lists[i] = (lflist) LFLIST(&lists[i], NULL);
    shared = lists;
    pp(1, shared);

    switch(program){
    case 1:
        launch_list_test(test_enq_deq, "test_enq_deq");
        break;
    case 2:
        launch_list_test(test_del, "test_del");
        break;
    /* case 3: */
    /*     TIME(launch_test((void *(*)(void *))test_lin_reinsert)); */
    /*     break; */
    }

    return 0;
}


