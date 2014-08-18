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

#define LIST_CHECK_FREQ 5
#define FLANC_CHECK_FREQ 40

#define MAX_LOOP 0
#define TEST_CONTENTION(c)                                                 \
    ({ if(MAX_LOOP && c++ > MAX_LOOP) SUPER_RARITY("LOTTA LOOPS %", c); })

static flx flinref_read(volatile flx *from, flx **held, type *t);
static int flinref_up(flx *a, type *t);
static void flinref_down(flx *a, type *t);
static err help_next(flx a, flx *n, flx *np, type *t);
static err help_prev(flx a, flx *p, flx *pn, type *t);
static bool flanchor_valid(flx ax);

#define flinref_up(as...) trace(LFLISTM, 5, flinref_up, as)
#define flinref_down(as...) trace(LFLISTM, 5, flinref_down, as)
#define help_next(as...) trace(LFLISTM, 3, help_next, as)
#define help_prev(as...) trace(LFLISTM, 3, help_prev, as)
#define flinref_read(as...) trace(LFLISTM, 4, flinref_read, as)


static
flanchor *pt(flx a){
    return (flanchor *) (uptr)(a.pt << 3);
}
static
flx casx(const char *f, int l, flx n, volatile flx *a, flx *e){
    assert(n.nil || (a != &pt(n)->p && a != &pt(n)->n));
    assert(!eq2(n, *e));
    log(2, "CAS! %:% - % if %, addr:%", f, l, n, *e, a);
    flx oe = *e;
    *e = cas2(n, a, oe);
    log(2, "% %:%- found:% addr:%", eq2(*e, oe)? "WON" : "LOST", f, l, *e, a);
    assert(!pt(n) || flanchor_valid(n));
    return *e;
}

static howok updx_ok(const char *f, int l, flx n, volatile flx *a, flx *e){
    flx oe = *e;
    casx(f, l, n, a, e);
    if(eq2(*e, oe))
        return *e = n, WON;
    if(eq2(*e, n))
        return OK;
    return NOT;
}

static bool updx_won(const char *f, int l,
                    flx n, volatile flx *a, flx *e){
    flx oe = *e;
    return eq2(oe, casx(f, l, n, a, e));
}

