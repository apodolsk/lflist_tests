/**
 * A lockfree double linked list supporting queue operations and arbitrary
 * deletion. It only allows addition at the end of the list.
 *
 * Nodes ("flanchors") and node references (double-word tagged pointers
 * via "flx") have an associated generation incremented upon enq():
 * 
 * for flx N, flx N', flanchor pN, pN':
 * flptr(N)==flptr(N') && N'.gen == N.gen + 1 
 * && !enq(N)==0 && del(N')!=0, then flx_of(flptr(N)).gen ==
 * N'.gen.
 *
 * flx A can be immediately reused after del(A)==0 and del(A') or enq(A')
 * will promptly detect this, for all A'. If flptr(A) is a lineage,
 * linfree(flptr(A)) will result in a 
 *
 * Multiple calls to del(A) will cooperate, but only one wins:
 * 
 * del(A)
 *
 * lflist structure and operation is much like that of a regular list with
 * a dummy node. del(A) sets (N:=A->n)->p = P and (P:=A->p)->p = A and enq(A) 
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

static inline
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

static howok updx_ok_modhelped(const char *f, int l, flx n,
                               volatile flx *a, flx *e){
    flx oe = *e;
    casx(f, l, n, a, e);
    if(eq2(*e, oe))
        return *e = n, WON;
    if(eq2(*e, n))
        return OK;
    n.helped = oe.helped = 0;
    if(eq2(*e, oe)){
        *e = oe;
        return updx_ok(f, l, n, a, e);
    }
    return NOT;
}

static bool updx_won(const char *f, int l,
                    flx n, volatile flx *a, flx *e){
    return WON == updx_ok(f, l, n, a, e);
}

static void countloops(cnt loops){
    if(MAX_LOOP && loops > MAX_LOOP)
        SUPER_RARITY("LOTTA LOOPS: %", loops);
}

static void progress(flx *o, flx n, cnt loops){
    assert(!pt(*o) || !eq2(*o, n));
    *o = n;
    countloops(loops);
}
#define progress(o, n, loops) progress(o, ppl(2, n), loops)

#define casx(as...) casx(__func__, __LINE__, as)
#define updx_ok(as...) updx_ok(__func__, __LINE__, as)
#define updx_ok_modhelped(as...) updx_ok_modhelped(__func__, __LINE__, as)
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
    *b = readx(a);
    if(pt(old) != pt(*b)){
        flinref_down((flx[]){old}, t);
        if(pt(*b)){
            flinref_up(b, t);
            assert(!eq2(*b, old));
        }
    }
    return eq2(old, *b);
}

static
flx (flinref_read)(volatile flx *from, flx **held, type *t){
    flx old = (flx){};
    for(int c = 0;; countloops(c)){
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
            return pp(old, a), (flx){};
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
    flx pn = {}, p = {};
    if(help_prev(a, &p, &pn, t))
        goto cleanup;
    flx np, n = flinref_read(&pt(a)->n, (flx*[]){NULL}, t);
    for(int lps = 0;;){
        if(help_next(a, &n, &np, t))
            break;
        if(help_prev(a, &p, &pn, t))
            break;
        assert(pt(pn) == pt(a) && pt(np) == pt(a) && !pn.locked);

        flx on = n;
        howok lock_ok = updx_ok((flx){.nil=n.nil,1,0,n.pt, n.gen + 1},
                                &pt(a)->n, &n);
        if(!lock_ok)
            continue;
        assert(!del_won || on.helped || on.locked);
        del_won |= lock_ok == WON && !on.helped && !on.locked;

        pn_ok = updx_ok((flx){.nil=n.nil, 0, pn.helped, n.pt, pn.gen+1},
                        &pt(p)->n, &pn);
        if(pn_ok)
            break;
    }
    if(!del_won)
        goto cleanup;

    static cnt naborts;
    static cnt paborts;
    static cnt wins;
    if(pn_ok) xadd(1, &wins);
    else if(pt(np) == pt(a)) xadd(1, &paborts);
    else if(pt(np) != pt(a)) xadd(1, &naborts);

    if(pn_ok || pt(np) == pt(a))
        assert(eq2(p, pt(a)->p));
    if(pn_ok || pt(np) != pt(a))
        assert(n.pt == pt(a)->n.pt);
    
    /* Must be p abort */
    if(!pn_ok && pt(np) == pt(a)){
        n = flinref_read(&pt(a)->n, ((flx* []){&n, NULL}), t);
        np = readx(&pt(n)->p);
    }

    flx onp = np;
    if(pt(np) == pt(a))
        updx_ok_modhelped((flx){.nil=p.nil, 0, np.helped, p.pt, np.gen + n.nil},
                          &pt(n)->p, &np);

    /* Clean up after an interrupted add of 'n'. In this case,
       a->n is the only reference to 'n' discoverable from nil,
       and we should finish the add before it gets cleared (next
       time a is added). */
    ppl(2, n, np, a, pn_ok);
    if(np.helped && onp.gen == np.gen && pt(np)){
        assert(!n.nil);
        flx nn = readx(&pt(n)->n);
        if(nn.nil && !nn.locked){
            flx nnp = readx(&pt(nn)->p);
            if(pt(nnp) == pt(a))
                casx((flx){.pt=n.pt, nnp.gen + 1}, &pt(nn)->p, &nnp);
        }
    }

    assert(pt(a)->n.locked &&
           pt(a)->p.gen == a.gen &&
           (pt(np) != pt(a) || eq2(p, pt(a)->p)) &&
           n.pt == pt(a)->n.pt);

    pt(a)->n = (flx){.gen = n.gen};
    pt(a)->p = (flx){.gen = a.gen};

    assert(flanchor_valid(n));

