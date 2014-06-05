#define MODULE LFLISTM

#include <stdlib.h>
#include <peb_util.h>
#include <atomics.h>
#include <stdbool.h>
#include <lflist.h>
#include <nalloc.h>
#include <inttypes.h>

#include <global.h>

#undef TS
#define TS (LFLIST_TS)

#ifndef FAKELOCKFREE

#define MAX_LOOP 32
#define TEST_PROGRESS(c)                                                 \
    ({ pp(c); if(MAX_LOOP && c++ > MAX_LOOP) SUPER_RARITY("LOTTA LOOPS %", c); })

static flx flinref_read(volatile flx *from, flx **held, type *t);
static int flinref_up(flx a, type *t);
static void flinref_down(flx a, type *t);
static err help_next(flx a, flx *n, flx *np, type *t);
static err help_prev(flx a, flx *p, flx *pn, type *t);

static inline flanchor *pt(flx a){
    return (flanchor *) (a.pt << 3);
}
static inline flx casx(const char *f, int l, flx n, volatile flx *a, flx *e){
    lprintf("CAS! %:% - % if %, addr:%", f, l, n, *e, a);
    flx oe = *e;
    *e = cas2(n, a, oe);
    lprintf("% %:%- found:% addr:%", eq2(*e, oe)? "WON" : "LOST", f, l, *e, a);
    return *e;
}
static inline enum howok{
    NOT = 0,
    OK = 1,
    WON = 2
} casx_ok(const char *f, int l, flx n, volatile flx *a, flx *e){
    flx oe = *e;
    casx(f, l, n, a, e);
    if(eq2(*e, oe))
        return WON;
    if(eq2(*e, n))
        return OK;
    return NOT;
}

static inline int casx_won(const char *f, int l,
                           flx n, volatile flx *a, flx *e){
    return eq2(*e, casx(f, l, n, a, e));
}

#define casx(as...) casx(__func__, __LINE__, as)
#define casx_ok(as...) casx_ok(__func__, __LINE__, as)
#define casx_won(as...) casx_won(__func__, __LINE__, as)

static inline flx readx(volatile flx *x){
    if(!aligned_pow2(x, sizeof(dptr)))
        return (flx){};
    /* flx r = (flx){.gen = atomic_read(&x->gen), .mp = atomic_read(&x->mp)}; */
    /* return r; */
    return cas2((flx){}, x, (flx){});
}
static inline bool eqx(volatile flx *a, flx *b, type *t){
    for(int c = 0;; TEST_PROGRESS(c)){
        flx old = *b;
        *b = readx(a);
        if(eq2(old, *b))
            return true;
        if(pt(old) == pt(*b))
            return false;
        if(!pt(*b) || !flinref_up(*b, t)){
            flinref_down(old, t);
            return false;
        }
    }
}
static
flx (flinref_read)(volatile flx *from, flx **held, type *t){
    for(int c = 0;; TEST_PROGRESS(c)){
        flx a = readx(from);
        flx *reused = NULL;
        for(flx **h = held; *h; h++){
            if(pt(a) == pt(**h) && !reused)
                reused = *h;
            else if(pt(**h))
                flinref_down(**h, t);
            **h = (flx){};
        }
        if(reused || !pt(a) || !flinref_up(a, t))
            return a;
    }
}

static
int flinref_up(flx a, type *t){
    if(a.nil)
        return 0;
    return t->linref_up(pt(a), t);
}

static
void flinref_down(flx a, type *t){
    if(!a.nil)
        t->linref_down(pt(a));
}

