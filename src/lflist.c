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
    llprintf1("CAS! %s:%d - %s if %s, addr:%s", f, l,
              str(n), str(e), str((void *) a)); 
    flx r = cas2(n, a, e);
    llprintf1("%s - %s", eq2(r,e)? "WON" : "LOST", str(r));
    return r;
}
static inline int casx_ok(const char *f, int l,
                          flx n, volatile flx *a, flx e){
    return eq2(casx(f, l, n, a, e), e);
}

#define casx(as...) casx(__func__, __LINE__, as)
#define casx_ok(as...) casx_ok(__func__, __LINE__, as)
 
static
flx (flinref_read)(volatile flx *from, flx **held, type *t){
    while(1){
        flx a = atomic_readflx(from);
        flx *reused = NULL;
        for(flx **h = held; *h; h++){
            if(pt(a) == pt(**h) && !reused)
                reused = *h;
            else if(pt(**h))
                flinref_down(**h, t);
            **h = (flx){};
        }
        if(reused)
            return a;
        if(!pt(a))
            return a;
        if(!flinref_up(a, t))
            return a;
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
        log(*pt(a));
        if(!pt(p = help_prev(a, p, t)) || p.gen.i != a.gen.i){
            RARITY("P abort");
            break;
        }
        if(!pt(n = help_next(a, n, t))){
            RARITY("N abort");
            break;
        }
        if(casx_ok((flx){p.mp, (flgen){p.gen.i, .locked = 1}}, &pt(a)->p, p)
           && casx_ok((flx){p.mp, n.gen}, &pt(n)->p, (flx){a.mp, n.gen}))
            break;
    }

    if(pt(n))
        flinref_down(n, t);
    if(pt(p))
        flinref_down(p, t);
    if((!pt(p) || !pt(n)) && pt(a)->n.gen.locked)
        return -1;
    
    p = pt(a)->p;
    for(uint i = 0; i < 2; i++){
        if(p.gen.i != a.gen.i || !pt(p))
            return -1;
        flx p0 = casx((flx){.gen.i = a.gen.i}, &pt(a)->p, p);
        if(eq2(p, p0)){
            casx(pt(a)->n, &pt(p)->n, a);
            return 0;
        }
        if(!p0.gen.unlocking)
            return -1;
        p = p0;
    }

    return -1;
}

static
/* TODO: could have flx *n. */
flx (help_next)(flx a, flx n, type *t){
    flx pat = {}, patp = {};
    while(1){
        pat = flinref_read(&pt(a)->n, (flx*[]){&n, &pat, &patp, NULL}, t);
        if(!pt(pat))
            return (flx){};
        if(pat.gen.locked)
            return flinref_down(pat, t), (flx){};
        assert(!pat.gen.unlocking);

        patp = atomic_readflx(&pt(pat)->p);
        if(pt(patp) != pt(a)){
            RARITY("patp != a (patp:%s)", str(patp));
            if(flinref_up(patp, t))
                continue;
            flx patpp = atomic_readflx(&pt(patp)->p);
            if(pt(patpp) == pt(a)){
                if(!atomic_flxeq(&pt(a)->n, pat))
                    continue;
                RARITY("patp has been added.");
                if(!patpp.gen.locked){
                    flx added = (flx){patp.mp, patpp.gen};
                    casx(added, &pt(a)->n, pat);
                    return flinref_down(pat, t), added;
                }
                else
                    continue;
            }
            RARITY("patpp != a (patpp:%s)", str(patpp));
            if(atomic_flxeq(&pt(a)->n, pat) ||
               (!a.mp.is_nil && pt(a)->p.gen.i != a.gen.i))
            {
                flinref_down(pat, t), flinref_down(patp, t);
                return (flx){};
            }
            else
                continue;
        }
        if(!patp.gen.locked)
            return pat;

        RARITY("Trying to help pat finish its removal.");
        n = flinref_read(&pt(pat)->n, (flx*[]){NULL}, t);
        if(pt(n) && eq2(atomic_readflx(&pt(a)->n), pat)){
            flx e = (flx){pat.mp, n.gen};
            flx np = casx((flx){a.mp, n.gen}, &pt(n)->p, e);
            if(eq2(np, e) || pt(np) == pt(a)){
                if(!casx_ok(n, &pt(a)->n, pat))
                    RARITY("Helped pat by swinging n, but then n died.");
                return flinref_down(pat, t), n;
            }
        }
        else
            continue;

        RARITY("Unlocking pat if it hasn't begun a new transaction.");
        flx new = {a.mp, (flgen){patp.gen.i, .locked=1, .unlocking=1}};
        if(casx_ok(new, &pt(pat)->p, patp) &&
           atomic_flxeq(&pt(pat)->n, n) &&
           casx_ok((flx){a.mp, (flgen){patp.gen.i}}, &pt(pat)->p, new))
            return flinref_down(n, t), pat;
        RARITY("Failed to unlock.");
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
            return flinref_down(p, t), (flx){.gen = p.gen};
        pn = atomic_readflx(&pt(p)->n);
        if(pt(pn) == pt(a))
            return p;
        RARITY("pn != a. (pn:%s) A died, p died, or something was added.",
               str(pn));
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
            return (flx){.gen = p.gen};
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
        casx((flx){p.mp, (flgen){.i=pp.gen.i}}, &pt(pp)->n,
             (flx){n.mp, (flgen){.i=n.gen.i - 1}});

        n.gen.i++;
        pt(a)->p.mp = p.mp;
        pt(a)->n = (flx){n.mp, (flgen){n.gen.i, .locked = 1}};
        if(casx_ok((flx){a.mp, n.gen}, &pt(n)->p, p))
            break;
        
    };

    if(!casx_ok(n, &pt(a)->n, (flx){n.mp, (flgen){.i=n.gen.i, .locked = 1}}))
        RARITY("Some node was added to the right.");
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
        log(l);
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
