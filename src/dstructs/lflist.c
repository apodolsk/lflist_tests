/**
 * A lockfree double linked list supporting queue operations and arbitrary
 * deletion. It only allows addition at the end of the list. Any lflist
 * anchor ("nodes") A can be immediately reused after a deletion, even if
 * other threads have yet to detect that A has died ("been deleted").
 *
 * lflists are structured like regular dllists with a dummy node, except
 * n/p pointers have associated generation counters and status bits to
 * allow functions on anchors ("nodes") adjacent to A to detect and help
 * conflicting functions on A. See lflist.h for details on each one.
 *
 * Deletion is the hard part. To avoid traversing chains of deleted
 * anchors, and to thus allow their immediate reuse, the lflist maintains
 * the invariant that there's at most one dead anchor between any two live
 * ones. To delete A, T1 finds P and N such that P == pt(A->p), pt(P->n)
 * == A, N == pt(A->n) and pt(N->p) == A (this isn't trivial). Then T1
 * marks A->n.locked = true and then writes P->n = N and N->p = P. If T2
 * is deleting N, it sees A->n.locked and attempts to complete all of T1's
 * aforementioned writes. It's important that T2 will succeed iff T1 will
 * succeed.
 *
 * Fuck this.
 *
 * Alex Podolsky <apodolsk@andrew.cmu.edu>
 */

#define MODULE LFLISTM

#include <atomics.h>
#include <lflist.h>
#include <nalloc.h>

#ifndef FAKELOCKFREE

#define MAX_LOOP 0
#define TEST_PROGRESS(c)                                                 \
    ({ if(MAX_LOOP && c++ > MAX_LOOP) SUPER_RARITY("LOTTA LOOPS %", c); })

static flx flinref_read(volatile flx *from, flx **held, type *t);
static int flinref_up(flx *a, type *t);
static void flinref_down(flx *a, type *t);
static err help_next(flx a, flx *n, flx *np, type *t);
static err help_prev(flx a, flx *p, flx *pn, type *t);

#define flinref_up(as...) trace(LFLISTM, 5, flinref_up, as)
#define flinref_down(as...) trace(LFLISTM, 5, flinref_down, as)
#define help_next(as...) trace(LFLISTM, 3, help_next, as)
#define help_prev(as...) trace(LFLISTM, 3, help_prev, as)
#define flinref_read(as...) trace(LFLISTM, 4, flinref_read, as)


static
flanchor *pt(flx a){
    return (flanchor *) (a.pt << 3);
}
static
flx casx(const char *f, int l, flx n, volatile flx *a, flx *e){
    log(2, "CAS! %:% - % if %, addr:%", f, l, n, *e, a);
    flx oe = *e;
    *e = cas2(n, a, oe);
    log(2, "% %:%- found:% addr:%", eq2(*e, oe)? "WON" : "LOST", f, l, *e, a);
    return *e;
}

static howok casx_ok(const char *f, int l, flx n, volatile flx *a, flx *e){
    flx oe = *e;
    casx(f, l, n, a, e);
    if(eq2(*e, oe))
        return WON;
    if(eq2(*e, n))
        return OK;
    return NOT;
}

static bool casx_won(const char *f, int l,
                    flx n, volatile flx *a, flx *e){
    flx oe = *e;
    return eq2(oe, casx(f, l, n, a, e));
}

#define casx(as...) casx(__func__, __LINE__, as)
#define casx_ok(as...) casx_ok(__func__, __LINE__, as)
#define casx_won(as...) casx_won(__func__, __LINE__, as)

static flx readx(volatile flx *x){
    /* assert(aligned_pow2(x, sizeof(dptr))); */
    /* flx r; */
    /* r.gen = atomic_read(&x->gen); */
    /* r.mp = atomic_read(&x->mp); */
    /* pp(1, r); */
    /* return r; */
    return cas2((flx){}, x, (flx){});
}
static bool eqx(volatile flx *a, flx *b, type *t){
    flx old = *b;
    for(int c = 0;; TEST_PROGRESS(c)){
        *b = readx(a);
        if(eq2(old, *b))
            return true;
        if(pt(old) == pt(*b))
            return false;
        if(!pt(*b) || !flinref_up(b, t)){
            flinref_down(&old, t);
            return false;
        }
    }
}

static
flx (flinref_read)(volatile flx *from, flx **held, type *t){
    flx old = (flx){};
    for(int c = 0;; TEST_PROGRESS(c)){
        flx a = readx(from);
        flx *reused = NULL;
        for(; *held; held++){
            if(!reused && pt(a) == pt(**held))
                reused = *held;
            else if(pt(**held))
                flinref_down(*held, t);
            **held = (flx){};
        }
        if(eq2(old, a))
            return (flx){};
        if(reused || !pt(a) || !flinref_up(&a, t))
            return a;
        old = a;
    }
}

static
err (flinref_up)(flx *a, type *t){
    if(a->nil || !t->linref_up(pt(*a), t))
        return 0;
    *a = (flx){};
    return -1;
}

