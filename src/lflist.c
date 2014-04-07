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

static err flinref_read(flx *r, volatile flx *from, flx **held, type *t);
static int flinref_up(flx a, type *t);
static void flinref_down(flx a, type *t);
static err help_next(flx a, flx *n, flx *np, type *t);
static err help_prev(flx a, flx *p, flx *pn, type *t);

static inline flanchor *pt(flx a){
    return (flanchor *) (a.pt << 2);
}
static inline flx atomic_readx(volatile flx *x){
    return cas2((flx){}, x, (flx){});
}
static inline bool atomic_eqx(volatile flx *a, flx *b, type *t){
    flx old = *b;
    *b = atomic_readx(a);
    if(eq2(old, b))
        return true;
    if(flinref_up(*b, t))
        *b = (flx){};
    flinref_down(*b, t);
    return false;
}
static inline flx casx(const char *f,
                       int l, flx n, volatile flx *a, flx e){
    llprintf1("CAS! %s:%d - %s if %s, addr:%p", f, l, str(n), str(e), a); 
    flx r = cas2(n, a, e);
    llprintf1("%s - found:%s addr:%p", eq2(r,e)? "WON" : "LOST", str(r), a);
    return r;
}
static inline int casx_ok(const char *f, int l,
                          flx n, volatile flx *a, flx e){
    flx r = casx(f, l, n, a, e);
    return eq2(r, e) || eq2(r, n);
}

static inline int casx_won(const char *f, int l,
                           flx n, volatile flx *a, flx e){
    return eq2(casx(f, l, n, a, e), e);
}

#define casx(as...) casx(__func__, __LINE__, as)
#define casx_ok(as...) casx_ok(__func__, __LINE__, as)
#define casx_won(as...) casx_won(__func__, __LINE__, as)
 
static
err (flinref_read)(flx *r, volatile flx *from, flx **held, type *t){
    *r = atomic_readx(from);
    flx *reused = NULL;
    for(flx **h = held; *h; h++){
        if(pt(*r) == pt(**h) && !reused)
            reused = *h;
        else if(pt(**h))
            flinref_down(**h, t);
        **h = (flx){};
    }
    if(reused)
        return 0;
    if(!pt(*r) || !flinref_up(*r, t))
        return -1;
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
    while(1){
        if(help_next(a, &n, &np, t)){
            RARITY("N abort");
            break;
        }
        if(help_prev(a, &p, &pn, t) || p.gen != a.gen){
            RARITY("P abort");
            break;
        }
        if(casx_ok((flx){.nil=n.nil, 1, n.pt, n.gen + 1}, &pt(a)->n, n) &&
           casx_ok((flx){n.mp, pn.gen + 1}, &pt(p)->n, pn)){
            casx((flx){.mp=p.mp, np.gen}, &pt(n)->p, np);
            break;
        }
    }

    int ret = -1;
    if(!pt(n) || p.gen != a.gen || !n.locked)
        RARITY("%s", str(p));
    else if(casx_won((flx){.gen = a.gen}, &pt(a)->p, p)){
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
        flinref_read(n, &pt(a)->n, (flx*[]){n, NULL}, t);
    newn:
        if(!pt(*n))
            return -1;
        *np = atomic_readx(&pt(*n)->p);
        if(pt(*np) == pt(a)){
            if(!atomic_eqx(&pt(a)->n, n, t))
                goto newn;
            else
                return 0;
        }
        flx npp = atomic_readx(&pt(*np)->p);
        if(!atomic_eqx(&pt(a)->n, *n, t))
            goto newn;
        if(pt(npp) != pt(a))
            return -1;
        if(casx_ok((flx){a.mp, np->gen}, &pt(n)->p, *np))
            return 0;
    }
}

static
err (help_prev)(flx a, flx *p, flx *pn, type *t){
    flx pp = {};
    while(1){
        *p = flinref_read(&pt(a)->p, (flx*[]){p, &pp, NULL}, t);
    newp:
        if(!pt(*p) || (!a.nil && p.gen != a.gen))
            return -1;

        *pn = atomic_readx(&pt(p)->n);
        if(!atomic_eqx(&pt(a)->p, p))
            goto newp;
        if(pt(*pn) != pt(a))
            return -1;
    newpn:
        if(!pn->locked)
            return 0;

        flinref_read(pp, &pt(p)->p, (flx*[]){pp, NULL}, t);
        /* TODO: !pt(pp)? */
        flx ppn = atomic_readx(&pt(pp)->n);
        if(pt(ppn) != pt(p))
            continue;
        
        flx new = (flx){a.mp, ppn.gen + 1};
        if(!ppn.locked && casx_ok(new, &pt(pp)->n, ppn) || pt(ppn) == pt(a)){
            if(!casx_ok((flx){pp.mp, p->gen}, &pt(a)->p, *p))
                continue;
            flinref_down(*p);
            *p = pp;
            if(ppn != pt(a))
                return *pn = new, 0;
            *pn = ppn;
            goto newp;
        }
        
        flx newpn = (flx){a.nil, a.pt, pn->gen + 1};
        if(casx_ok(newpn, &pt(*p)->n, *pn))
            return *pn = newpn, 0;
    }
}

static
err lflist_enq(flx a, type *t, lflist *l){
    if(!casx_ok((flx){.gen = a.gen + 1}, &pt(a)->p, (flx){.gen = a.gen}))
        return -1;
    pt(a)->n.pt = mpt(&l->nil);
    pt(a)->n.gen++;

    flx p = {}, pn;
    while(1){
        if(help_prev((flx){.nil = 1, .pt = mpt(&l->nil)}, &p, &pn, t)){
            if(pt(pn))
                casx((flx){.pt = pn.pt, p.gen + 1}, &l->nil.p, p);
            continue;
        }
        
        pt(a)->p.mp = p.mp;
        if(casx_ok((flx){.mp = a.mp, pn.gen, &pt(p)->n, pn}))
            break;
    }

    casx((flx){.mp = p.mp, .gen = p.gen + 1}, &l->nil.p, p);
}

static
err lflist_deq(type *t, lflist *l){
    flx n = {}, np, a = (flx){.nil = 1, .pt = mpt(&l->nil)};
    while(1){
        help_next(a, &n, &np);
        assert(pt(*n));
        if(!lflist_del((flx){n.mp, np.gen}, t))
            return 0;
    }
}

flanchor *flptr(flx a){
    assert(!a.mp.is_nil);
    return pt(a);
}

flx flx_of(flanchor *a){
    return (flx){.pt = mpt(a), a->p.gen};
}


#endif
