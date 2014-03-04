
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

    for(flx nx = {}; px = {};;){
        while(1){
            do{
                flx oldnx = atomic_read2(&a->nx);
                nx = help_patron(a, n, h);
            }while(!cas2_ok(nx, &a->nx, oldnx));
            
            do{
                px = flinref_read(&a->px, p, h);
                if(px.igen != px.gen.igen)
                    return -1;
            }while(!cas2_ok(flx(px.p, (flgen){px.igen, .locked = 1}),
                            &a->px, px));

            if(cas2_ok(flx(px.pr, nx.gen), &nx.p->px, flx(a, nx.gen))){
                flx pnx = atomic_read2(&px.p->nx);
                
            }
        }
    }
    
    flinref_down(n);
    flinref_down(p);

    a->host = NULL;
    return 0;
}

static
flanchor *help_patron(flanchor *a, flanchor *n, heritage *h)
{
    while(1){
        pat = flinref_read(&a->n, n, h);

        n = atomic_read_128b(&pat.p->px);
        if(n.p == a)
            return n;

        uintptr_t patgen = pat.p->gen;
        if(flinref_up(n.p))
            continue;
        flx npx = cas2(flx(a, n.gen), &n.p->px, flx(pat.p, n.gen));
        if(npx.p == a || npx.p == pat.p && npx.gen == n.gen){
            flinref_down(pat.p);
            return npx;
        }
        
        if(cas2_ok(flx(a, patgen), &nx.p->px, n)){
            flinref_down(n.p);
            return npx;
        }
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