static
void (flinref_down)(flx *a, type *t){
    if(!a->nil)
        t->linref_down(pt(*a));
    *a = (flx){};
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
            n = flinref_read(&pt(a)->n, ((flx*[]){&n, NULL}),t);
            break;
        }
        assert(!pn.locked);

        lock = (flx){.nil=n.nil, 1, 0, n.pt, n.gen + 1};
        howok r = casx_ok(lock, &pt(a)->n, &n);
        if(r != NOT){
            n = lock;            
            if(r != WON) lock = (flx){};
            if(casx_ok((flx){.nil=n.nil,0,0,n.pt,pn.gen+1}, &pt(p)->n, &pn))
                break;
        }else{
            if(pt(n) != pt(lock)){
                flinref_down(&lock, t);
                if(flinref_up(&n, t))
                    n = flinref_read(&pt(a)->n, (flx*[]){NULL}, t);
            }else
                lock = (flx){};
        }
    }

    if(pt(p))
        flinref_down((flx[]){p}, t);

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
        if(n.helped && pt(n = flinref_read(&pt(a)->n, ((flx*[]){&n, NULL}),t)))
            np = readx(&pt(n)->p);
        if(pt(np) == pt(a)){
            casx((flx){p.mp, np.gen}, &pt(n)->p, &np);
            /* Clean up after an interrupted add of 'n'. In this case,
               a->n is the only reference to 'n' discoverable from nil,
               and we should finish the add before it gets cleared (next
               time a is added). */
            flx nn = readx(&pt(n)->n);
            if(nn.nil){
                flx nnp = readx(&pt(nn)->p);
                if(pt(nnp) == pt(a)){
                    assert(!n.nil);
                    casx((flx){.pt=n.pt, nnp.gen + 1}, &pt(nn)->p, &nnp);
                }
            }
        }

        pt(a)->p = (flx){.gen = a.gen};
        e = 0;
    }
    else
        RARITY("p:%, n:%, l:%", p, n, lock);

cleanup:
    if(pt(n))
        flinref_down(&n, t);
    return e;
}

static
err (help_next)(flx a, flx *n, flx *np, type *t){
    flx oldn = (flx){}, oldnp = (flx){};
    for(int c = 0;;){
    newn:
        assert(!a.nil || pt(*n));
        assert((!a.nil && pt(*n) != pt(a)) ||
               ((n->nil && a.nil) ^ (pt(a) != pt(*n))));
        if(!pt(*n))
            return -1;
        *np = readx(&pt(*n)->p);
    newnp:
        TEST_PROGRESS(c);
        if(pt(*np) == pt(a))
            return 0;
        /* This is subtle. help_next handles interrupted adds of a and
           interrupted deletes of n with the same write. For an
           interrupted add, !n->locked is always true. */
        if(n->locked){
            if(!pt(*np) || np->locked || np->nil){
                if(!eqx(&pt(a)->n, n, t))
                    goto newn;
                else return assert(!a.nil), -1;
            }
            
            flx npp = readx(&pt(*np)->p);
            ppl(2, *n, *np, npp);
            if(!eqx(&pt(a)->n, n, t))
                goto newn;
            if(pt(npp) != pt(a)){
                flx onp = *np;
                if(!eq2(onp, *np = readx(&pt(*n)->p)))
                    goto newnp;
                else return assert(!a.nil), -1;
            }
        } else{
            if(!eqx(&pt(a)->n, n, t))
                goto newn;
            if(!a.nil && pt(a)->p.gen != a.gen)
                return -1;
            assert(pt(*np) && !np->locked);
        }
        assert(!eq2(oldn, *n) || !eq2(oldnp, *np));
        oldn = *n;
        oldnp = *np;

        RARITY("Swinging n:% np:%", *n, *np);

        flx newnp = {a.mp, np->gen + (n->nil ? 1 : 0)};
        if(casx_ok(newnp, &pt(*n)->p, np))
            return *np = newnp, 0;
        goto newnp;
        /* *n = flinref_read(&pt(a)->n, ((flx*[]){n, NULL}), t); */
    }
}

