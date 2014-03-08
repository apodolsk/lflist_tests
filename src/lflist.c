/**
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * Lockfree doubly-linked list. Good luck!
 */

#define MODULE LFLIST

#include <stdlib.h>
#include <peb_macros.h>
#include <atomics.h>
#include <stdbool.h>
#include <lflist.h>
#include <nalloc.h>

#include <global.h>

#ifndef FAKELOCKFREE

static flx flinref_read(volatile flx *from, flx **held, type *h);
static int flinref_up(flx a, type *h);
static void flinref_down(flx a);
static flx help_next(flx a, flx n, type *h);
static flx help_prev(flx a, flx p, type *h);

static inline flanchor *pt(flx a){
    return (flanchor *) a.mp.ptr;
}

static inline flx atomic_readflx(volatile flx *x){
    return cas2((flx){}, x, (flx){});
}
static inline int geneq(flgen a, flgen b){
    return PUN(int64_t, a) == PUN(int64_t, b);
}
static inline int atomic_flxeq(volatile flx *aptr, flx b){
    flx a = atomic_readflx(aptr);
    return PUN(int128_t, a) == PUN(int128_t, b);
}
static inline int casx_ok(flx n, volatile flx *a, flx e){
    return cas2_ok(n, a, e);
}
static inline flx casx(flx n, volatile flx *a, flx e){
    return cas2(n, a, e);
}

static
flx flinref_read(volatile flx *from, flx **held, type *t){
    while(1){
        flx a = atomic_readflx(from);
        flx *reused = NULL;
        for(flx **h = held; *h; h++){
            if(pt(a) == pt(**h) && !reused)
                reused = *h;
            else if(pt(**h))
                flinref_down(**h);
            **h = (flx){};
        }
        if(reused)
            return *reused;
        if(!pt(a) || !flinref_up(a, t))
            return a;
    }
}

static
int flinref_up(flx a, type *t){
    if(!pt(a))
        return -1;
    if(a.mp.is_nil)
        return 0;
    return linref_up(pt(a), t);
}

static
void flinref_down(flx a){
    assert(pt(a));
    if(!a.mp.is_nil)
        linref_down(pt(a));
}

int lflist_remove(flx a, type *t){
    trace(a.mp.pt, p);
    assert(!a.mp.is_nil);
    assert(aligned_pow2(pt(a), 16));

    flx n = {}, p = {}, plocked;    
    do{
        p = help_prev(a, p, t);
        if(!pt(p) || p.gen.i != a.gen.i)
            break;
        flx oldn;
        do{
            oldn = atomic_readflx(&pt(a)->n);
            n = help_next(a, n, t);
        }while(!casx_ok(n, &pt(a)->n, oldn));
        if(!pt(n))
            break;
        plocked = (flx){p.mp, (flgen) {p.gen.i, .locked = 1 }};
    }while(!casx_ok(plocked, &pt(a)->p, p) ||
           !casx_ok((flx){p.mp, n.gen}, &pt(n)->p, (flx){a.mp, n.gen}));

    int ret = 0;
    if(casx_ok((flx){(mptr){}, a.gen}, &pt(a)->p, plocked)){
        casx(n, &pt(p)->n, a);
        pt(a)->n = (flx){};
    }
    else
        ret = -1;

    if(pt(n))
        flinref_down(n);
    if(pt(p))
        flinref_down(p);
    return ret;
}

static
flx help_next(flx a, flx n, type *t)
{
    trace(a.mp.pt, p, n.mp.pt, p); 
    flx pat = {}, patp = {};
    while(1){
        pat = flinref_read(&pt(a)->n, (flx*[]){&n, &pat, &patp, NULL}, t);
        if(!pt(pat))
            return pat;

        patp = atomic_readflx(&pt(pat)->p);
        if(!geneq(patp.gen, pat.gen)){
            patp = (flx){};
            continue;
        }
        
        if(pt(patp) != pt(a)){
            if(flinref_up(patp, t))
                continue;
            flx patpp = atomic_readflx(&pt(patp)->p);
            /* patp has been added */
            if(pt(patpp) == pt(a)){
                if(!patpp.gen.locked)
                    return flinref_down(pat), patp;
                else
                    continue;
            }
            /* a has been removed */
            if(atomic_flxeq(&pt(a)->n, pat)){
                assert(!a.mp.is_nil);
                return (flx){};
            }
            else
                continue;
        }
        if(!patp.gen.locked)
            return pat;

        /* try helping pat finish its removal transaction */
        n = flinref_read(&pt(pat)->n, (flx*[]){NULL}, t);
        if(pt(n)){
            flx np = casx((flx){a.mp, n.gen}, &pt(n)->p, (flx){pat.mp, n.gen});
            if(pt(np) == pt(a) || (pt(np) == pt(pat) && geneq(np.gen, n.gen)))
                return flinref_down(pat), n;
        }

        /* unlock pat if it hasn't begun a new transaction */
        flx new = {a.mp, (flgen){patp.gen.i, .locked=1, .unlocking=1}};
        if(casx_ok(new, &pt(pat)->p, patp) &&
           atomic_flxeq(&pt(pat)->n, n) &&
           casx_ok((flx){a.mp, (flgen){patp.gen.i}}, &pt(pat)->n, new))
            return flinref_down(n), pat;
    }
}

static
flx help_prev(flx a, flx p, type *t){
    flx n = {};
    do{
        p = flinref_read(&pt(a)->p, (flx*[]){&p, &n, NULL}, t);
        if(p.gen.locked || !pt(p))
            return p;
        n = atomic_readflx(&pt(p)->n);
        if(pt(n) == pt(a))
            return p;
        if(flinref_up(n, t)){
            n = (flx){};
            continue;
        }
        if(!atomic_flxeq(&pt(a)->p, p))
            continue;
        if(pt(pt(n)->n) != pt(a) && atomic_flxeq(&pt(p)->n, n)){
            flinref_down(p);
            flinref_down(n);
            return (flx){};
        }
    }while(!casx_ok(a, &pt(p)->n, n));
    return p;
}

void lflist_add_before(flx a, flx n, type *t){
    trace(a.mp.pt, p, n.mp.pt, p);
    assert(aligned_pow2(pt(a), 16));

    a.gen.i += GEN_INTVL;
    pt(a)->n = n;
    pt(a)->p.gen = a.gen;
    flx p = {};
    do{
        p = help_prev(n, p, t);
        assert(pt(p));
        assert(geneq(p.gen, n.gen));
        assert(geneq(a.gen, pt(a)->p.gen));

        pt(a)->p.mp = p.mp;
    }while(!casx_ok((flx){a.mp, p.gen}, &pt(n)->p, p));

    casx(a, &pt(p)->n, n);
    
    flinref_down(p);
}

void lflist_add_rear(flx a, type *t, lflist *l){
    assert(pt(a) != &l->nil);
    assert(aligned_pow2(l, 16));
    
    lflist_add_before(a, (flx){mptr(&l->nil, 1)}, t);
}

flx lflist_pop_front(type *t, lflist *l){
    trace(l,p);
    assert(aligned_pow2(l, 16));

    for(flx n = {};;){
        n = help_next((flx){mptr(&l->nil, 1)}, n, t);
        assert(pt(n));
        if(n.mp.is_nil){
            assert(pt(n) == &l->nil);
            return (flx){};
        }
        if(!lflist_remove(n, t))
            return flinref_down(n), n;
    }
}

flanchor *flptr(flx a){
    assert(!a.mp.is_nil);
    return pt(a);
}

flx flx_of(flanchor *a){
    return (flx){mptr(a, 0), a->p.gen};
}

#endif