#define casx(as...) casx(__func__, __LINE__, as)
#define updx_ok(as...) updx_ok(__func__, __LINE__, as)
#define updx_won(as...) updx_won(__func__, __LINE__, as)

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
    for(int c = 0;; TEST_CONTENTION(c)){
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
    for(int c = 0;; TEST_CONTENTION(c)){
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
    assert(pt(*a));
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

    howok pn_ok = NOT;
    bool del_won = false;
    flx pn = {}, np = {}, p = {};
    if(help_prev(a, &p, &pn, t))
        goto cleanup;
    flx n = flinref_read(&pt(a)->n, (flx*[]){NULL}, t);
    for(int c = 0;; TEST_CONTENTION(c)){
        if(help_next(a, &n, &np, t)){
            RARITY("n abort");
            break;
        }
        if(help_prev(a, &p, &pn, t)){
            RARITY("p abort");
            assert(pt(np) == pt(a));
            break;
        }
        assert(!pn.locked);

        flx oldn = n;
        howok lock_ok = updx_ok((flx){.nil=n.nil,1,0,n.pt, n.gen + 1},
                                &pt(a)->n, &n);
        if(!lock_ok)
            continue;
        assert(!del_won || oldn.helped || oldn.locked);
        del_won |= lock_ok == WON && !oldn.helped && !oldn.locked;

        pn_ok = updx_ok((flx){.nil=n.nil, 0, pn.helped, n.pt, pn.gen+1},
                        &pt(p)->n, &pn);
        if(pn_ok)
            break;
    }

    if(!del_won)
        goto cleanup;

    if(pn_ok == WON)
        assert(pt(pn) == pt(pt(a)->n) &&
               eq2(pt(a)->p, p) &&
               pt(a)->n.pt == n.pt);

    ppl(2, np, a, pn_ok);
    if(pt(np) != pt(a))
        goto np_done;
    assert(pt(p) && !p.locked && p.gen == a.gen && eq2(pt(a)->p, p));
    if(!pn_ok)
       if(!pt(n = flinref_read(&pt(a)->n, ((flx*[]){&n, NULL}), t))
          || pt(a) != pt(np = readx(&pt(n)->p))){
        goto np_done;
       }
    if(!updx_won((flx){.nil=p.nil, .pt=p.pt, .gen=np.gen}, &pt(n)->p, &np))
        goto np_done;
    
    if(!n.nil){
        /* Clean up after an interrupted add of 'n'. In this case,
           a->n is the only reference to 'n' discoverable from nil,
           and we should finish the add before it gets cleared (next
           time a is added). */
        flx nn = readx(&pt(n)->n);
        ppl(2, nn);
        if(nn.nil){
            flx nnp = readx(&pt(nn)->p);
            ppl(2, nnp);
            if(pt(nnp) == pt(a)){
                assert(!n.nil);
                casx((flx){.pt=n.pt, nnp.gen + 1}, &pt(nn)->p, &nnp);
            }
        }
    }

np_done:
    assert(pt(n) && (n.locked || n.helped) && pt(pt(a)->n) == pt(n));

    pt(a)->n = (flx){.gen = n.gen};
    pt(a)->p = (flx){.gen = a.gen};

cleanup:
    if(pt(n))
        flinref_down(&n, t);
    return -!del_won;
}

static
err (help_next)(flx a, flx *n, flx *np, type *t){
    flx oldn = (flx){}, oldnp = (flx){};
    for(int c = 0;;){
    newn:
        ppl(2, *n);
        assert(!a.nil || (!n->locked && !n->helped && pt(*n)));
        if(!pt(*n))
            return -1;
        *np = readx(&pt(*n)->p);
    newnp:
        ppl(2, *np);
        TEST_CONTENTION(c);
        if(!eqx(&pt(a)->n, n, t))
            goto newn;
        if(pt(*np) == pt(a))
            return 0;
        if(n->locked)
            return -1;
        if(!a.nil && n->nil && pt(a)->p.gen != a.gen)
            return -1;
        assert(pt(*np) && !np->locked);
        assert(!eq2(oldn, *n) || !eq2(oldnp, *np));
        oldn = *n;
        oldnp = *np;

        if(updx_ok((flx){a.mp, np->gen + (n->nil ? 1 : 0)}, &pt(*n)->p, np))
            return 0;
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
        ppl(2, *p);
        assert(a.nil || (pt(*p) != pt(a)));
        assert((!a.nil && pt(*p) != pt(a)) ||
               ((p->nil && a.nil) ^ (pt(a) != pt(*p))));
        if(!a.nil && (!pt(*p) || p->locked || p->gen != a.gen))
            return -1;
        *pn = readx(&pt(*p)->n);
    newpn:
        ppl(2, *pn);
        if(!eqx(&pt(a)->p, p, t))
            goto newp;
        if(pt(*pn) != pt(a)){
            if(!a.nil)
                return -1;
            assert(!pn->nil && pt(*pn));
            if(!updx_ok((flx){.pt=pn->pt, .gen=p->gen + 1}, &pt(a)->p, p))
                continue;
            goto newp;
        }
        if(!pn->locked)
            return 0;
        
        pp = ppl(2, flinref_read(&pt(*p)->p, ((flx*[]){&pp, NULL}), t));
        if(!pt(pp))
            continue;
        flx ppn = ppl(2, readx(&pt(pp)->n));
        if(pt(ppn) != pt(*p) && pt(ppn) != pt(a))
            continue;
        if(!eqx(&pt(a)->p, p, t))
            goto newp;
        
        if(pt(ppn) == pt(a) ||
           (!ppn.locked
            && (updx_ok((flx){.nil=a.nil, 1, 1, a.pt, pn->gen + 1},
                        &pt(*p)->n, pn) ?: ({goto newpn;0;}))
            && updx_ok((flx){.nil=a.nil, 0, ppn.helped, a.pt, ppn.gen + 1},
                       &pt(pp)->n, &ppn)))
        {
            /* TODO flinref... */
            if(!updx_ok((flx){.nil=pp.nil, .pt=pp.pt, p->gen}, &pt(a)->p, p))
                goto newp;
            flinref_down(p, t);
            *pn = ppn;
            goto newpn;
        }
        if(!eqx(&pt(a)->p, p, t))
            goto newp;
        

        if(updx_ok((flx){.nil=a.nil, 0, 1, a.pt, pn->gen + 1}, &pt(*p)->n, pn))
            return 0;
        goto newpn;
    }
}

err (lflist_enq)(flx a, type *t, lflist *l){
    if(!updx_won((flx){.locked=1, .gen=a.gen + 1}, &pt(a)->p,
                 &(flx){.gen=a.gen}))
        return -1;
    assert(!pt(a)->n.mp);
    pt(a)->n = (flx){.nil=1, .pt=mpt(&l->nil), .gen = pt(a)->n.gen + 1};
    
    flx p = {}, pn = {}, oldp = {}, oldpn = {};
    for(int c = 0;;TEST_CONTENTION(c)){
        if(help_prev(((flx){.nil = 1, .pt = mpt(&l->nil)}), &p, &pn, t))
            EWTF();
        assert(!p.locked && !p.helped && !pn.locked
               && pt(pn) && pn.nil && pt(p) != pt(a) &&
               (!eq2(oldp, p) || !eq2(oldpn, pn)));
        oldp = p;
        oldpn = pn;

        pt(a)->p.mp = p.mp;
        if(updx_won((flx){.helped=pn.helped, .pt=a.pt, pn.gen + 1},
                    &pt(p)->n, &pn))
            break;
    }
    casx((flx){a.mp, .gen=p.gen + 1}, &l->nil.p, (flx[]){p});
    flinref_down(&p, t);
    return 0;
}

flx (lflist_deq)(type *t, lflist *l){
    flx np, oldn = {}, n = {};
    for(int c = 0;;TEST_CONTENTION(c)){
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

static bool _flanchor_valid(flx ax, flx *retn, lflist **on);

bool lflist_valid(flx a){
    if(!randpcnt(LIST_CHECK_FREQ) || pause_universe())
        return true;
    lflist *on = NULL;
    for(flx c = a;;){
        assert(_flanchor_valid(c, &c, &on));
        if(!pt(c) || pt(c) == pt(a))
            break;
    }
    assert(on);
    resume_universe();
    return true;
}

static 
bool flanchor_valid(flx ax){
    if(!randpcnt(FLANC_CHECK_FREQ) || pause_universe())
        return true;
    assert(_flanchor_valid(ax, NULL, NULL));
    resume_universe();
    return true;
}

static
bool _flanchor_valid(flx ax, flx *retn, lflist **on){
    flanchor *a = pt(ax);
    assert(a);
    flx px = a->p, nx = a->n;
    flanchor *p = pt(px), *n = pt(nx);
    if(retn) *retn = nx;

    if(!p || !n){
        assert(!ax.nil);
        assert((eq2(nx,(flx){.gen=nx.gen}) && !p && !px.helped)
               || (!p && n && pt(n->p) != a
                   && (nx.locked || (nx.nil && px.locked))
                   && (!pt(n->n) || pt(pt(n->n)->p) != a))
               || (!n && p && pt(p->n) != a && !px.locked));
        if(retn) *retn = (flx){};
        return true;
    }

    flanchor *pn = pt(p->n), *pp = pt(p->p), *np = pt(n->p), *nn= pt(n->n);

    if(ax.nil){
        assert(!on || !*on || *on == cof(a, lflist, nil));
        if(on)
            *on = cof(a, lflist, nil);
        assert(p && n && pn && np && pp && nn);
        assert(!px.locked && !px.helped && !nx.locked && !px.helped);
        assert(np == a
               || (pt(np->p) == a && pt(np->n) == n && np->n.locked));
        assert(pn == a
               || (!pn->n.locked && pt(pn->n) == a && pt(pn->p) == p));
    }else{
        assert(!on || *on != cof(a, lflist, nil));
        assert(p != a && n != a);
        assert(nx.nil || n != p);
        if(!nx.locked)
            assert((pn == a && np == a) ||
                   (pn == a && pt(np->p) == a && pt(np->n) == n) ||
                   (nx.nil && ((pn == n && np == p)
                               || (pn == a && np == p)
                               || (pn != n && np != p)
                               || (pn != n && pt(pn->n) == n && pt(pn->p) == p))));
        else
            assert((pn == a && np == a) ||
                   (pn == n && np == a) ||
                   (pn == n && np == p));
    }


    return true;
}


#endif
