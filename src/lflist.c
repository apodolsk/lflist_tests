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
static flx help_next(flx a, flx n, type *t);
static flx help_prev(flx a, flx p, type *t);

/* #define help_next(a...) lflist_trace(help_next, a) */
/* #define help_prev(a...) lflist_trace(help_prev, a) */

static inline flanchor *pt(flx a){
    return (flanchor *) (a.mp.ptr << 1);
}
static inline flx atomic_readflx(volatile flx *x){
    return cas2((flx){}, x, (flx){});
}
static inline int atomic_flxeq(volatile flx *aptr, flx b){
    flx a = atomic_readflx(aptr);
    return PUN(dptr, a) == PUN(dptr, b);
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
    return eq2(casx(f, l, n, a, e), e);
}

#define casx(as...) casx(__func__, __LINE__, as)
#define casx_ok(as...) casx_ok(__func__, __LINE__, as)
 
static
err (flinref_read)(flx *r, volatile flx *from, flx **held, type *t){
    while(1){
        *r = atomic_readflx(from);
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
        if(!pt(*r))
            return 0;
        if(!flinref_up(*r, t))
            return 0;
    }
}

static
int flinref_up(flx a, type *t){
    if(!pt(a))
        return -1;
    if(a.mp.is_nil)
        return 0;
    return t->linref_up(pt(a), t);
}

static
void flinref_down(flx a, type *t){
    assert(pt(a));
    if(!a.mp.is_nil)
        t->linref_down(pt(a));
}


err (lflist_remove)(flx a, type *t){
    assert(!a.mp.is_nil);

    flx n = {}, p = {};
    while(1){
        if(help_next(a, &n, t)){
            RARITY("N abort");
            break;
        }
        if(help_prev(a, &p, t)){
            RARITY("P abort");
            break;
        }

        flx newn = (flx){n.nil, 1, n.pt, n.gen + 1};
        flx f = cas(newn, &pt(a)->n, n);
        if((eq2(f, n) || eq2(f, newn)) && casx_ok(n, &pt(p)->n, a))
            break;
    }

    int ret = -1;
    if(!pt(n) || p.gen != a.gen || !n.locked)
        RARITY("%s", str(p));
    else if(casx_ok((flx){.gen = a.gen}, &pt(a)->p, p)){
        casx((flx){.mp=p.mp, n.gen}, &pt(n)->p, a);
        ret = 0;
    }

    if(pt(n))
        flinref_down(n, t);
    if(pt(p))
        flinref_down(p, t);
    
    return ret;
}

static
err (help_next)(flx a, flx *n, type *t){
    flx nn = {}, np = {};
    while(1){
        *n = flinref_read(&pt(a)->n, (flx*[]){&n, &nn, &np, NULL}, t);
        if(!pt(*n))
            return assert(!a.mp.is_nil), -1;

        np = atomic_readflx(&pt(*n)->p);
        assert(!np.locked);
        if(pt(np) != pt(a)){
            RARITY("np != a (np:%s)", str(np));
            if(flinref_up(np, t))
                return -1;
            flx npp = atomic_readflx(&pt(np)->p);
            if(!atomic_flxeq(&pt(a)->n, *n))
                continue;
            if(pt(npp) == pt(a)){
                RARITY("np has been added.");
                flx added = (flx){np.mp, npp.gen};
                casx(added, &pt(a)->n, n);
                return flinref_down(n, t), added;
            }
            RARITY("DEAD! npp != a (npp:%s), a:%s", str(npp), str(a));
            flinref_down(n, t), flinref_down(np, t);
            return assert(!a.mp.is_nil), -1;
        }

            nn = flinref_read(&pt(n)->n, (flx*[]){n, NULL}, t);
            if(!pt(nn) || !eq2(atomic_readflx(&pt(a)->n), n))
                break;

            for(int i = 0; i < 2; i++){
                flx e = (flx){nn.mp, nn.gen};
                e.locked = i;
                flxn nnp = casx((flx){a.mp, e.gen}, &pt(nn)->p, e);
                if(pt(nnp) == pt(a)){
                    if(!casx_ok(n, &pt(a)->n, n))
                        RARITY("Helped n by swinging n, but then n died.");
                    n = nn;
                    if(!nnp.locked)
                        return n;
                    continue;
                }
                if(pt(np) != pt(n))
                    break;
            }
            
            
        }
        RARITY("Trying to help n finish its removal.");

    }
}

static
flx (help_prev)(flx a, flx p, type *t){
    flx pn = {};
    while(1){
        p = flinref_read(&pt(a)->p, (flx*[]){&p, &pn, NULL}, t);
        if(p.gen.locked || !pt(p))
            return p;
        if(p.gen.i != a.gen.i)
            return flinref_down(p, t),
                assert(!a.mp.is_nil || p.gen.i != a.gen.i),
                (flx){.gen = p.gen};
        pn = atomic_readflx(&pt(p)->n);
        if(pt(pn) == pt(a))
            return p;
        
        RARITY("pn != a. (pn:%s) ", str(pn));
        if(flinref_up(pn, t)){
            pn = (flx){};
            continue;
        }
        flanchor *pnn = pt(pt(pn)->n);
        if(!atomic_flxeq(&pt(a)->p, p) || !atomic_flxeq(&pt(p)->n, pn))
            continue;
        if(pnn != pt(a)){
            flinref_down(p, t);
            flinref_down(pn, t);
            return assert(!a.mp.is_nil || p.gen.i != a.gen.i),
                (flx){.gen = p.gen};
        }
        if(casx_ok(a, &pt(p)->n, pn))
            break;
    };
    return p;
}

err (lflist_add_before)(flx a, flx n, type *t){
    assert(n.mp.is_nil);
    if(!casx_ok((flx){.gen.i=a.gen.i + 1}, &pt(a)->p, (flx){.gen=a.gen}))
        return RARITY("Spurious add"), -1;
    a.gen.i++;

    flx p = {}, pp = {}, newn;
    while(1){
        p = help_prev(n, p, t);
        if(!pt(p)){
            assert(p.gen.i != n.gen.i);
            RARITY("List was added to beneath us");
            n.gen = p.gen;
            continue;
        }
        n.gen = p.gen;
        assert(!p.gen.locked && !p.gen.unlocking);

        /* Finish up an interrupted add. */
        pp = flinref_read(&pt(p)->p, (flx*[]){&pp, NULL}, t);
        if(!pt(pp))
            continue;
        casx((flx){p.mp, (flgen){pp.gen.i}}, &pt(pp)->n,
             (flx){n.mp, (flgen){n.gen.i - 1}});

        pt(a)->p.mp = p.mp;
        n.gen.i++;
        pt(a)->n = n;
        if(casx_ok((flx){a.mp, n.gen}, &pt(n)->p, p))
            break;
    };

    if(!casx_ok(a, &pt(p)->n, (flx){n.mp, p.gen}))
        RARITY("p helped a add itself");
    
    flinref_down(pp, t);
    flinref_down(p, t);
    return 0;
}

err (lflist_add_rear)(flx a, type *t, lflist *l){
    assert(pt(a) != &l->nil);
    return lflist_add_before(a, (flx){mptr(&l->nil, 1), l->nil.p.gen}, t);
}

flx (lflist_pop_front)(type *t, lflist *l){
    for(flx n = {};;){
        n = help_next((flx){mptr(&l->nil, 1)}, n, t);
        assert(pt(n));
        if(n.mp.is_nil){
            assert(pt(n) == &l->nil);
            return (flx){};
        }
        if(!lflist_remove(n, t)){
            /* Note that no linref_down has been done. */
            return n;
        }
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