cleanup:
    if(pt(n))
        flinref_down(&n, t);
    if(pt(p))
        flinref_down(&n, t);
    return -!del_won;
}

static
err (help_next)(flx a, flx *n, flx *np, type *t){
    flx on = {}, onp = {};
    for(cnt lps = 0;; progress(&on, *n, lps++)){
        if(!pt(*n))
            return -1;
        for(*np = readx(&pt(*n)->p);; progress(&onp, *np, lps++)){
            if(pt(*np) == pt(a))
                return 0;
            if(!eqx(&pt(a)->n, n, t))
                break;
            if(n->locked)
                return -1;
            if(!a.nil && n->nil && pt(a)->p.gen != a.gen)
                return -1;
            assert(pt(*np) && !np->locked);

            if(updx_ok_modhelped(
                   (flx){.nil=a.nil, 0, np->helped, a.pt, np->gen + n->nil}, &
                   pt(*n)->p, np))
                return 0;
        }
    }
}

static
err (help_prev)(flx a, flx *p, flx *pn, type *t){
    flx op = {}, opn = {}, opp = {}, oppn = {};
    for(cnt lps = 0;; ppl(2, *p)){
        /* progress(&op, *p, lps++) */
        for(;; ppl(2, *pn)){
            /* progress(&opn, *pn, lps++); */
            if(!eqx(&pt(a)->p, p, t))
                break;
            if(pt(*pn) != pt(a)){
                if(!a.nil)
                    return -1;
                assert(pt(pt(*pn)->n) == pt(a) || !eq2(pt(a)->p, p));
                updx_ok((flx){.pt=pn->pt, .gen=p->gen + 1}, &pt(a)->p, p);
                break;
            }
            if(p->helped && !updx_ok((flx){.nil=p->nil,.pt=p->pt, a.gen},
                                     &pt(a)->p, p))
            goto newp;

            if(!pn->locked)
                return 0;

            flx pp = flinref_read(&pt(*p)->p, ((flx*[]){&pp, NULL}), t);
            if(!pt(pp)){
                must(!eqx(&pt(a)->p, p, t));
                goto newp;
            }
            for(flx ppn = readx(&pt(pp)->n);;progress(&oppn, ppn, lps++)){
                if(!eqx(&pt(a)->p, p, t))
                    goto newp;
                if(pt(ppn) != pt(*p) && pt(ppn) != pt(a))
                    break;
                if(pt(ppn) == pt(a)){
                    if(!updx_ok((flx){.nil=pp.nil, .pt=pp.pt, p->gen},
                                &pt(a)->p, p))
                        goto newp;
                    *pn = ppn;
                    break;
                }
                if(!updx_ok((flx){.nil=a.nil, !ppn.locked, 1, a.pt, pn->gen + 1},
                            &pt(*p)->n, pn))
                    break;
                if(!pn->locked){
                    assert((eq2(pt(a)->p, *p) && pt(pp)->n.pt == p->pt)
                           || !eq2(pt(*p)->n, *pn)
                           || !eq2(pt(*p)->p, pp));
                    return 0;
                }

                if(updx_ok((flx){.nil=a.nil, 0, ppn.helped, a.pt, ppn.gen + 1},
                           &pt(pp)->n, &ppn)){
                    assert(eq2(pt(a)->p, *p)
                           || pt(a)->p.pt == pp.pt
                           || !eq2(pt(pp)->n, ppn));
                    assert(pt(*p)->n.locked
                           || !pt(*p)->n.pt
                           || !eq2(pp.gen, pt(*p)->p.gen));
                }
            }
        }
    newp:;
        if(!a.nil && (!pt(*p) || p->locked || p->gen != a.gen))
            return -1;
        assert(a.nil || pt(*p) != pt(a));
        *pn = readx(&pt(*p)->n);
    }
}