err (lflist_del)(flx a, type *t){
    assert(!a.nil);

    flx lock = {}, pn, np, p = {};
    flx n = flinref_read(&pt(a)->n, (flx*[]){NULL}, t);
    for(int c = 0;; TEST_PROGRESS(c)){
        if(help_next(a, &n, &np, t)){
            RARITY("N abort n:% np:%", n, np);
            break;
        }
        if(help_prev(a, &p, &pn, t) || p.gen != a.gen){
            RARITY("P abort p:% pn:%", p, pn);
            n = readx(&pt(a)->n);
            break;
        }
        assert(!pn.locked);

        lock = (flx){.nil=n.nil, 1, 0, n.pt, n.gen + 1};
        enum howok r = casx_ok(lock, &pt(a)->n, &n);
        if(r != NOT){
            n = lock;            
            if(r != WON) lock = (flx){};
            if(casx_ok((flx){.nil=n.nil,0,0,n.pt,pn.gen+1}, &pt(p)->n, &pn))
                break;
        }else lock = (flx){};
    }

    err e = -1;
    if(!p.locked && p.mp && p.gen == a.gen &&
       ((lock.mp && n.gen == lock.gen) || n.helped))
    {
        if(!casx_won((flx){.nil=p.nil,1,0,p.pt,p.gen}, &pt(a)->p, &p)){
            if(p.locked || !p.mp || p.gen != a.gen)
                goto cleanup;
            if(!casx_won((flx){.nil=p.nil,1,0,p.pt,p.gen}, &pt(a)->p, &p)){
                assert(p.locked || !p.mp || p.gen != a.gen);
                goto cleanup;
            }
        }
        assert(!p.locked && !p.helped);
        if(n.helped && pt(n = flinref_read(&pt(a)->n, (flx*[]){&n, NULL},t)))
            np = readx(&pt(n)->p);
        if(pt(np) == pt(a)){
            casx((flx){p.mp, np.gen}, &pt(n)->p, &np);
            /* Clean up after an interrupted add of 'n'. In this case,
               a->n is the only reference to 'n', and we should finish the
               add before it gets cleared (next time a is added). */
            flx nn = pt(n)->n;
            if(nn.nil){
                flx nnp = pt(nn)->p;
                if(pt(nnp) == pt(a))
                    casx((flx){.pt=n.pt, nnp.gen + 1}, &pt(nn)->p, &nnp);
            }
        }

        pt(a)->p = (flx){.gen = a.gen};
        e = 0;
    }
    else
        RARITY("p:%, n:%, l:%", p, n, lock);
cleanup:    
    if(pt(n))
        flinref_down(n, t);
    if(pt(p))
        flinref_down(p, t);
    return e;
}

static
err (help_next)(flx a, flx *n, flx *np, type *t){
    for(int c = 0;;){
    newn:
        assert(a.nil || pt(*n) != pt(a));
        assert(pt(*n));
        if(n->helped)
            return assert(!a.nil), -1;
        *np = readx(&pt(*n)->p);
    newnp:
        TEST_PROGRESS(c);
        if(pt(*np) == pt(a))
            return 0;
        if(!pt(*np) || np->locked || np->nil){
            if(!eqx(&pt(a)->n, n, t))
                goto newn;
            else return assert(!a.nil), -1;
        }
        
        flx npp = readx(&pt(*np)->p);
        pp(*n, *np, npp);
        if(!eqx(&pt(a)->n, n, t)){
            lprintf("n changed");
            goto newn;
        }
        if(pt(npp) != pt(a)){
            if(!eq2(*np, (*np = readx(&pt(*n)->p)))){
                lprintf("np changed");
                goto newnp;
            }
            else return assert(!a.nil), -1;
        }
        
        RARITY("Swinging n:% np:% npp:%", *n, *np, npp);
        if(casx_ok((flx){a.mp, np->gen}, &pt(*n)->p, np))
            return *np = (flx){a.mp, np->gen}, 0;
        *n = flinref_read(&pt(a)->n, (flx*[]){n, NULL}, t);
    }
}

