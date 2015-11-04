#define MODULE LIST_TESTSM

#include <thread.h>
#include <pthread.h>
#include <semaphore.h>
#include <nalloc.h>
#include <lflist.h>
#include <list.h>
#include <getopt.h>
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
    cnt ephrefs;
    dbg_id owner;
    bool enq_attempted;
    bool invalidated;
    flanchor flanc;
    flanchor danc;
} node;
#define NODE {                                  \
        .lanc = LANCHOR(NULL),                  \
        .flanc = FLANCHOR(NULL),                \
        .danc = FLANCHOR(NULL),                 \
        .ephrefs = 1,                           \
       }                                        \

static cnt nlists = 1;
static cnt niter = 1000;
static cnt nallocs = 100;
static cnt nwrites = 8;

#define NPRIV 2

static void node_init(node *b);
static err perm_ref_up();
static void perm_ref_down();
static err eph_ref_up();
static void eph_ref_down();
static void ltthread_init(lflist priv[static NPRIV], list perm[static NPRIV],
                          heritage **h, type **t, dbg_id id);
static void ltthread_finish(lflist priv[static NPRIV], list perm[static NPRIV],
                            type *t, dbg_id id);

static type *perm_node_t = &(type)TYPE(node,
                                       (err (*)(volatile void *, type *)) perm_ref_up,
                                       (void (*)(volatile void *)) perm_ref_down,
                                       (void (*)(lineage *)) node_init);

/* static type *perm_node_t = &(type)TYPE(node, */
/*                                        (err (*)(volatile void *, type *)) linref_up, */
/*                                        (void (*)(volatile void *)) linref_down, */
/*                                        (void (*)(lineage *)) node_init); */

static heritage *thread_heritages;

static lflist *shared;
/* TODO: dead[nlists]? */
static lflist dead = LFLIST(&dead, NULL);
static lflist live = LFLIST(&live, NULL);;

static list all = LIST(&all, NULL);
static pthread_mutex_t all_lock = PTHREAD_MUTEX_INITIALIZER;

static void node_init(node *n){
    *n = (node) NODE;
}

static err perm_ref_up(){
    fake_linref_up();
    return 0;
}

static void perm_ref_down(){
    fake_linref_down();
}

#ifndef FAKELOCKFREE

static type *eph_node_t = &(type)TYPE(node,
                                      (err (*)(volatile void *, type *)) eph_ref_up,
                                      (void (*)(volatile void *)) eph_ref_down,
                                      (void (*)(lineage *)) node_init);

static err eph_ref_up(volatile void *f, type *_){
    assert(aligned_pow2(f, alignof(flanchor)));
    cnt *p = &cof(f, node, flanc)->ephrefs;
    for(cnt r = *p; r;)
        if(cas_won(r + 1, p, &r))
            return fake_linref_up(),
                   0;
    return -1;
}

static void eph_ref_down(volatile void *f){
    assert(aligned_pow2(f, alignof(flanchor)));
    node *n = cof(f, node, flanc);
    if(must(xadd(-1, &n->ephrefs)) == 1){
        n->flanc.n = n->flanc.p = (flx)
            {.markp = PUN(markp, (dptr) rand()),
             .mgen = PUN(mgen, (dptr) rand())};
        muste(lflist_enq(flx_of(&n->danc), perm_node_t, &dead));
    }
        
    fake_linref_down();
}

#endif