err (lflist_enq)(flx a, type *t, lflist *l){
    if(!updx_won((flx){.locked=1, .gen=a.gen + 1}, &pt(a)->p,
                 &(flx){.gen=a.gen}))
        return -1;
    assert(!pt(a)->n.mp);
    pt(a)->n = (flx){.nil=1, .pt=mpt(&l->nil), .gen = pt(a)->n.gen + 1};

    if(a.gen == UPTR_MAX)
        SUPER_RARITY("woahverflow");

    markp ap;
    flx p = {}, pn = {};
    flx oldp = {}, oldpn = {};
    for(int c = 0;;countloops(c)){
        if(help_prev(((flx){.nil=1, .pt=mpt(&l->nil)}), &p, &pn, t))
            EWTF();
        assert(pt(p) != pt(a));
        assert(!p.locked && !p.helped && !pn.locked
               && pt(pn) && pn.nil && pt(p) != pt(a) &&
               (!eq2(oldp, p) || !eq2(oldpn, pn)));
        oldp = p;
        oldpn = pn;

        pt(a)->p.markp = ap = (markp){.nil=p.nil,.helped=1,.pt=p.pt};
        if(updx_won((flx){.helped=pn.helped, .pt=a.pt, pn.gen + 1},
                    &pt(p)->n, &pn))
            break;
    }
    casx((flx){a.mp, .gen=p.gen + 1}, &l->nil.p, (flx[]){p});
    casx((flx){.nil=p.nil,.pt=p.pt,.gen=a.gen+1}, &pt(a)->p,
         (flx[]){{.markp=ap, .gen=a.gen+1}});
    flinref_down(&p, t);
    return 0;
}

