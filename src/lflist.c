
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

static flanchor *flinref_read(flanchor * volatile*from,
                              flanchor *held, heritage *h, lflist *l);
static flanchor *flinref_up(flanchor *a, heritage *h, lflist *l);
static void flinref_down(flanchor *a, lflist *l);
static flanchor *help_patron(flanchor *a, flanchor *n, heritage *h, lflist *l);

static inline pxchg(flanchor *p, flgen gen){
    return (pxchg) {p, .gen = gen, .is_nil = is_nil};
}
static inline nxchg(flanchor *n, flanchor *pat){
    return (nxchg) {n, pat};
}

static
flanchor *flinref_read(flanchor * volatile*from, flanchor *held, heritage *h){
    markptr ma = *(markptr *) from;
    if(ma.read)
        /* This handles markptr and genptr. If genptr is locked, then
           p == held. */
        return held;
    flanchor *a = (flanchor *) ma.patint;
    if(held == a)
        return a;
    flinref_down(held, l);
    return flinref_up(a, h, l);
}

static
flanchor *flinref_up(flanchor *a, heritage *h){
    if(is_nil(a))
        return a;
    return linref_up((void *) a, h) ? NULL : a;
}

static
void flinref_down(flanchor *a, lflist *l){
    if(!is_nil(a))
        linref_down(a);
}

int lflist_remove_any(flanchor *a, heritage *h){
    return lflist_remove(a, h, a->host);
}

int lflist_remove(flanchor *a, heritage *h, uintptr_t gen){
    assert(!is_nil(a));

    uintptr_t gen;

    for(flanchor *p = NULL, *n = NULL;;){
        while(1){
            n = help_patron(a, n, h);
            flgen ngen = n->gen;
            if(n->p != a)
                continue;
            
            while(1){
                p = flinref_read(&a->p, p, h);
                if(!p)
                    continue;
                nxchg pnx = {p->n, p->pat};
                if(pnx.n == a || pnx.pat == a)
                    if(cas2_ok(nxchg(n, a), &p->n, pnx))
                        break;
            }

            if(cas2_ok(pxchg(n, (flgen){ngen, TRUE}), &a->p,
                       pxchg(p, gen)))
                

            if(cas_ok(((genptr){.ngen = ngen, .locked = true}), &a->p, p))
                break;
        }

               pxchg r = cas2(pxchg(p, ngen), &n->p, pxchg(a, ngen));
        if(r.gen == ngen && (r.p == p || r.p == a))
            break;
        if(p->patint != (uintptr_t) a && a->locked)
            break;
    }
    
    flinref_down(n);
    flinref_down(p);

    a->gen++;
    a->host = NULL;
    return 0;
}

static
flanchor *help_patron(flanchor *a, flanchor *n, heritage *h)
{
    flanchor *pat = NULL;
    while(1){
        n = flinref_read(&a->n, n, h);
        if(n && n->p == a)
            goto return_n;

        pat = flinref_read(&a->pat, pat, h);
        if(!pat)
            continue;

        pxchg patpx = atomic_read_128b(&pat->p);
        if(patpx.p == a)
            goto return_pat;
        if(pat->realp != a)
            if(a->pat == pat && atomic_eq_128b(&pat->p, patpx))
                return NULL;
            else
                continue;
        if(patpx.p != n)
            continue;

        flgen ngen = patpx.gen;
        if(cas2_ok(pxchg(a, ngen), &n->p, pxchg(pat, ngen)))
            goto return_n;

        if(cas2_ok(pxchg(a, ngen), &pat->p, patpx))
            goto return_pat;
    }

return_n:
    flinref_down(pat, l);
    return n;
return_pat:
    flinref_down(n, l);
    return pat;
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

        if(!cas2_ok(((pxchg){client, gen + 1}), &n->p,
                    ((pxchg){p, gen})))
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
            /* n may rewrite p.pat once, but that's OK. */
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
    for(flanchor *p; (p = l->nil.p);){
        if(flinref_up(p, h, l))
            continue;
        if(lflist_remove(p, h, l) == l)
            flinref_down(p, l);
            return p;
    }
    return NULL;
}