static void test_enq_deq(dbg_id id){
    lflist priv = LFLIST(&priv, NULL);
    list perm = LIST(&perm, NULL);
    heritage *node_h = &thread_heritages[id - firstborn];
    type *t = perm_node_t;

    for(cnt i = 0; i < nallocs; i++){
        node *b = (node *) mustp(linalloc(node_h));
        lflist_enq(flx_of(&b->flanc), t, &priv);
        list_enq(&b->lanc, &perm);
    }

    thr_sync(start_timing);

    for(uint i = 0; i < niter; i++){
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

    thr_sync(stop_timing);

    for(flx bx; flptr(bx = lflist_deq(t, &priv));)
        lflist_enq(bx, t, &shared[0]), t->linref_down(flptr(bx));

    for(node *b; (b = cof(list_deq(&perm), node, lanc));){
        pthread_mutex_lock(&all_lock);
        list_enq(&b->lanc, &all);
        pthread_mutex_unlock(&all_lock);
    }
}

#define ID_BITS 16
typedef struct{
    uptr last_priv:ID_BITS;
    /* TODO: 2 invalid bits per mgen. Advertise this somewhere. */
    uptr gen:WORDBITS - 2 - ID_BITS;
} lpgen;

static void test_enq_deq_del(dbg_id id){
    lflist priv[NPRIV];
    list perm[NPRIV];
    heritage *h;
    type *t;
    ltthread_init(priv, perm, &h, &t, id);

    for(uint i = 0; i < niter; i++){
        dbg bool del_failed = false;
        flx bx;
        node *b;
        if(randpcnt(50)){
            bool deq_priv = randpcnt(50);
            lflist *l = deq_priv
                      ? &priv[rand() % NPRIV]
                      : &shared[rand() % nlists];
            if(!(b = cof(flptr(bx = lflist_deq(t, l)), node, flanc)))
                goto grow;
            assert(PUN(lpgen, (uptr) bx.gen).last_priv == (deq_priv ? id : 0));
        }else{
        grow:
            if(!(b = cof(list_deq(&perm[rand() % NPRIV]), node, lanc)))
                continue;
            assert(b->owner == id);
            list_enq(&b->lanc, &perm[rand() % NPRIV]);

            muste(t->linref_up(b, t));
            bx = flx_of(&b->flanc);
            if(!lflist_del(bx, t))
                assert(flx_of(&b->flanc).gen == bx.gen);
            else
                del_failed = true;
        }
        
        b->enq_attempted = true;
        bool enq_priv = randpcnt(30);
        lpgen lp = PUN(lpgen, (uptr) bx.gen);
        lflist *nl = enq_priv ? &priv[rand() % NPRIV] : &shared[rand() % nlists];
        
        if(lflist_enq_upd(rup(lp,
                              .gen++,
                              .last_priv = (enq_priv ? id : 0)),
                          bx, t, nl))
            assert(b->owner != id || del_failed);
        
        t->linref_down(flptr(bx));
    }

    ltthread_finish(priv, perm, t, id);
}

static void test_enq_deq_jam(dbg_id id){
    lflist priv[NPRIV];
    list perm[NPRIV];
    heritage *h;
    type *t;
    ltthread_init(priv, perm, &h, &t, id);

    for(uint i = 0; i < niter; i++){
        dbg bool del_failed = false;
        flx bx;
        node *b;
        if(randpcnt(50)){
            bool deq_priv = randpcnt(50);
            lflist *l = deq_priv
                      ? &priv[rand() % NPRIV]
                      : &shared[rand() % nlists];
            if(!(b = cof(flptr(bx = lflist_deq(t, l)), node, flanc)))
                goto grow;
            assert(PUN(lpgen, (uptr) bx.gen).last_priv == (deq_priv ? id : 0));
        }else{
        grow:
            if(!(b = cof(list_deq(&perm[rand() % NPRIV]), node, lanc)))
                continue;
            assert(b->owner == id);
            list_enq(&b->lanc, &perm[rand() % NPRIV]);

            muste(t->linref_up(b, t));
            bx = flx_of(&b->flanc);
            if(randpcnt(30)){
                if(!lflist_del(bx, t))
                    assert(flx_of(&b->flanc).gen == bx.gen);
                else
                    del_failed = true;
            }else{
                while(lflist_jam(bx, t)){
                    flx obx = bx;
                    bx = flx_of(&b->flanc);
                    assert(bx.gen != obx.gen);
                }
                bx.gen++;
                assert(flx_of(&b->flanc).gen == bx.gen);
            }
        }
        
        b->enq_attempted = true;
        bool enq_priv = randpcnt(30);
        lpgen lp = PUN(lpgen, (uptr) bx.gen);
        lflist *nl = enq_priv ? &priv[rand() % NPRIV] : &shared[rand() % nlists];
        
        if(lflist_enq_upd(rup(lp,
                              .gen++,
                              .last_priv = (enq_priv ? id : 0)),
                          bx, t, nl))
            assert(b->owner != id || del_failed);
        
        t->linref_down(flptr(bx));
    }

    ltthread_finish(priv, perm, t, id);
}

#ifndef FAKELOCKFREE

static void test_validity_bits(dbg_id id){
    lflist priv[NPRIV];
    list perm[NPRIV];
    heritage *h;
    type *t;
    ltthread_init(priv, perm, &h, &t, id);

    return;

    for(uint i = 0; i < niter; i++){
        dbg bool del_failed = false;
        flx bx;
        node *b;
        if(randpcnt(50)){
            bool deq_priv = randpcnt(50);
            lflist *l = deq_priv
                      ? &priv[rand() % NPRIV]
                      : &shared[rand() % nlists];
            if(!(b = cof(flptr(bx = lflist_deq(t, l)), node, flanc)))
                goto grow;
            lpgen lp = PUN(lpgen, (uptr) bx.gen);
            assert(lp.last_priv == (deq_priv ? id : 0));
        }else{
        grow:
            while(!(b = cof(list_deq(&perm[rand() % NPRIV]), node, lanc)))
                continue;
            assert(b->owner == id);
            list_enq(&b->lanc, &perm[rand() % NPRIV]);

            muste(t->linref_up(b, t));
            bx = flx_of(&b->flanc);
            if(b->invalidated){
                assert(bx.validity != FLANC_VALID);
                flanc_ordered_init(bx.gen, &b->flanc);
                bx.validity = FLANC_VALID;
                b->invalidated = false;
            }
            
            if(randpcnt(50)){
                if(!lflist_del(bx, t))
                    assert(flx_of(&b->flanc).gen == bx.gen);
                else
                    del_failed = true;
            }else{
                while(lflist_jam(bx, t)){
                    flx obx = bx;
                    bx = flx_of(&b->flanc);
                    assert(bx.gen != obx.gen);
                }
                bx.gen++;
                assert(flx_of(&b->flanc).gen == bx.gen);
            }

            if(!del_failed && randpcnt(32)){
                must(mgen_upd_won(rup(bx.mgen, .validity=0), bx, t));
                b->invalidated = true;
                
                b->flanc.n.rsvd = 1;
                b->flanc.n.validity = 1;
                b->flanc.p.rsvd = 1;
                b->flanc.p.validity = 1;
                continue;
            }
        }
        
        b->enq_attempted = true;
        bool enq_priv = randpcnt(32);
        lpgen lp = PUN(lpgen, (uptr) bx.gen);
        lflist *nl = enq_priv ? &priv[rand() % NPRIV] : &shared[rand() % nlists];
        
        if(lflist_enq_upd(rup(lp,
                              .gen++,
                              .last_priv = (enq_priv ? id : 0)),
                          bx, t, nl))
            assert(b->owner != id || del_failed);
        
        t->linref_down(flptr(bx));
    }

    ltthread_finish(priv, perm, t, id);
}

static void test_linref_failure(dbg_id id){
    lflist priv[NPRIV];
    list perm[NPRIV];
    heritage *h;
    type *t;
    ltthread_init(priv, perm, &h, &t, id);
    t = eph_node_t;

    for(uint i = 0; i < niter; i++){
        dbg bool del_failed = false;
        flx bx;
        node *b;
        
        if(randpcnt(20)){
            b = cof(flptr(bx = lflist_deq(perm_node_t, &live)), node, danc);
            if(!b)
                continue;
            t->linref_down(&b->flanc);
            continue;
        }else if(randpcnt(20)){
            b = cof(flptr(bx = lflist_deq(perm_node_t, &dead)), node, danc);
            if(!b)
                continue;
            assert(!b->ephrefs);
            b->flanc = (flanchor) FLANCHOR(NULL);
            b->ephrefs = 1;
            muste(lflist_enq(bx, perm_node_t, &live));
            continue;
        }

        
        if(randpcnt(50)){
            bool deq_priv = randpcnt(50);
            lflist *l = deq_priv
                      ? &priv[rand() % NPRIV]
                      : &shared[rand() % nlists];
            if(!(b = cof(flptr(bx = lflist_deq(t, l)), node, flanc)))
                goto grow;
            assert(PUN(lpgen, (uptr) bx.gen).last_priv == (deq_priv ? id : 0));
        }else
        {
        grow:
            if(!(b = cof(list_deq(&perm[rand() % NPRIV]), node, lanc)))
                continue;
            assert(b->owner == id);
            list_enq(&b->lanc, &perm[rand() % NPRIV]);

            if(t->linref_up(&b->flanc, t))
                continue;
            
            bx = flx_of(&b->flanc);
            if(randpcnt(30)){
                if(!lflist_del(bx, t))
                    assert(flx_of(&b->flanc).gen == bx.gen);
                else
                    del_failed = true;
            }else{
        /*         /\* while(lflist_jam(bx, t)){ *\/ */
        /*         /\*     flx obx = bx; *\/ */
        /*         /\*     bx = flx_of(&b->flanc); *\/ */
        /*         /\*     assert(bx.gen != obx.gen); *\/ */
        /*         /\* } *\/ */
        /*         /\* bx.gen++; *\/ */
        /*         /\* assert(flx_of(&b->flanc).gen == bx.gen); *\/ */
            }
        }
        
        b->enq_attempted = true;
        bool enq_priv = randpcnt(30);
        lpgen lp = PUN(lpgen, (uptr) bx.gen);
        lflist *nl = enq_priv ? &priv[rand() % NPRIV] : &shared[rand() % nlists];
        
        if(lflist_enq_upd(rup(lp,
                              .gen++,
                              .last_priv = (enq_priv ? id : 0)),
                          bx, t, nl))
            assert(b->owner != id || del_failed);
        
        t->linref_down(flptr(bx));
    }

    ltthread_finish(priv, perm, perm_node_t, id);
}

#endif 

static void time_del(dbg_id id){
    list priv[NPRIV];
    for(cnt i = 0; i < NPRIV; i++)
        priv[i] = (list) LIST(&priv[i], NULL);
    
    heritage *h = &thread_heritages[id - firstborn];
    type *t = perm_node_t;

    for(cnt i = 0; i < niter; i++){
        node *b = (node *) mustp(linalloc(h));
        list_enq(&b->lanc, &priv[rand() % NPRIV]);
        muste(lflist_enq(flx_of(&b->flanc), t, &shared[rand() % nlists]));
    }

    thr_sync(start_timing);

    for(cnt i = 0; i < niter; i++){
        node *b = NULL;
        for(idx li, max = NPRIV + (li = rand()); must(li < max); li++)
            if((b = cof(list_deq(&priv[li % NPRIV]), node, lanc)))
                break;
        muste(lflist_del(flx_of(&b->flanc), t));
    }

    thr_sync(stop_timing);
}

static void launch_list_test(void t(dbg_id), bool gc, const char *name){
    heritage hs[nthreads];
    for(volatile heritage *h = hs; h != &hs[nthreads]; h++)
        *h = (heritage) POSIX_HERITAGE(perm_node_t);
    thread_heritages = hs;

    launch_test((void *(*)(void *)) t, name);
    
    if(!gc)
        return;

    list done = LIST(&done, NULL);
    cnt nb = nthreads * nallocs;
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

    lflist_report_profile();
}

static
void ltthread_init(lflist priv[static NPRIV], list perm[static NPRIV],
                   heritage **h, type **t, dbg_id id){
    assert(id && id < (1 << ID_BITS));
    
    for(idx i = 0; i < NPRIV; i++)
        priv[i] = (lflist) LFLIST(&priv[i], NULL);
    for(idx i = 0; i < NPRIV; i++)
        perm[i] = (list) LIST(&perm[i], NULL);

    *h = &thread_heritages[id - firstborn];
    *t = perm_node_t;

    for(cnt i = 0; i < nallocs; i++){
        node *b = (node *) mustp(linalloc(*h));
        b->owner = id;
        list_enq(&b->lanc, &perm[rand() % NPRIV]);
        lflist_enq(flx_of(&b->danc), perm_node_t, &live);
    }

    thr_sync(start_timing);
}

static
void ltthread_finish(lflist priv[static NPRIV], list perm[static NPRIV],
                     type *t, dbg_id id)
{
    thr_sync(stop_timing);

    for(idx i = 0; i < NPRIV; i ++)
        for(flx bx; flptr(bx = lflist_deq(t, &priv[i]));){
            muste(lflist_enq(bx, t, &shared[0]));
            t->linref_down(flptr(bx));
        }
        
    for(idx i = 0; i < NPRIV; i++){
        for(node *b; (b = cof(list_deq(&perm[i]), node, lanc));){
            if(b->invalidated)
                flanc_ordered_init(flx_of(&b->flanc).gen, &b->flanc);
            if(!b->enq_attempted || b->invalidated)
                lflist_enq(flx_of(&b->flanc), t, &shared[0]);
            pthread_mutex_lock(&all_lock);
            list_enq(&b->lanc, &all);
            pthread_mutex_unlock(&all_lock);
        }
    }
}

int main(int argc, char **argv){
    int program = 2;
    for(int opt; (opt = getopt(argc, argv, "t:l:a:i:p:w:")) != -1;){
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

    switch(program){
    case 1:
        launch_list_test(test_enq_deq, 1, "test_enq_deq");
        break;
    case 2:
        launch_list_test(test_enq_deq_del, 1, "test_enq_deq_del");
        break;
    case 3:
        launch_list_test(time_del, 0, "time_del");
        break;
    case 4:
        launch_list_test(test_enq_deq_jam, 1, "test_enq_deq_jam");
        break;

/* TODO: just a bit tired of updating fakelflist */
#ifndef FAKELOCKFREE        
    case 5:
        launch_list_test(test_validity_bits, 1, "test_validity_bits");
        break;
    case 6:
        launch_list_test(test_linref_failure, 1, "test_linref_failure");
        break;
#endif
    }


    return 0;
}


