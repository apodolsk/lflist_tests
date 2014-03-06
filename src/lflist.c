
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

static flx flinref_read(flx *from, flx **held, heritage *h, lflist *l);
static int flinref_up(flx a, heritage *h, lflist *l);
static void flinref_down(flx a, lflist *l);
static flx help_next(flx a, flx n, heritage *h, lflist *l);
static flx help_prev(flx a, flx p, heritage *h, lflist *l);

static inline flx atomic_readflx(flx *x){
    return (flx) cas2((flx){}, x, (flx){});
}
static inline int geneq(flgen a, flgen b){
    return PUN(int64_t, a) == PUN(int64_t, b);
}
static inline int atomic_flxeq(flx *aptr, flx b){
    flx a = atomic_readflx(aptr);
    return PUN(int128_t, a) == PUN(int128_t, b);
}
static inline int casx_ok(flx n, flx *a, flx e){
    return cas2_ok(n, a, e);
}
static inline flx casx(flx n, flx *a, flx e){
    return cas2(n, a, e);
}

static
flx flinref_read(flx *from, flx **held, heritage *h, lflist *l){
    while(1){
        flx a = atomic_readflx(from);
        
        flx *reused = NULL;
        for(; *held; held++){
            if(a.pt == (*held)->pt)
                reused = *held;
            else
                flinref_down(**held, l);
        }
        if(reused)
            return *reused;
        
        if(!a.pt || !flinref_up(a, h, l))
            return a;
    }
}

static
int flinref_up(flx a, heritage *h, lflist *l){
    assert(a.pt);
    if(a.pt == &l->nil)
        return 0;
    if(!a.pt)
        return -1;
    return linref_up((void *) a.pt, h);
}

static
void flinref_down(flx a, lflist *l){
    if(a.pt && a.pt == &l->nil)
        linref_down(a.pt);
}

int lflist_remove(flx a, heritage *h, lflist *l){
    assert(a.pt != &l->nil);
    assert(aligned_pow2(l, 16));
    assert(aligned_pow2(a.pt, 16));

    flx n = {}, p = {};
    do{
        p = help_prev(a, p, h, l);
        if(!p.pt || p.gen.i != a.gen.i)
            break;
        flx oldn;
        do{
            oldn = atomic_readflx(&a.pt->n);
            n = help_next(a, n, h, l);
        }while(!casx_ok(n, &a.pt->n, oldn));
        if(!n.pt)
            break;
    }while(!casx_ok((flx){p.pt, (flgen){p.gen.i, .locked = 1}}, &a.pt->p, p) ||
           !casx_ok((flx){p.pt, n.gen}, &n.pt->p, (flx){a.pt, n.gen}));

    int ret = 0;
    if(casx_ok((flx){NULL, a.gen}, &a.pt->p, p)){
        casx(n, &p.pt->n, a);
        a.pt->n = (flx){};
    }
    else
        ret = -1;

    flinref_down(n, l);
    flinref_down(p, l);
    return ret;
}

static
flx help_prev(flx a, flx p, heritage *h, lflist *l){
    flx n;
    do{
        p = flinref_read(&a.pt->p, (flx*[]){&p, &n, NULL}, h, l);
        if(!p.pt)
            return (flx){};
        n = atomic_readflx(&p.pt->n);
        if(n.pt == a.pt)
            return p;
        if(flinref_up(n, h, l)){
            n = (flx){};
            continue;
        }
        if(!atomic_flxeq(&a.pt->p, p))
            continue;
        if(n.pt->n.pt != a.pt && atomic_flxeq(&p.pt->n, n)){
            flinref_down(p, l);
            flinref_down(n, l);
            return (flx){};
        }
    }while(!casx_ok((flx){a.pt, p.gen}, &p.pt->n, n));
    return p;
}

static
flx help_next(flx a, flx n, heritage *h, lflist *l)
{
    flx pat = {}, patp = {};
    while(1){
        pat = flinref_read(&a.pt->n, (flx*[]){&n, &pat, &patp, NULL}, h, l);

        patp = atomic_readflx(&pat.pt->p);
        if(patp.pt != a.pt){
            flinref_up(patp, h, l);
            flx patpp = atomic_readflx(&pat.pt->p);
            /* patp has been added */
            if(patpp.pt == a.pt){
                if(!patpp.gen.locked)
                    return flinref_down(pat, l), patp;
                else
                    continue;
            }
            /* a has been removed */
            if(atomic_flxeq(&a.pt->n, pat))
                return (flx){};
            else
                continue;
        }
        if(!patp.gen.locked)
            return pat;

        /* try helping pat finish its removal transaction */
        n = flinref_read(&pat.pt->n, (flx*[]){NULL}, h, l);
        flx np = casx((flx){a.pt, n.gen}, &n.pt->p, (flx){pat.pt, n.gen});
        if(np.pt == a.pt || (np.pt == pat.pt && geneq(np.gen, n.gen)))
            return flinref_down(pat, l), n;

        /* unlock pat if it hasn't begun a new transaction */
        flx new = {a.pt, (flgen){patp.gen.i, .locked=1, .unlocking=1}};
        if(casx_ok(new, &pat.pt->p, patp) &&
           atomic_flxeq(&pat.pt->n, n) &&
           casx_ok((flx){a.pt, (flgen){patp.gen.i}}, &pat.pt->n, new))
            return flinref_down(n, l), pat;
    }
}

void lflist_add_before(flx a, flx n, heritage *h, lflist *l){
    assert(a.pt != &l->nil);
    assert(aligned_pow2(l, 16));
    assert(aligned_pow2(a.pt, 16));

    a.pt->n = n;
    flx p = {};
    do{
        p = help_prev(n, p, h, l);
        a.pt->p.pt = p.pt;
        
        assert(p.pt);
        assert(geneq(p.gen, n.gen));
        assert(geneq(a.gen, a.pt->p.gen));

    }while(!casx_ok((flx){a.pt, p.gen}, &n.pt->p, p));

    casx_ok((flx){a.pt, p.gen}, &p.pt->n, n);
    
    flinref_down(p, l);
}

void lflist_add_rear(flx a, heritage *h, lflist *l){
    lflist_add_before(a, (flx){&l->nil}, h, l);
}

flx lflist_pop_front(heritage *h, lflist *l){
    for(flx n;;){
        n = help_next((flx){&l->nil}, n, h, l);
        assert(n.pt);
        if(n.pt == &l->nil)
            return (flx){};
        if(!lflist_remove(n, h, l))
            return n;
    }
}

flanchor *flptr(flx a){
    return a.pt;
}

flx flx_of(flanchor *a){
    return (flx){a, a->p.gen};
}

#endif