static
err (help_prev)(flx a, flx *p, flx *pn, type *t){
    flx pp = {};
    for(int c = 0;;){
        *p = flinref_read(&pt(a)->p, ((flx*[]){p, &pp, NULL}), t);
    newp:
        assert(a.nil || (pt(*p) != pt(a)));
        assert((!a.nil && pt(*p) != pt(a)) ||
               ((p->nil && a.nil) ^ (pt(a) != pt(*p))));
        if(!pt(*p) || p->locked || (!a.nil && p->gen != a.gen))
            return *pn = (flx){}, pt(pp) ? flinref_down(&pp, t):0, -1;
        *pn = readx(&pt(*p)->n);
    newpn:
        /* This has to be before the *pn == a check to ensure that
           a->p.gen == a.gen for non-nil nodes. */
        if(!eqx(&pt(a)->p, p, t))
            goto newp;
        if(pt(*pn) != pt(a))
            return pt(pp) ? flinref_down(&pp, t):0, -1;
        TEST_PROGRESS(c);
        if(!pn->locked)
            return pt(pp) ? flinref_down(&pp, t):0, 0;
        
        pp = flinref_read(&pt(*p)->p, ((flx*[]){&pp, NULL}), t);
        if(!pt(pp))
            continue;
        flx ppn = readx(&pt(pp)->n);
        if(pt(ppn) != pt(*p) && pt(ppn) != pt(a))
            continue;
        if(!eqx(&pt(a)->p, p, t)){
            flinref_down(&pp, t);
            goto newp;
        }
        assert(!ppn.nil || !ppn.locked);
        
        if(pt(ppn) == pt(a) ||
           (!ppn.locked &&
            (casx_ok((flx){.nil=a.nil, 1, 1, a.pt, pn->gen + 1}, &pt(*p)->n, pn)
             ?: ({goto newpn;0;}))
            && casx_ok((flx){a.mp, ppn.gen + 1}, &pt(pp)->n, &ppn)))
        {
            flx oldp = *p, newp = (flx){.nil=pp.nil, .pt=pp.pt, p->gen};
            if(!casx_ok(newp, &pt(a)->p, p)){
                if(pt(*p) != pt(oldp)){
                    flinref_down(&oldp, t);
                    if(flinref_up(p, t))
                        continue;
                }
                goto newp;
            }
            flinref_down(&oldp, t);
            *p = newp;
            if(pt(ppn) != pt(a)){
                *pn = (flx){a.mp, ppn.gen + 1};
                return 0;
            }
            pp = (flx){};
            *pn = ppn;
            goto newpn;
        }
        if(!eqx(&pt(a)->p, p, t))
            goto newp;

        ppl(2, a, p, pp, ppn);
        flx newpn = (flx){.nil=a.nil, 0, 0, a.pt, pn->gen + 1};
        if(casx_ok(newpn, &pt(*p)->n, pn))
            return *pn = newpn, flinref_down(&pp, t), 0;
    }
}

err (lflist_enq)(flx a, type *t, lflist *l){
    if(!casx_won((flx){.gen = a.gen + 1}, &pt(a)->p, &(flx){.gen = a.gen}))
        return -1;
    pt(a)->n = (flx){.nil=1, .pt=mpt(&l->nil), .gen = pt(a)->n.gen + 1};

    flx p = {}, pn = {}, oldp = {}, oldpn = {};
    for(int c = 0;;TEST_PROGRESS(c)){
        if(help_prev(((flx){.nil = 1, .pt = mpt(&l->nil)}), &p, &pn, t)){
            assert(pt(p));
            assert(pt(pn));
            flinref_down((flx[]){p}, t);
            casx((flx){.pt = pn.pt, p.gen + 1}, &l->nil.p, &p);
            p = (flx){};
            continue;
        }
        assert(!pn.locked);
        assert(pn.nil);
        assert(!eq2(oldpn, pn) || !eq2(oldp, p));
        oldp = p;
        oldpn = pn;
        ppl(2, p, pn);
        
        pt(a)->p.mp = p.mp;
        if(casx_won((flx){.mp = a.mp, pn.gen + 1}, &pt(p)->n, &pn))
            break;
    }
    flinref_down((flx[]){p}, t);
    casx((flx){.mp = a.mp, .gen = p.gen + 1}, &l->nil.p, &p);
    return 0;
}

flx (lflist_deq)(type *t, lflist *l){
    flx np, oldn = {}, n = {};
    for(;;){
        n = flinref_read(&l->nil.n, ((flx*[]){&n, NULL}), t);
        if(help_next(((flx){.nil = 1, .pt = mpt(&l->nil)}), &n, &np, t))
            EWTF();
        if(n.nil)
            return assert(&l->nil == pt(n)), (flx){};
        assert(!eq2(oldn, n));
        if(!lflist_del(((flx){n.mp, np.gen}), t))
            return (flx){n.mp, np.gen};
        oldn = n;
    }
}

flx (lflist_next)(flx p, lflist *l){
    flx r, n;
    do{
        n = readx(&pt(p)->n);
        if(n.nil)
            return (flx){};
        r = (flx){.pt = n.pt, pt(n)->p.gen};
        if(pt(p)->p.gen != p.gen)
            return lflist_peek(l);
    }while(atomic_read(&pt(p)->n.gen) != n.gen);
    return r;
}

flx (lflist_peek)(lflist *l){
    flx n = l->nil.n;
    return n.nil ? (flx){} : (flx){.pt = n.pt, pt(n)->p.gen};
}

flanchor *flptr(flx a){
    assert(!a.nil);
    return pt(a);
}

flx flx_of(flanchor *a){
    return (flx){.pt = mpt(a), a->p.gen};
}


#endif