static
err (help_prev)(flx a, flx *p, flx *pn, type *t){
    flx pp = {};
    for(int c = 0;;){
        *p = flinref_read(&pt(a)->p, (flx*[]){p, &pp, NULL}, t);
    newp:
        assert(a.nil || (pt(*p) != pt(a)));
        if(!pt(*p) || p->locked || (!a.nil && p->gen != a.gen))
            return *pn = (flx){}, -1;

        *pn = readx(&pt(*p)->n);
    newpn:
        if(!eqx(&pt(a)->p, p, t))
            goto newp;
        if(pt(*pn) != pt(a))
            return -1;
        TEST_PROGRESS(c);
        if(!pn->locked)
            return 0;
        
        pp = flinref_read(&pt(*p)->p, (flx*[]){&pp, NULL}, t);
        if(!pt(pp))
            continue;
        flx ppn = readx(&pt(pp)->n);
        if(pt(ppn) != pt(*p) && pt(ppn) != pt(a))
            continue;
        if(!eqx(&pt(a)->p, p, t))
            goto newp;
        assert(!ppn.nil || !ppn.locked);
        
        if(pt(ppn) == pt(a) ||
           (!ppn.locked &&
            (casx_ok((flx){.nil=a.nil, 1, 1, a.pt, pn->gen + 1},
                     &pt(*p)->n, pn) ?: ({goto newpn;0;})) &&
            casx_ok((flx){a.mp, ppn.gen + 1}, &pt(pp)->n, &ppn)))
        {
            flinref_down(*p, t);
            if(!casx_ok((flx){.nil=pp.nil, .pt=pp.pt, p->gen}, &pt(a)->p, p))
                 goto newp;
            *p = (flx){.nil=pp.nil, .pt=pp.pt, p->gen};
            if(pt(ppn) != pt(a)){
                *pn = (flx){a.mp, ppn.gen + 1};
                return 0;
            }
            *pn = ppn;
            goto newpn;
        }
        if(!eqx(&pt(a)->p, p, t))
            goto newp;

        pp(a, p, pp, ppn);
        flx newpn = (flx){.nil=a.nil, 0, 0, a.pt, pn->gen + 1};
        if(casx_ok(newpn, &pt(*p)->n, pn))
            return *pn = newpn, 0;
    }
}

err (lflist_enq)(flx a, type *t, lflist *l){
    if(!casx_won((flx){.gen = a.gen + 1}, &pt(a)->p, &(flx){.gen = a.gen}))
        return -1;
    pt(a)->n = (flx){.nil=1, .pt=mpt(&l->nil), .gen = pt(a)->n.gen + 1};

    flx p = {}, pn = {};
    for(int c = 0;;TEST_PROGRESS(c)){
        if(help_prev((flx){.nil = 1, .pt = mpt(&l->nil)}, &p, &pn, t)){
            assert(pt(p));
            assert(pt(pn));
            casx((flx){.pt = pn.pt, p.gen + 1}, &l->nil.p, &p);
            continue;
        }
        assert(!pn.locked);
        assert(pn.nil);
        pp(p, pn);
        
        pt(a)->p.mp = p.mp;
        if(casx_won((flx){.mp = a.mp, pn.gen + 1}, &pt(p)->n, &pn))
            break;
    }

    casx((flx){.mp = a.mp, .gen = p.gen + 1}, &l->nil.p, &p);
    return 0;
}

flx (lflist_deq)(type *t, lflist *l){
    flx np, oldn = {};
    flx n = flinref_read(&l->nil.n, (flx*[]){NULL}, t);
    for(int c = 0;;TEST_PROGRESS(c)){
        if(help_next((flx){.nil = 1, .pt = mpt(&l->nil)}, &n, &np, t))
            EWTF();
        if(n.nil)
            return assert(&l->nil == pt(n)), (flx){};
        if(eq2(oldn, n))
            return (flx){};
        if(!lflist_del(((flx){n.mp, np.gen}), t))
            return (flx){n.mp, np.gen};
        oldn = n;
    }
}

flanchor *flptr(flx a){
    assert(!a.nil);
    return pt(a);
}

flx flx_of(flanchor *a){
    return (flx){.pt = mpt(a), a->p.gen};
}


#endif
