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
 * marks A->n.lk = true and then writes P->n = N and N->p = P. If T2
 * is deleting N, it sees A->n.lk and attempts to complete all of T1's
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
#define FLANC_CHECK_FREQ 90
#define MAX_LOOP 256

static int flinref_up(flx *a, type *t);
static void flinref_down(flx *a, type *t);
static err help_next(flx a, flx *n, flx *np, flx *on, type *t);
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

static inline
flx fl(flx p, flstate s, uptr gen){
    return (flx){.nil=p.nil, .st=s, .pt=p.pt, gen};
}

static
flx casx(const char *f, int l, flx n, volatile flx *a, flx *e){
    assert(n.nil || (a != &pt(n)->p && a != &pt(n)->n));
    assert(n.pt || !e->pt);
    assert(!eq2(n, *e));
    log(2, "CAS! %:% - % if %, addr:%", f, l, n, *e, a);
    flx oe = *e;
    *e = cas2(n, a, oe);
    log(2, "% %:%- found:% addr:%", eq2(*e, oe)? "WON" : "LOST", f, l, *e, a);
    if(e->gen > n.gen && e->gen - n.gen >= (UPTR_MAX >> 1))
        SUPER_RARITY("woahverflow");
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

static howok updx_ok_modhlp(const char *f, int l, flx n,
                               volatile flx *a, flx *e){
    flx oe = *e;
    casx(f, l, n, a, e);
    if(eq2(*e, oe))
        return *e = n, WON;
    if(eq2(*e, n))
        return OK;
    if(e->st == RDY && n.st == ADD){
        oe.st = n.st = RDY;
        if(eq2(*e, oe)){
            *e = oe;
            return updx_ok(f, l, n, a, e);
        }
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
#define updx_ok_modhlp(as...) updx_ok_modhlp(__func__, __LINE__, as)
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

static err refupd(flx n, flx held, type *t){
    /* if(pt(n) != pt(held) && flinref_up(&n, t)) */
    /*     return -1; */
    /* if(pt(held)) */
    /*     flinref_down(&held, t); */
    return 0;
}

static bool eqx(volatile flx *a, flx *b){
    flx old = *b;
    *b = readx(a);
    return eq2(old, *b);
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
    flx p = readx(&pt(a)->p);
    if(!pt(p) || p.gen != a.gen)
        return -1;
    flx pn = readx(&pt(p)->n);
    flx np, n = readx(&pt(a)->n), on = n;
    ppl(2, p, n);
    for(int lps = 0;; progress(&on, n, lps++)){
        if(help_next(a, &n, &np, &on, t))
            break;
        if(help_prev(a, &p, &pn, t))
            break;
        assert(pt(pn) == pt(a) && pt(np) == pt(a) && pn.st < COMMIT);

        howok lock_ok = updx_ok(fl(n, COMMIT, n.gen + 1), &pt(a)->n, &n);
        if(!lock_ok)
            continue;
        assert(!del_won || lock_ok != WON || on.st >= ABORT);
        del_won = lock_ok == WON && on.st < ABORT;

        assert(pn.st > ADD);
        pn_ok = updx_ok(fl(n, pn.st, pn.gen + 1), &pt(p)->n, &pn);
        if(pn_ok)
            break;
    }
    if(!del_won)
        goto cleanup;
    log(2, "del_won! %", a);

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
        n = readx(&pt(a)->n);
        np = refupd(n, on, t) ? (flx){} : readx(&pt(n)->p);
    }

    flx onp = np;
    if(pt(np) == pt(a))
        updx_ok_modhlp(fl(p, np.st, np.gen + n.nil), &pt(n)->p, &np);

    /* Clean up after an interrupted add of 'n'. In this case,
       a->n is the only reference to 'n' discoverable from nil,
       and we should finish the add before it gets cleared (next
       time a is added). */
    ppl(2, n, np, a, pn_ok);
    if(pt(np) && np.st == ADD && onp.gen == np.gen){
        assert(!n.nil);
        flx nn = readx(&pt(n)->n);
        if(nn.nil && nn.st == ADD){
            flx nnp = readx(&pt(nn)->p);
            if(pt(nnp) == pt(a))
                casx(fl(n, RDY, nnp.gen + 1), &pt(nn)->p, &nnp);
        }
    }

    assert(pt(a)->n.st == COMMIT &&
           pt(a)->p.gen == a.gen &&
           (pt(np) != pt(a) || eq2(p, pt(a)->p)) &&
           n.pt == pt(a)->n.pt);

    pt(a)->n.markp = pt(a)->p.markp = (markp){.st = ADD};
    
    assert(flanchor_valid(n));

cleanup:
    if(pt(n))
        flinref_down(&n, t);
    if(pt(p))
        flinref_down(&n, t);
    return -!del_won;
}

static
err (help_next)(flx a, flx *n, flx *np, flx *on, type *t){
    for(cnt nl = 0, npl = 0;; progress(on, *n, nl++)){
        if(!pt(*n))
            return -1;
        /* if(refupd(*n, *on, t)){ */
        /*     *n = readx(&pt(a)->n); */
        /*     continue; */
        /* } */
        flx onp = {};
        for(*np = readx(&pt(*n)->p);; progress(&onp, *np, nl + npl++)){
            if(pt(*np) == pt(a))
                return 0;
            if(!eqx(&pt(a)->n, n))
                break;
            if(n->st == ADD || n->st == COMMIT)
                return -1;
            assert(pt(*np) && (np->st == RDY || np->st == ADD));

            if(updx_ok_modhlp(fl(a, RDY, np->gen + n->nil), &pt(*n)->p, np))
                return 0;
        }
    }
}

static
err (help_prev)(flx a, flx *p, flx *pn, type *t){
    flx op = *p, opn = *pn;
    for(cnt pl = 0;; progress(&op, *p, pl++)){
        /* if(refupd(*p, op, t)) */
        /*     goto newp; */
        flx opp = {};
        for(cnt pnl = 0;; countloops(pl + pnl++)){
            if(!eqx(&pt(a)->p, p))
                break;
            if(pt(*pn) != pt(a)){
                if(!a.nil)
                    return -1;
                updx_ok(fl(*pn, RDY, p->gen + 1), &pt(a)->p, p);
                break;
            }
            if(p->st == ADD && !updx_ok(fl(*p, RDY, a.gen), &pt(a)->p, p))
                break;
            if(pn->st < COMMIT)
                return 0;

        readpp:;
            flx pp = readx(&pt(*p)->p);
            if(!pt(pp) || pp.st == COMMIT || pp.st == ADD){
                must(!eqx(&pt(a)->p, p));
                break;
            }
            assert(!eq2(pn, opn) || !eq2(pp, opp));
            if(refupd(pp, opp, t))
                goto readpp;
            flx ppn = readx(&pt(pp)->n), oppn = {};
            for(cnt ppnl = 0;;progress(&oppn, ppn, pl + pnl + ppnl++)){
                if(!eqx(&pt(a)->p, p))
                    goto newp;
                if(pt(ppn) != pt(*p) && pt(ppn) != pt(a))
                    goto readpp;
                if(pt(ppn) == pt(a)){
                    if(!updx_ok(fl(pp, RDY, p->gen), &pt(a)->p, p))
                        goto newp;
                    *pn = ppn;
                    /* TODO: can avoid the a->p recheck. */
                    break;
                }
                if(!updx_ok(fl(a, ppn.st == COMMIT ? ABORT : COMMIT, pn->gen + 1),
                            &pt(*p)->n, pn))
                    break;
                if(pn->st == ABORT){
                    assert((eq2(pt(a)->p, *p) && pt(pp)->n.pt == p->pt)
                           || !eq2(pt(*p)->n, *pn)
                           || !eq2(pt(*p)->p, pp));
                    return 0;
                }
                assert(ppn.st < COMMIT);
                assert(ppn.st > ADD);

                if(updx_ok(fl(a, ppn.st, ppn.gen + 1), &pt(pp)->n, &ppn))
                {
                    assert(eq2(pt(a)->p, *p)
                           || pt(a)->p.pt == pp.pt
                           || !eq2(pt(pp)->n, ppn));
                    assert(pt(*p)->n.st == COMMIT
                           || !pt(*p)->n.st
                           || !eq2(pp.gen, pt(*p)->p.gen));
                }
            }
        }
    newp:;
        if(!a.nil && (!pt(*p) || p->st == COMMIT || p->gen != a.gen))
            return -1;
        assert(a.nil || pt(*p) != pt(a));
        *pn = readx(&pt(*p)->n);
    }
}

err (lflist_enq)(flx a, type *t, lflist *l){
    if(!updx_won(fl((flx){}, COMMIT, a.gen + 1), &pt(a)->p,
                 &(flx){.st=ADD, .gen=a.gen}))
        return -1;
    assert(!pt(a)->n.mp);
    pt(a)->n = (flx){.nil=1, ADD, mpt(&l->nil), pt(a)->n.gen + 1};

    markp amp;
    flx op = {}, opn = {}, p = {}, pn = {};
    for(int c = 0;;countloops(c)){
        assert(flanchor_valid((flx){.nil=1, .pt=mpt(&l->nil)}));
        if(help_prev(((flx){.nil=1, .pt=mpt(&l->nil)}), &p, &pn, t))
            EWTF();
        assert(!eq2(op, p) || !eq2(opn, pn)), op = p, opn = pn;

        pt(a)->p.markp = amp = fl(p, ADD, 0).markp;
        if(updx_won(fl(a, umax(pn.st, RDY), pn.gen + 1), &pt(p)->n, &pn))
            break;
    }
    casx(fl(a, RDY, p.gen + 1), &l->nil.p, (flx[]){p});
    casx(fl(p, RDY, a.gen + 1), &pt(a)->p, &(flx){.markp=amp, a.gen + 1});
    flinref_down(&p, t);
    return 0;
}

flx (lflist_deq)(type *t, lflist *l){
    flx on = {}, a = (flx){.nil=1,.pt=mpt(&l->nil)};
    for(cnt lps = 0;; countloops(lps++)){
        flx np = {}, n = readx(&pt(a)->n);
        if(help_next(a, &n, &np, &on, t))
            EWTF();
        if(eq2(n, on))
            return (flx){};
        if(pt(n) == &l->nil)
            return (flx){};
        if(!lflist_del(((flx){.pt=n.pt, np.gen}), t))
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

    assert(px.st == RDY || px.st == COMMIT || px.st == ADD);

    /* Early enq(a) or late del(a). */
    if(!p || !n){
        assert(!ax.nil);
               /* a's on no list. */
        assert((!p && !n && px.st == ADD)
               /* enq(a) has locked a->p */
               || (!n && !p && px.st == COMMIT)
               /* enq(a, l) set a->n=&l->nil  */
               || (!p && n
                   && px.st == COMMIT
                   && pt(n->p) != a
                   && (!pt(n->n) || pt(pt(n->n)->p) != a))
               /* last stage of del(a). It should have helped enq("n")
                  first. */
               || (!n && p
                   && px.st == RDY
                   && pt(p->n) != a
                   /* try to check that del(a) hlp enq(n) */
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
        assert(px.st == RDY && nx.st == RDY);
        assert(np == a
               || (pt(np->p) == a && pt(np->n) == n && np->n.st >= ABORT));
        assert(pn == a
               || (pn->n.st == ADD
                   && pt(pn->n) == a
                   && (pt(pn->p) == p || pt(pp->n) != p)));
    }else{
        assert(!on || *on != cof(a, lflist, nil));
        assert(p != a && n != a);
        assert(n != p || (nx.nil && n == p));
        assert(nx.nil || nx.st != ADD);
        switch(px.st){
        case ADD:
            assert(nx.st == ADD || nx.st == RDY);
            assert(np == a ||
                   pn != a ||
                   (nx.nil && pt(np->n) == a && nx.st == ADD));
            break;
        case RDY:
            assert(np == a
                   || (pn != a && nx.st == COMMIT)
                   || (pt(np->p) == a && nx.st < COMMIT && nx.st > ADD));
            break;
        case COMMIT:
            assert(nx.st == COMMIT);
            break;
        }
        switch(nx.st){
        case ADD:
            assert(nx.nil);
            assert((pn == n && np == p)
                   /* first step, and del(np) hasn't gone too far */
                   || (pn == a && np == p)
                   /* finished. */
                   || (pn == a && np == a)
                   /* enq(a) has stale p so hasn't set p->n=a */
                   || (pn != a && np != p)
                   /* enq(pn) has set p->n=pn but not nil->p=pn.
                      enq(a) will help in help_prev. */
                   || (pn != n && np == p
                       && pt(pn->n) == n && pn->n.st == ADD
                       /* del(p) hasn't made its first step */
                       && (pt(pn->p) == p
                           /* del(p) is far enough along to enable pn->p
                              updates from del(pp) but hasn't helped enq(pn)
                              set nil->p=pn */
                           || (pt(pp->n) != p && p->n.st == COMMIT)))
                   /* Similar to above, except now enq(a) is the one in
                      trouble. It hasn't set nil->p=a but del(PREV_P) has
                      set a->p=PREV_P->p but hasn't helped set n->p=a and
                      thus hasn't cleared PREV_P->n. */
                   || (pn == a && np != p
                       && pt(np->n) == a && np->n.st == COMMIT
                       && pt(pt(np->p)->n) != np));
            break;
        case RDY: case ABORT:
            assert(pn == a);
            /* TODO: could move p->p.st from ADD to RDY */
            assert(np == a || (pt(np->p) == a && np->n.st == COMMIT));
            break;
        case COMMIT:
            assert((pn == a && np == a)
                   || (pn == n && np == a)
                   || (pn == n && np == p)
                   || (pn != n && pn != a && np != a));
            break;
        }
    }

    
    /* Detect unpaused universe or reordering weirdness. */
    assert(eq2(a->p, px));
    assert(eq2(a->n, nx));
    
    return true;
}


#endif
