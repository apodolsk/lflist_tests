#define MODULE LFLISTM

#include <stdlib.h>
#include <peb_util.h>
#include <atomics.h>
#include <stdbool.h>
#include <lflist.h>
#include <nalloc.h>
#include <inttypes.h>

#include <global.h>

#define TS LFLIST_TS
#define str(a) pustr(a, TS)

#ifndef FAKELOCKFREE

#define MAX_LOOP 100
#define TEST_PROGRESS(...)
/* #define TEST_PROGRESS(c) ((void)(c++ > MAX_LOOP ? SUPER_RARITY("LOTTA LOOPS %d", c) : 0), 1) */

static flx flinref_read(volatile flx *from, flx **held, type *t);
static int flinref_up(flx a, type *t);
static void flinref_down(flx a, type *t);
static err help_next(flx a, flx *n, flx *np, type *t);
static err help_prev(flx a, flx *p, flx *pn, type *t);

__attribute__((pure))
static inline flanchor *pt(flx a){
    return (flanchor *) (a.pt << 2);
}
static inline flx casx(const char *f, int l, flx n, volatile flx *a, flx *e){
    llprintf1("CAS! %s:%d - %s if %s, addr:%p", f, l, str(n), str(e), a);
    flx oe = *e;
    *e = cas2(n, a, oe);
    llprintf1("%s %s:%d- found:%s addr:%p", eq2(*e, oe)? "WON" : "LOST", f, l, str(*e), a);
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
    for(int c = 0; TEST_PROGRESS(c);){
        flx old = *b;
        *b = readx(a);
        /* b->mp = atomic_read(&a->mp); */
        /* b->gen = atomic_read(&a->gen); */
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
    for(int c = 0; TEST_PROGRESS(c);){
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
    if(!pt(a))
        return -1;
    if(a.nil)
        return 0;
    return t->linref_up(pt(a), t);
}

static
void flinref_down(flx a, type *t){
    assert(pt(a));
    if(!a.nil)
        t->linref_down(pt(a));
}

err (lflist_del)(flx a, type *t){
    assert(!a.nil);

    flx lock = {}, pn, np, p = {};
    flx n = flinref_read(&pt(a)->n, (flx*[]){NULL}, t);
    for(int c = 0; TEST_PROGRESS(c);){
        if(help_next(a, &n, &np, t)){
            RARITY("N abort n:%s np:%s", str(n), str(np));
            break;
        }
        if(help_prev(a, &p, &pn, t) || p.gen != a.gen){
            RARITY("P abort");
            n = pt(a)->n;
            break;
        }
        
        lock = (flx){.nil=n.nil, 1, n.pt, n.gen + 1};
        enum howok r = casx_ok(lock, &pt(a)->n, &n);
        if(r){
            n = lock;            
            if(r != WON) lock = (flx){};
            if(casx_ok((flx){.nil=n.nil, 0, n.pt, pn.gen+1}, &pt(p)->n, &pn))
                break;
        }else lock = (flx){};
    }

    int ret = -1;
    if(lock.mp && n.gen == lock.gen){
        assert(p.gen == a.gen);
        p = pt(a)->p;
        if(p.gen == a.gen){
            if(pt(np) == pt(a))
                casx((flx){p.mp, np.gen}, &pt(n)->p, &np);
            if(casx_won((flx){.gen = a.gen}, &pt(a)->p, &p)){
                if(pt(np) == pt(a)){
                    flx nn = pt(n)->n;
                    if(nn.nil && !flinref_up(nn, t)){
                        flx nnp = pt(nn)->p;
                        if(pt(nnp) == pt(a))
                            casx((flx){n.mp, nnp.gen + 1}, &pt(nn)->p, &nnp);
                        flinref_down(nn, t);
                    }
                }
                ret = 0;
            }
        }
    }
    else
        RARITY("p:%s, n:%s, l:%s", str(p), str(n), str(lock));
    
    if(pt(n))
        flinref_down(n, t);
    if(pt(p))
        flinref_down(p, t);
    return ret;
}

static
err (help_next)(flx a, flx *n, flx *np, type *t){
    for(int c = 0;;){
    newn:
        assert(a.nil || pt(*n) != pt(a));
        assert(pt(*n));
    newnp:
        *np = readx(&pt(*n)->p);
        if(pt(*np) == pt(a))
            return 0;
        if(!pt(*np) || np->nil){
            if(!eqx(&pt(a)->n, n, t))
                goto newn;
            else return assert(!a.nil), -1;
        }
        
        TEST_PROGRESS(c);
        flx npp = readx(&pt(*np)->p);
        if(!eqx(&pt(a)->n, n, t))
            goto newn;
        if(pt(npp) != pt(a)){
            if(!eq2(*np, readx(&pt(*n)->p)))
                goto newnp;
            else return assert(!a.nil), -1;
        }
        RARITY("Swinging n:%s np:%s npp:%s", str(*n), str(*np), str(npp));
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
        if(!pt(*p) || (!a.nil && p->gen != a.gen))
            return *pn = (flx){}, -1;

        *pn = readx(&pt(*p)->n);
        if(!eqx(&pt(a)->p, p, t))
            goto newp;
        if(pt(*pn) != pt(a))
            return -1;
    newpn:
        if(!pn->locked)
            return 0;

        TEST_PROGRESS(c);
        pp = flinref_read(&pt(*p)->p, (flx*[]){&pp, NULL}, t);
        if(!pt(pp))
            continue;
        flx ppn = readx(&pt(pp)->n);
        if(pt(ppn) != pt(*p) && pt(ppn) != pt(a))
            continue;
        if(!eqx(&pt(a)->p, p, t))
            goto newp;
        if(!eqx(&pt(p)->n, &pn, t))
            goto newpn;
        assert(!ppn.nil || !ppn.locked);
        
        flx new = (flx){a.mp, ppn.gen + 1};
        if(pt(ppn) == pt(a) ||
           (!ppn.locked && casx_ok(new, &pt(pp)->n, &ppn)))
        {
            if(!casx_ok((flx){pp.mp, p->gen}, &pt(a)->p, p))
                goto newp;
            flinref_down(*p, t);
            *p = (flx){pp.mp, p->gen};
            if(pt(ppn) != pt(a))
                return *pn = new, 0;
            *pn = ppn;
            goto newpn;
        }
        if(!eqx(&pt(a)->p, p, t))
            goto newp;

        log(a, p, pp, ppn);
        flx newpn = (flx){.nil=a.nil, .pt=a.pt, pn->gen + 1};
        if(casx_ok(newpn, &pt(*p)->n, pn))
            return *pn = newpn, 0;
    }
}

err (lflist_enq)(flx a, type *t, lflist *l){
    if(!casx_won((flx){.gen = a.gen + 1}, &pt(a)->p, &(flx){.gen = a.gen}))
        return -1;
    pt(a)->n = (flx){.nil=1, .pt=mpt(&l->nil), .gen = pt(a)->n.gen + 1};

    flx p = {}, pn = {};
    for(int c = 0; TEST_PROGRESS(c);){
        if(help_prev((flx){.nil = 1, .pt = mpt(&l->nil)}, &p, &pn, t)){
            assert(pt(p));
            casx((flx){.pt = pn.pt, p.gen + 1}, &l->nil.p, &p);
            continue;
        }
        assert(pn.nil);
        log(p, pn);
        
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
    for(int c = 0; TEST_PROGRESS(c);){
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
