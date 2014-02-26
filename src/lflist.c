/**
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 *
 * Lockfree doubly-linked list.
 *
 */

#include <stdlib.h>
#include <peb_macros.h>
#include <atomics.h>
#include <stdbool.h>
#include <lflist.h>

extern int linref_up(volatile void *, heritage *);
extern void linref_down(volatile void *);

static flanchor *flinref_read(flanchor * volatile*from,
                              flanchor *held, heritage *h, list *l);
static flanchor *flinref_up(flanchor *a, heritage *h, list *l);
static void flinref_down(flanchor *a, list *l);
static flanchor *help_patron(flanchor *a, flanchor *n, heritage *h, list *l);

static
flanchor *flinref_read(flanchor * volatile*from, flanchor *held,
                       heritage *h, list *l){
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
flanchor *flinref_up(flanchor *a, heritage *h, list *l){
    if(a == &l->nil)
        return a;
    return linref_up(a, h) ? NULL : a;
}

static
void flinref_down(flanchor *a, list *l){
    if(a && a != &l->nil)
        linref_down(a);
}

flanchor *lflist_pop(heritage *h, list *l){
    for(flanchor *n = NULL;;){
        n = help_patron(&l->nil, n, h, l);
        if(n == &l->nil)
            return NULL;
        if(lflist_remove(n, h, l) == l)
            return n;
    }
}

list *lflist_add_rear(flanchor *a, heritage *h, list *l){
    list *bl = cas(ADDING, &a->host, NULL);
    if(bl)
        return bl;
    
    a->n = &l->nil;
    flanchor *p = NULL;
    while(a->n == &l->nil && !a->pat && !a->gen){
        p = flinref_read(&l->nil.p, p, h, l);
        if(!p)
            continue;
        int gen = l->nil.gen;
        
        flanchor *client = cas(a, &p->n, &l->nil);
        if(client == &l->nil)
            client = a;
        else if(flinref_up(client, h, l))
            continue;

        if(!cas2_ok(((pxchg){client, gen + 1}), &l->nil.p,
                    ((pxchg){p, gen})))
            continue;

        if(client != a)
            flinref_down(client, l);
        else
            break;
    }

    flinref_down(p, l);
    return NULL;
}

list *lflist_remove(flanchor *a, heritage *h, list *l){
    assert(a != &l->nil);

    list *bl = cas(REMOVING, &a->host, l);
    if(bl != l)
        return bl;

    flanchor *p = NULL, *n = NULL;
    while(1){
        int ngen;
        while(1){
            n = help_patron(a, n, h, l);
            ngen = n->gen;
            if(n->p != a)
                continue;
            
            while(1){
                p = flinref_read(&a->p, p, h, l);
                if(!p)
                    continue;
                nxchg pnx = {p->n, p->pat};
                if(pnx.n == a || pnx.patint == (uintptr_t) a)
                    if(cas2_ok(((nxchg){n, .pat = a}), &p->n, pnx))
                        break;
            }

            if(cas_ok(((genptr){.ngen = ngen, .locked = true}), &a->p, p))
                break;
        }

        pxchg r = cas2(((pxchg){p, ngen}), &n->p, ((pxchg){a, ngen}));
        if(r.gen == ngen && (r.p == p || r.p == a))
            break;
        if(p->patint != (uintptr_t) a && a->locked)
            break;
        cas(p, &a->p, ((genptr) {.ngen = ngen, .locked = true}));
    }
    
    flinref_down(n, l);
    flinref_down(p, l);

    a->host = NULL;
    return l;
}

static
flanchor *help_patron(flanchor *a, flanchor *n, heritage *h, list *l)
{
    flanchor *pat = NULL;
    while(1){
        n = flinref_read(&a->n, n, h, l);
        if(n && n->p == a)
            goto return_n;

        pat = flinref_read(&a->pat, pat, h, l);
        if(!pat)
            continue;
        if(!cas_ok(((markptr) {.patint = (uintptr_t) pat, .read = true}),
                   &a->pat, pat))
            continue;

        genptr patp = (genptr) pat->p;
        if(patp.p == a)
            goto return_pat;
        if(a->n != n || !a->read)
            continue;
        if(!patp.locked)
            continue;

        int ngen = patp.ngen;
        if(n && cas2_ok(((pxchg){a, ngen}), &n->p, ((pxchg){pat, ngen})))
            goto return_n;
        
        if(cas_ok(a, &pat->p, patp))
            goto return_pat;
    }

return_n:
    flinref_down(pat, l);
    return n;
return_pat:
    flinref_down(n, l);
    return pat;
}
