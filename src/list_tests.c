#define MODULE LIST_TESTSM

#include <thread.h>
#include <nalloc.h>
#include <lflist.h>
#include <list.h>
#include <getopt.h>
#include <test_framework.h>

/* TODO: nalloc can't handle huge allocations, unfortunately. */
#define MAX_ALLOCS 1000000
#define MAX_WRITE 8
#define ID_BITS 16

typedef struct node{
    union{
        lineage lin;
        lanchor lanc;
    };
    cnt ephrefs;
    uptr owner;
    bool enq_attempted;
    bool invalidated;
    flanchor flanc;
    flanchor danc;
} node;
#define NODE() {                                \
        .lanc = LANCHOR(NULL),                  \
        .flanc = FLANCHOR(NULL),                \
        .danc = FLANCHOR(NULL),                 \
        .ephrefs = 1,                           \
       }                                        \

static cnt nlists = 1;
static cnt niter = 1000;
static cnt nallocs = 100;
static cnt nwrites = 8;

static void node_init(node *b);
static bool has_perm_ref(void *l, bool up);
static bool has_eph_ref(void *l, bool up);
static void ltthread_init(lflist *fill, uptr id);
static void ltthread_finish(lflist *clear);


static struct type t = TYPE(node,
                            (void *) node_init,
                            (void *) has_perm_ref);


static lflist *shared;

static node *all[MAX_ALLOCS];
static lflist dead = LFLIST(&dead, NULL);
static lflist live = LFLIST(&live, NULL);;

static __thread heritage h = POSIX_HERITAGE(&t);
static __thread node **owned;

static void node_init(node *n){
    *n = (node) NODE();
}

static bool has_perm_ref(void *_, bool up){
    return true;
}

#ifndef FAKELOCKFREE

type *perm_node_t = &(type)TYPE(node,
                                (void *) node_init,
                                (void *) has_perm_ref);

/* TODO: rethink this. Never return true?  */
static bool has_eph_ref(void *f, bool up){
    assert(aligned_pow2(f, alignof(flanchor)));    
    if(up){
        cnt *p = &cof(f, node, flanc)->ephrefs;
        for(cnt r = *p; r;)
            if(cas_won(r + 1, p, &r))
                return true;
        return false;
    }else{
        node *n = cof(f, node, flanc);
        if(must(xadd(-1, &n->ephrefs)) == 1){
            assert(n->flanc.p.st == COMMIT);
            while(!flanc_valid(&n->flanc))
                continue;
            /* TODO: something random, but less random. */
            /* n->flanc.n = n->flanc.p = (flx) */
            /*     {.markp = PUN(markp, (uptr) rand()), */
            /*      .gen = (uptr) rand()}; */
            muste(lflist_enq(flref_of(&n->danc), &dead, perm_node_t));
        }
        
        return true;
    }
}
#else
static bool has_eph_ref(void *f, bool up){
    return true;
}

#endif

static node *next_owned_node(void){
    static __thread uptr next_idx = 0;
    return owned[next_idx++ % nallocs];
}

static node *rand_owned_node(void){
    return owned[rand() % nallocs];
}

static node *rand_node(void){
    return all[rand() % (nallocs * nthreads)];
}

static lflist *rand_shared_list(void){
    return &shared[rand() % nlists];
}

static void launch_list_test(void test(uptr), bool gc, const char *name){
    lflist lists[nlists];
    for(cnt i = 0; i < nlists; i++)
        lists[i] = (lflist) LFLIST(&lists[i], NULL);
    shared = lists;

    assertl(0, nallocs * nthreads <= MAX_ALLOCS);

    launch_test((void *(*)(void *)) test, name);
    
    if(!gc)
        return;

    list done = LIST(&done, NULL);
    for(cnt i = 0; i < nlists; i++)
        for(node *b;
            (b = cof(flptr(lflist_unenq(&shared[i], &t)), node, flanc));)
        {
            lflist_del(flref_of(&b->danc), &t);
            linfree(&b->lin);
        }

    cnt nlost = 0;
    for(node *lost; (lost = cof(flptr(lflist_unenq(&live, &t)), node, flanc));){
        if(lost->invalidated || !lost->enq_attempted)
            continue;
        ppl(0, &lost->flanc);
        nlost++;

        linref_down(lost, &t);
        linfree(&lost->lin);
    }
    assert(!nlost);
}

