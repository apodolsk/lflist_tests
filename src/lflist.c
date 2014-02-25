/**
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 *
 * Lockfree doubly-linked list.
 *
 */

static
flanchor *flinref_read(flanchor **from, flanchor *held, heritage *h, list *l)
{
    marpktr *ma = *(markptr *) from;
    if(ma.read)
        /* This handles markptr and genptr. If genptr is locked, then
           p == held. */
        return holding;
    a = (flanchor *) ma.patint;
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

void lflist_peek(lflist *l, heritage *h){
    flanchor *n = help_patron(&l->nil, l, h);
    return n == l->nil ? NULL : n
}

void lflist_add_rear(flanchor *a, lflist *l, heritage *h){
    if(cas(ADDING, &a->host, 0))
        return -1;
    
    a->next = &l.nil;
    flanchor *p = NULL;
    while(a->n == &l.nil && !a->pat && !a->gen){
        p = flinref_read(&l->nil.p, p, h, l);
        if(!p)
            continue;
        int gen = l->nil.gen;
        
        flanchor *client = (flanchor *) cas(a, &p->n, &l->nil);
        if(client == &l->nil)
            client = a;
        else if(flinref_up(client, h))
            continue;

        if(!cas64_ok((pxchg){client, gen + 1}, &l->nil.px, (pxchg){p, gen}))
            continue;

        if(client != a)
            flinref_down(client);
        else
            break;
    }

    flinref_down(p);
    return;
}

list *lflist_remove(flanchor *a, lflist *l, heritage *h){
    assert(!is_nil(a));

    list *bl = (list *) cas(REMOVING, &a->hosti, l);
    if(bl != l)
        return bl;
    
    for(flanchor *p = NULL, *n = NULL){
        while(1){
            n = help_patron(a, n, h, l);
            int ngen = n->gen;
            if(n->p != a)
                continue;
            
            while(1){
                p = flinref_read(&a->p, p, h, l);
                if(!p)
                    continue;
                nxchg pnx = p->nx;
                if(pnx.n == a || pnx.patint == (uint) a
                   && cas64_ok((nxchg){n, .pat = a}, &p->nx, pnx))
                    break;
            }

            genptr pg = (genptr) a->p;
            if(!pg.locked && (flanchor *) pg != p)
                continue;
            if(cas_ok((genptr){ngen, TRUE}, &a->p, pg))
                break;
        }

        pxchg r = cas64((pxchg){p, ngen}, &n->p, (pxchg){a, ngen});
        if(r.gen == ngen && (r.p == p || r.p == a))
            break;
        if(p->patint != (uint) a && a->locked)
            break;
        cas(p, &a->p, (genptr) {ngen, TRUE});
    }
done:
    flinref_down(n, l);
    flinref_down(p, l);

    a->host = NULL;
}

static
flanchor *help_patron(flanchor *a, flanchor *n, heritage *h, list *l)
{
    for(flanchor *pat = NULL;;){
        n = flinref_read(&a->nx.n, n, h, l);
        if(n && n->px.p == a)
            goto return_n;

        pat = flinref_read(&a->nx.pat, pat, h, l);
        if(!pat)
            continue;
        if(!cas_ok((markptr) {(uint) pat, .read = TRUE}, &a->nx.pat, pat))
            continue;

        genptr patp = (genptr) pat->px.p;
        if(patp.p == a)
            goto return_pat;
        if(a->nx.n != n || !a->nx.pat.read)
            continue;
        if(!patp.locked)
            continue;

        int ngen = patpx.gen;
        if(n && cas64_ok((pxchg){a, ngen}, &n->px, (pxchg){pat, ngen}))
            goto return_n;
        
        if(cas_ok(a, &pat->px.p, patp))
            goto return_pat;
    }

return_n:
    flinref_down(pat);
    return n;
return_pat:
    flinref_down(n);
    return pat;
}