flx (lflist_deq)(type *t, lflist *l){
    flx np, oldn = {}, n = {};
    for(cnt lps = 0;;progress(&oldn, n, lps++)){
        n = flinref_read(&l->nil.n, ((flx*[]){&n, NULL}), t);
        if(help_next(((flx){.nil = 1, .pt = mpt(&l->nil)}), &n, &np, t))
            EWTF();
        if(pt(n) == &l->nil)
            return (flx){};
        if(!lflist_del(((flx){n.mp, np.gen}), t))
            return (flx){n.mp, np.gen};
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
    flx px = readx(&a->p), nx = readx(&a->n);
    flanchor *p = pt(px), *n = pt(nx);
    if(retn) *retn = nx;


    /* Early enq(a) or late del(a). */
    if(!p || !n){
        assert(!ax.nil);
               /* a's on no list. */
        assert((!px.mp && !nx.mp)
               /* enq(a) has locked a->p */
               || (!nx.mp && px.locked)
               /* enq(a, l) set a->n=&l->nil  */
               || (!p && n && px.locked
                   && pt(n->p) != a
                   && (!pt(n->n) || pt(pt(n->n)->p) != a))
               /* last stage of del(a). It should have helped enq("n")
                  first. */
               || (!n && p && pt(p->n) != a && !px.locked
                   /* try to check that del(a) helped enq(n) */
                   && (!pt(pt(p->n)->n) ||
                        pt(pt(pt(p->n)->n)->p) != a)));
    if(retn) *retn = (flx){};
        return true;
    }

    flanchor
        *pn = pt(readx(&p->n)),
        *pp = pt(readx(&p->p)),
        *np = pt(readx(&n->p)),
        *nn = pt(readx(&n->n));

    if(ax.nil){
        assert(!on || !*on || *on == cof(a, lflist, nil));
        if(on)
            *on = cof(a, lflist, nil);
        assert(p && n && pn && np && pp && nn);
        assert(!px.locked && !px.helped && !nx.locked && !px.helped);
        assert(np == a
               || (pt(np->p) == a && pt(np->n) == n && np->n.locked));
        assert(pn == a
               || (!pn->n.locked
                   && pn->p.helped
                   && pt(pn->n) == a
                   && (pt(pn->p) == p || pt(pp->n) != p)));
    }else{
        assert(!on || *on != cof(a, lflist, nil));
        assert(p != a && n != a);
        assert(nx.nil || n != p);
        if(!nx.locked){
            /* "equilibrium" after enq(a) and before del(a) sets anything,
               or after it's been reset by del(n). pn == a 4 lyfe. */
            if(!nx.nil)
                assert((pn == a && np == a) ||
                       /* del(np) has set a->n=n but not n->p=a */
                       (pn == a && pt(np->p) == a && pt(np->n) == n));
            /* enq(a) in progress. It gets messy. */
            else
                ((pn == n && np == p && px.helped)
                 /* first step, and del(np) hasn't gone too far */
                 || (pn == a && np == p && px.helped)
                 /* finished. */
                 || (pn == a && np == a)
                 /* enq(a) has stale p so hasn't set p->n=a */
                 || (pn != a && np != p && px.helped)
                 /* enq(pn) has set p->n=pn but not nil->p=pn.
                    enq(a) will help in help_prev. */
                 || (pn != n && np == p
                     && pt(pn->n) == n
                     && !pn->n.locked
                     && pn->p.helped
                     /* del(p) hasn't made its first step */
                     && (pt(pn->p) == p
                         /* del(p) is far enough along to enable pn->p
                            updates from del(pp) but hasn't helped enq(pn)
                            set nil->p=pn */
                         || (pt(pp->n) != p && p->n.locked)))
                 /* Same as above, except now enq(a) is the one in
                    trouble. It hasn't set nil->p=a but del(PRIV_P) has
                    set a->p=PRIV_P->p but hasn't helped set n->p=a. */
                 || (pn == a && np != p
                     && !nx.locked && px.helped
                     && pt(np->n) == a
                     && pt(pt(np->p)->n) != np));
        
        }
        else
            /* Stages of del(a) after locking a->n. If np!=a, then pn!=a,
               but not vice-versa. */
            assert((pn == a && np == a) ||
                   (pn == n && np == a) ||
                   (pn == n && np == p) ||
                   (pn != a && np != a));
    }

    
    /* Detect unpaused universe or reordering weirdness. */
    assert(eq2(a->p, px));
    assert(eq2(a->n, nx));
    
    return true;
}


#endif