static
void ltthread_init(lflist *fill, uptr id){
    assert(id && id < (1 << ID_BITS));

    owned = &all[(id - firstborn) * nallocs];
    for(cnt i = 0; i < nallocs; i++){
        node *b = (node *) mustp(linalloc(&h));
        b->owner = id;
        owned[i] = b;

        if(fill)
            lflist_enq(flref_of(&b->flanc), fill, &t);
        lflist_enq(flref_of(&b->danc), &live, &t);
    }

    thr_sync(start_timing);
}

static
void ltthread_finish(lflist *clear)
{
    thr_sync(stop_timing);

    if(!clear)
        return;

    for(flref bx; flptr(bx = lflist_unenq(clear, &t));){
        muste(lflist_enq(bx, &shared[0], &t));
        linref_down(flptr(bx), &t);
    }
}

static void test_enq_unenq(uptr id){
    ltthread_init(&shared[0], id);

    for(uint i = 0; i < niter; i++){
        flref bx;
        while(!flptr(bx = lflist_unenq(rand_shared_list(), &t)))
            continue;
        muste(lflist_enq(bx, rand_shared_list(), &t));
        linref_down(flptr(bx), &t);
    }

    ltthread_finish(NULL);
}

static void test_enq_deq(uptr id){
    ltthread_init(&shared[0], id);

    for(uint i = 0; i < niter; i++){
        flref bx;
        while(!flptr(bx = lflist_deq(rand_shared_list(), &t)))
            continue;
        muste(lflist_enq(bx, rand_shared_list(), &t));
        linref_down(flptr(bx), &t);
    }

    ltthread_finish(NULL);
}

typedef struct{
    uptr last_priv:ID_BITS;
    /* TODO: 2 invalid bits per mgen. Advertise this somewhere. */
    uptr gen:WORDBITS - 2 - ID_BITS;
} lpgen;

static void test_enq_unenq_del(uptr id){
    lflist priv = LFLIST(&priv, NULL);
    ltthread_init(NULL, id);

    for(uint i = 0; i < niter; i++){
        /* TODO: can cut this, now that del blocks enqs */
        dbg bool del_failed = false;
        flref bx;
        node *b;
        if(randpcnt(50)){
            bool unenq_priv = randpcnt(50);
            lflist *l = unenq_priv
                      ? &priv
                      : rand_shared_list();
            if(!(b = cof(flptr(bx = lflist_unenq(l, &t)), node, flanc)))
                goto grow;
            assert(PUN(lpgen, (uptr) bx.gen).last_priv == (unenq_priv ? id : 0));
        }else{
        grow:
            if(!(b = rand_owned_node()))
                continue;

            muste(linref_up(b, &t));
            bx = flref_of(&b->flanc);
            if(!lflist_del(bx, &t))
                assert(flref_of(&b->flanc).gen == bx.gen);
            else
                del_failed = true;
        }
        
        b->enq_attempted = true;
        lpgen lp = PUN(lpgen, (uptr) bx.gen);
        lflist *nl = randpcnt(30) ? &priv : rand_shared_list();
        
        if(lflist_enq_cas(rup(lp,
                              .gen++,
                              .last_priv = (nl == &priv ? id : 0)),
                          bx, nl, &t))
            assert(b->owner != id || del_failed);
        
        linref_down(flptr(bx), &t);
    }

    ltthread_finish(&priv);
}

