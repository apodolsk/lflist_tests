#include <lflist.h>

#ifdef FAKELOCKFREE
#include <list.h>
#include <atomics.h>
#include <global.h>


#include <pthread.h>

static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

static
void lock_lflist(lflist *l){
    (void) l;
    pthread_mutex_lock(&l->mut);
    /* pthread_mutex_lock(&mut); */
    /* disable_interrupts(); */
}

static
void unlock_lflist(lflist *l){
    (void) l;
    pthread_mutex_unlock(&l->mut);
    /* pthread_mutex_unlock(&mut); */
    /* enable_interrupts(); */
}

flx flx_of(flanchor *a){
    return (flx){a, a->gen};
}

flanchor *flptr(flx a){
    return a.a;
}

err (lflist_del)(flx a, type *h){
    (void) h;
    lflist *l;
    while(1){
        l = a.a->host;
        if(!l)
            return -1;
        lock_lflist(l);
        if(a.a->host == l)
            break;
        unlock_lflist(l);
    }
    if(a.a->gen != a.gen || !a.a->lanc.n || ((uptr) a.a->lanc.n & 1))
        return unlock_lflist(l), -1;
    list_remove(&a.a->lanc, &l->l);
    a.a->lanc = (lanchor) LANCHOR;
    a.a->host = NULL;
    unlock_lflist(l);
    return 0;
}

flx (lflist_deq)(type *h, lflist *l){
    (void) h;
    lock_lflist(l);
    flx rlx = (flx){};
    flanchor *r = cof(list_pop(&l->l), flanchor, lanc);
    if(r){
        rlx = (flx){r, r->gen};
        r->lanc = (lanchor) LANCHOR;
        r->host = NULL;
    }
    unlock_lflist(l);
    return rlx;
}

err (lflist_enq)(flx a, type *h, lflist *l){
    (void) h;
    lock_lflist(l);
    if(!cas_ok(a.gen + 2, &a.a->gen, a.gen) || a.a->lanc.n)
        return unlock_lflist(l), -1;
    a.a->host = l; 
    list_add_rear(&a.a->lanc, &l->l);
    unlock_lflist(l);
    return 0;
}
 
#endif
