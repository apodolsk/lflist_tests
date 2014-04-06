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
static inline flx atomic_readx(volatile flx *x){
    return cas2((flx){}, x, (flx){});
}
static inline bool atomic_eqx(volatile flx *a, flx *b){
    flx old = *b;
    *b = atomic_readflx(a);
    if(eq2(old, b))
        return true;
    if(flinref_up(*b, t))
        *b = (flx){};
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
        *n = flinref_read(&pt(a)->n, (flx*[]){n, &nn, &np, NULL}, t);
    skip_read:
        if(!pt(*n))
            return assert(!a.mp.is_nil), -1;

        np = atomic_readx(&pt(*n)->p);
        if(pt(np) != pt(a)){
            if(flinref_up(np, t))
                continue;
            flx npp = n->nil ? atomic_readx(&pt(np)->p) : flx(){};
            if(!atomic_eqx(&pt(a)->n, n))
                goto skip_read;
            if(n.nil && pt(npp) == pt(a)){
                casx_ok(np, &pt(a)->n, n)
                
            }
            if(eq2(np, atomic_readx(&pt(*n)->p)))
                return -1;
        }
        if(!np.locked)
            return 0;

flanchor *flptr(flx a){
    assert(!a.mp.is_nil);
    return pt(a);
}

flx flx_of(flanchor *a){
    return (flx){mptr(a, 0), a->p.gen};
}


#endif