static void test_enq_unenq_jam(uptr id){
    lflist priv = LFLIST(&priv, NULL);
    ltthread_init(NULL, id);

    for(uint i = 0; i < niter; i++){
        dbg bool del_failed = false;
        flref bx;
        node *b;
        if(randpcnt(50)){
            bool unenq_priv = randpcnt(50);
            lflist *l = unenq_priv
                      ? &priv
                      : rand_shared_list();
            if(!(b = cof(flptr(bx = lflist_unenq(l, &t)), node, flanc)))
                goto grow;
            assert(PUN(lpgen, (uptr) bx.gen).last_priv == (unenq_priv ? id : 0));
        }else{
        grow:
            if(!(b = rand_owned_node()))
                continue;
            assert(b->owner == id);

            muste(linref_up(b, &t));
            bx = flref_of(&b->flanc);
            if(randpcnt(30)){
                if(!lflist_del(bx, &t))
                    assert(flref_of(&b->flanc).gen == bx.gen);
                else
                    del_failed = true;
            }else{
                while(lflist_jam(bx, &t)){
                    dbg flref obx = bx;
                    bx = flref_of(&b->flanc);
                    assert(bx.gen != obx.gen);
                }
                bx.gen++;
                assert(flref_of(&b->flanc).gen == bx.gen);
            }
        }
        
        b->enq_attempted = true;
        lpgen lp = PUN(lpgen, (uptr) bx.gen);
        lflist *nl = randpcnt(30) ? &priv : rand_shared_list();
        
        if(lflist_enq_cas(rup(lp,
                              .gen++,
                              .last_priv = (nl == &priv ? id : 0)),
                          bx, nl, &t))
            assert(b->owner != id || del_failed);
        
        linref_down(flptr(bx), &t);
    }

    ltthread_finish(&priv);
}

static void time_del(uptr id){
    ltthread_init(&shared[0], id);

    for(cnt i = 0; i < nallocs; i++)
        muste(lflist_del(flref_of(&next_owned_node()->flanc), &t));

    ltthread_finish(NULL);    
}

static void time_enq_del(uptr id){
    ltthread_init(NULL, id);

    for(cnt i = 0; i < nallocs; i++)
        muste(lflist_enq(flref_of(&next_owned_node()->flanc), rand_shared_list(), &t));

    for(cnt i = 0; i < nallocs; i++)
        muste(lflist_del(flref_of(&next_owned_node()->flanc), &t));

    ltthread_finish(NULL);
}

static void time_enq(uptr id){
    ltthread_init(NULL, id);

    for(cnt i = 0; i < nallocs; i++)
        muste(lflist_enq(flref_of(&next_owned_node()->flanc), rand_shared_list(), &t));

    ltthread_finish(NULL);
}

typedef struct {
    enum{
        JAM,
        ENQ,
    } st:1;
    uptr gen:WORDBITS - 1;
} stgen;

static void test_enq_del_generally(uptr id){
    ltthread_init(&shared[0], id);

    for(cnt i = 0; i < niter; i++){
        node *b = rand_node();
        flref bx = flref_of(&b->flanc);
        stgen stg = PUN(stgen, bx.gen);

        if(randpcnt(50)){
            lflist_del_cas(rup(stg, .gen++, .st = JAM), bx, &t);
            assert(bx.gen != flref_of(&b->flanc).gen);
        }else{
            if(!lflist_enq_cas(rup(stg, .gen++, .st = ENQ), bx,
                               rand_shared_list(), &t))
                assert(stg.st == JAM &&
                       bx.gen != flref_of(&b->flanc).gen);
            else
                assert(stg.st == ENQ ||
                       bx.gen != flref_of(&b->flanc).gen);
        }
    }

    ltthread_finish(NULL);
}

/* TODO: disabling these entirely, for now. */
#if 0
#ifndef FAKELOCKFREE

