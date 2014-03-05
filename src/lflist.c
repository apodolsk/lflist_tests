
/**
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 *
 * Lockfree doubly-linked list.
 *
 * Trickiness with gen and extra ops. ngen means that a->pat == pat bc
 * n->p == pat and if pat had disappeared than n->gen would change.
 *
 */

#include <stdlib.h>
#include <peb_macros.h>
#include <atomics.h>
#include <stdbool.h>
#include <lflist.h>
#include <nalloc.h>

static flx *flinref_read(flx * volatile*from, flanchor *held, heritage *, list *l);
static flanchor *flinref_up(flanchor *a, heritage *h, lflist *l);
static void flinref_down(flanchor *a, lflist *l);
static flanchor *help_patron(flanchor *a, flanchor *n, heritage *h, lflist *l);

static inline flx flx_(flanchor *pt, flgen gen){ return (flx) {pt, gen}; }

static
flx *flinref_read(flx * volatile*from, flanchor *held, heritage *h, list *l){
    do{
        flx a = atomic_read2(from);
        if(a.pt == held)
            break;
        flinref_down(held, l);
    }while(flinref_up(a, h, l));
    return a;
}

static
flanchor *flinref_up(flanchor *a, heritage *h, list *l){
    if(a == &l->nil)
        return a;
    return linref_up((void *) a, h) ? NULL : a;
}

static
void flinref_down(flanchor *a, lflist *l){
    if(a == &l->nil)
        linref_down(a);
}

int lflist_remove(flanchor *a, heritage *h, uintptr_t gen, list *l){
    assert(a != &l->is_nil);

    flx n = {}, p = {};
    do{
        do{
            flx oldn = atomic_read2(&a->n);
            n = help_next(a, n.pt, h, l);
            if(!n.p)
                goto cleanup;
        }while(!cas2_ok(n, &a->n, oldn));
        do{
            p = help_prev(a, p.pt, h, l);
            if(p.igen != gen || !p.pt)
                goto cleanup;
        }while(!cas2_ok(flx_(p.pt, (flgen){p.igen, .locked = 1}), &a->p, p));
    }while(!cas2_ok(flx_(p.ptr, n.gen), &n.pt->p, flx_(a, n.gen)));
    
        
    int ret = 0;
    if(cas2_ok(flx_(NULL, gen + 4), &a->p, p)){
        cas2(n, &p.pt->n, flx_(a, (flgen){gen}));
        a->n = {};
    }
    else
        ret -1;

cleanup:
    flinref_down(n.pt, l);
    flinref_down(p.pt, l);
    return ret;
}

static
flx help_prev(flanchor *a, flanchor *p, heritage *h, list *l){
    do{
        flx p = flinref_read(&a->p, p, h, l);
        flx pn = atomic_read2(&p.pt->n);
        if(pn.pt == a)
            return p;
        if(flinref_up(pn.pt, h, l))
            continue;

        if(!atomic_eq2(&a->p, p))
            continue;
        if(pn.pt->n.pt != a && atomic_eq2(&p.pt->n, pn))
            return flx_(NULL, (flgen){});
    }while(!cas2_ok(flx_(a, pn.gen), &p.pt->n, pn));
}

static
flx help_next(flanchor *a, flanchor *n, heritage *h, list *l)
{
    flx n = flx_(n, (flgen){});
    while(1){
        pat = flinref_read(&a->n, n.n, h, l);

        patp = atomic_read_128b(&pat.pt->p);
        if(patp.pt != a)
            if(atomic_eq2(&a->n, pat))
                return flx_(NULL, 0);
            else
                continue;
        if(!patp.locked && !patp.unlocking)
            return pat;

        n = flinref_read(&pat.pt->n, NULL, h, l);
        flx np = cas2(flx_(a, n.gen), &n.pt->p, flx_(pat.pt, n.gen));
        if(np.pt == a || (np.pt == pat.pt && np.gen == n.gen)){
            flinref_down(pat.pt);
            return n;
        }

        flx new = flx_(a, (flgen){patp.igen,
                                .unlocking = 1, .nil = patp.nil});
        if(cas2_ok(new, &pat.pt->p, patp) &&
           atomic_eq2(&pat.pt->n, n) &&
           cas2_ok(flx_(a, (flegn){patp.igen, .nil = patp.xnil}, &pat.pt->n)))
        {            
            flinref_down(n.pt);
            return n;
        }
}

int lflist_add_rear(flanchor *a, heritage *h, lflist *l){
    return lflist_add_before(a, &l->nil, h, l);
}

int lflist_add_before(flanchor *a, flanchor *n, heritage *h){
    a->n = n;
    flanchor *p = NULL;
    while(a->n == n && !a->pat && !a->gen){
        p = flinref_read(&n->p, p, h);
        if(!p)
            continue;
        int gen = l->nil.gen;
        
        flanchor *client = cas(a, &p->n, n);
        if(client == n)
            client = a;
        else if(flinref_up(client, h))
            continue;

        if(!cas2_ok(((pchg){client, gen + 1}), &n->p,
                    ((pchg){p, gen})))
            continue;

        if(client != a)
            flinref_down(client);
        else
            break;
    }

    flinref_down(p, l);
    return 0;
}

/* Doesn't return lflist * b/c: what happens if host == ADDING. */
int lflist_add_after_priv(flanchor *a, flanchor *p,
                          heritage *h, lflist *l)
{
    assert(p->host == l);
    if(!cas_ok(ADDING, &a->host, NULL))
        return -1;

    a->p = p;
    for(flanchor *n = NULL;;){
        n = help_patron(p, n, h, l);
        a->n = n;
        if(cas_ok(a, &n->p, &p)){
            p->pat = NULL;
            /* n may rewrite p.ptat once, but that's OK. */
            p->n = a;

            flinref_down(n, l);
            
            a->host = l;
            return 0;
        }
    }
}

int lflist_add_front_priv(flanchor *a, heritage *h, lflist *l){
    return lflist_add_after_priv(&l->nil, a, h, l);
}

flanchor *lflist_pop_front_priv(heritage *h, lflist *l){
    for(flanchor *n = NULL;;){
        n = help_patron(&l->nil, n, h, l);
        if(n == &l->nil)
            return NULL;
        if(lflist_remove(n, h, l) == l){
            flinref_down(n, l);
            return n;
        }
    }
}

flanchor *lflist_pop_rear(heritage *h, lflist *l){
    for(flanchor *p; (p = l->nil.pt);){
        if(flinref_up(p, h, l))
            continue;
        if(lflist_remove(p, h, l) == l)
            flinref_down(p, l);
            return p;
    }
    return NULL;
}
