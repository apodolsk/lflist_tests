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

static flx flinref_read(volatile flx *from, flx **held, type *t);
static int flinref_up(flx a, type *t);
static void flinref_down(flx a, type *t);
static err help_next(flx a, flx *n, flx *np, type *t);
static err help_prev(flx a, flx *p, flx *pn, type *t);

static inline flanchor *pt(flx a){
    return (flanchor *) (a.pt << 2);
}
static inline flx casx(const char *f,
                       int l, flx n, volatile flx *a, flx e){
    llprintf1("CAS! %s:%d - %s if %s, addr:%p", f, l, str(n), str(e), a); 
    flx r = cas2(n, a, e);
    llprintf1("%s - found:%s addr:%p", eq2(r,e)? "WON" : "LOST", str(r), a);
    return r;
}
static inline enum okr{
    NOT = 0,
    OK = 1,
    WON = 2
} casx_ok(const char *f, int l,
                          flx n, volatile flx *a, flx e){
    flx r = casx(f, l, n, a, e);
    if(rq2(r, e))
        return OK;
    if(eq2(e, n))
        return WON;
    return NOTK;
}

static inline int casx_won(const char *f, int l,
                           flx n, volatile flx *a, flx e){
    return eq2(casx(f, l, n, a, e), e);
}

#define casx(as...) casx(__func__, __LINE__, as)
#define casx_ok(as...) casx_ok(__func__, __LINE__, as)
#define casx_won(as...) casx_won(__func__, __LINE__, as)

static inline flx atomic_readx(volatile flx *x){
    return cas2((flx){}, x, (flx){});
}
static inline bool atomic_eqx(volatile flx *a, flx *b, type *t){
    while(1){
        flx old = *b;
        *b = atomic_readx(a);
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
    while(1){
        flx a = atomic_readx(from);
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

    flx n = {}, p = {}, pn, np;
    enum okr last_lock;
    while(1){
        if(help_next(a, &n, &np, t)){
            RARITY("N abort");
            break;
        }
        if(help_prev(a, &p, &pn, t) || p.gen != a.gen){
            RARITY("P abort");
            break;
        }
        last_lock = casx_ok((flx){.nil=n.nil, 1, n.pt, n.gen + 1}, &pt(a)->n, n)
        if(last_lock &&
           casx_ok((flx){.nil=n.nil, 0, n.pt, pn.gen + 1}, &pt(p)->n, pn)){
            break;
        }
    }

    int ret = -1;
    if(last_lock != WON || !n.locked || p.gen != a.gen)
        RARITY("%s", str(p));
    else{
        casx((flx){p.mp, np.gen}, &pt(n)->p, np);
        if(casx_won((flx){.gen = a.gen}, &pt(a)->p, p))
            ret = 0;
    }
    
    if(pt(n))
        flinref_down(n, t);
    if(pt(p))
        flinref_down(p, t);
    return ret;
}

static
err (help_next)(flx a, flx *n, flx *np, type *t){
    while(1){
        *n = flinref_read(&pt(a)->n, (flx*[]){n, NULL}, t);
    newn:
        if(!pt(*n))
            return assert(!a.nil), -1;
        *np = atomic_readx(&pt(*n)->p);
    newnp:;
        if(!pt(*np) || pt(*np) == pt(a)){
            if(!atomic_eqx(&pt(a)->n, n, t))
                goto newn;
            else
                return pt(*np) == pt(a) ? 0 : (assert(!a.nil), -1);
        }
        flx npp = atomic_readx(&pt(*np)->p);
        if(pt(npp) != pt(a)){
            if(!atomic_eqx(&pt(a)->n, n, t))
                goto newn;
            if(!eq2(*np, *np = atomic_readx(&pt(*n)->p)))
                goto newnp;
            if(!atomic_eqx(&pt(a)->n, n, t))
                goto newn;
            return assert(!a.nil), -1;
        }
        if(n->nil && !atomic_eqx(&pt(a)->n, n, t))
            goto newn;
        if(casx_ok((flx){a.mp, np->gen}, &pt(*n)->p, *np))
            return 0;
    }
}

static
err (help_prev)(flx a, flx *p, flx *pn, type *t){
    flx pp = {};
    while(1){
        *p = flinref_read(&pt(a)->p, (flx*[]){p, &pp, NULL}, t);
    newp:
        if(!pt(*p) || (!a.nil && p->gen != a.gen))
            return -1;

        *pn = atomic_readx(&pt(*p)->n);
        if(!atomic_eqx(&pt(a)->p, p, t))
            goto newp;
        if(pt(*pn) != pt(a))
            return -1;
    newpn:
        if(!pn->locked)
            return 0;

        pp = flinref_read(&pt(*p)->p, (flx*[]){&pp, NULL}, t);
        if(!pt(pp))
            continue;
        flx ppn = atomic_readx(&pt(pp)->n);
        if(!atomic_eqx(&pt(a)->p, p, t))
            goto newp;
        if(pt(ppn) != pt(*p) && pt(ppn) != pt(a))
            continue;
        assert(!ppn.nil || !ppn.locked);
        
        flx new = (flx){a.mp, ppn.gen + 1};
        if(pt(ppn) == pt(a) || (!ppn.locked && casx_ok(new, &pt(pp)->n, ppn))){
            if(!casx_ok((flx){pp.mp, p->gen}, &pt(a)->p, *p))
                continue;
            flinref_down(*p, t);
            *p = (flx){pp.mp, p->gen};
            if(pt(ppn) != pt(a))
                return *pn = new, 0;
            *pn = ppn;
            goto newpn;
        }
        
        flx newpn = (flx){.nil=a.nil, a.pt, pn->gen + 1};
        if(casx_ok(newpn, &pt(*p)->n, *pn))
            return *pn = newpn, 0;
    }
}

err (lflist_enq)(flx a, type *t, lflist *l){
    if(!casx_won((flx){.gen = a.gen + 1}, &pt(a)->p, (flx){.gen = a.gen}))
        return -1;
    pt(a)->n = (flx){.nil=1, .pt=mpt(&l->nil), .gen = pt(a)->n.gen + 1};

    flx p = {}, pn = {};
    while(1){
        if(help_prev((flx){.nil = 1, .pt = mpt(&l->nil)}, &p, &pn, t)){
            if(pt(p))
                casx((flx){.pt = pn.pt, p.gen + 1}, &l->nil.p, p);
            continue;
        }
        
        pt(a)->p.mp = p.mp;
        if(casx_won((flx){.mp = a.mp, pn.gen + 1}, &pt(p)->n, pn))
            break;
    }

    casx((flx){.mp = a.mp, .gen = p.gen + 1}, &l->nil.p, p);
    return 0;
}

flx (lflist_deq)(type *t, lflist *l){
    flx n = {}, np;
    while(1){
        if(help_next((flx){.nil = 1, .pt = mpt(&l->nil)}, &n, &np, t))
            EWTF();
        assert(pt(n));
        if(n.nil)
            return assert(&l->nil == pt(n)), (flx){};
        if(!lflist_del(((flx){n.mp, np.gen}), t))
            return (flx){n.mp, np.gen};
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