static void test_validity_bits(uptr id){
    lflist priv[NPRIV];
    list perm[NPRIV];
    heritage *h;
    type *t;
    ltthread_init(priv, perm, &h, id, &t);

    for(uint i = 0; i < niter; i++){
        dbg bool del_failed = false;
        flref bx;
        node *b;
        if(randpcnt(50)){
            bool unenq_priv = randpcnt(50);
            lflist *l = unenq_priv
                      ? &priv[rand() % NPRIV]
                      : rand_shared_list();
            if(!(b = cof(flptr(bx = lflist_unenq(l, &t)), node, flanc)))
                goto grow;
            lpgen lp = PUN(lpgen, (uptr) bx.gen);
            assert(lp.last_priv == (unenq_priv ? id : 0));
        }else{
        grow:
            while(!(b = cof(list_unenq(&perm[rand() % NPRIV]), node, lanc)))
                continue;
            assert(b->owner == id);
            list_enq(&b->lanc, &perm[rand() % NPRIV]);

            muste(linref_up(b, &t));
            bx = flref_of(&b->flanc);
            if(b->invalidated){
                assert(bx.validity != FLANC_VALID);
                flanc_ordered_init(bx.gen, &b->flanc);
                bx.validity = FLANC_VALID;
                b->invalidated = false;
            }
            
            if(randpcnt(50)){
                if(!lflist_del(bx, &t))
                    assert(flref_of(&b->flanc).gen == bx.gen);
                else
                    del_failed = true;
            }else{
                while(lflist_jam(bx, &t)){
                    flref obx = bx;
                    bx = flref_of(&b->flanc);
                    assert(bx.gen != obx.gen);
                }
                bx.gen++;
                assert(flref_of(&b->flanc).gen == bx.gen);
            }

            if(!del_failed && randpcnt(32)){
                must(mgen_upd_won(rup(bx.mgen, .validity=0), bx, &t));
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
        lflist *nl = enq_priv ? &priv[rand() % NPRIV] : rand_shared_list();
        
        if(lflist_enq_cas(rup(lp,
                              .gen++,
                              .last_priv = (enq_priv ? id : 0)),
                          bx, nl, &t))
            assert(b->owner != id || del_failed);
        
        linref_down(flptr(bx), &t);
    }

    ltthread_finish(priv, perm, id, &t);
}

#endif
#endif

#include <signal.h>

int run_list_tests(int argc, char **argv){
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
        case 'r':
            TODO("ephref stuff is out of date.");
            t.has_special_ref = (void *) &has_eph_ref;
            break;
        }
    }
    niter /= nthreads;
    nallocs /= nthreads;
    nallocs = umax(1, nallocs);

    switch(program){
    case 1:
        launch_list_test(test_enq_unenq, 1, "test_enq_unenq");
        break;
    case 2:
        launch_list_test(test_enq_unenq_del, 1, "test_enq_unenq_del");
        break;
    case 3:
        launch_list_test(test_enq_unenq_jam, 1, "test_enq_unenq_jam");
        break;
    case 4:
        nallocs = niter;
        launch_list_test(time_del, 0, "time_del");
        break;
    case 5:
        nallocs = niter;
        launch_list_test(time_enq_del, 0, "time_enq_del");
        break;
    case 6:
        nallocs = niter;
        launch_list_test(time_enq, 0, "time_enq");
        break;
    case 7:
        launch_list_test(test_enq_del_generally, 0, "test_enq_del_generally");
        break;
    case 8:
        launch_list_test(test_enq_deq, 1, "test_enq_deq");
        break;
        
        
/* TODO: just a bit tired of updating fakelflist */
/* TODO: and these are probably broken by now. */
#if 0        
#ifndef FAKELOCKFREE        
    case 7:
        launch_list_test(test_validity_bits, 1, "test_validity_bits");
        break;
#endif
#endif
    default:
        EWTF("Unknown test number.");
    }

    return 0;
}


