#include <lflist.h>

#ifdef FAKELOCKFREE
#include <list.h>
#include <atomics.h>
#include <global.h>


#include <pthread.h>

pthread_mutex_t *mut = &((pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER);

static
void lock_world(void){
    pthread_mutex_lock(mut);
    /* disable_interrupts(); */
}

static
void unlock_world(void){
    pthread_mutex_unlock(mut);
    /* enable_interrupts(); */
}

flx flx_of(flanchor *a){
    return (flx){a, a->gen};
}

flanchor *flptr(flx a){
    return a.a;
}

err lflist_add_before(flx a, flx n, type *h, lflist *l){
    (void) h;
    lock_world();
    assert(!(a.a->gen & 1));
    if(!cas_ok(a.gen + 2, &a.a->gen, a.gen) || a.a->lanc.n)
        return unlock_world(), -1;
    list_add_before(&a.a->lanc, &n.a->lanc, &l->l);
    unlock_world();
    return 0;
}

err lflist_del(flx a, type *h, lflist *l){
    (void) h;
    lock_world();
    if(a.a->gen != a.gen || !a.a->lanc.n || ((uptr) a.a->lanc.n & 1))
        return unlock_world(), -1;
    list_remove(&a.a->lanc, &l->l);
    unlock_world();
    return 0;
}

flx lflist_deq(type *h, lflist *l){
    (void) h;
    lock_world();
    flx rlx = (flx){};
    flanchor *r = cof(list_pop(&l->l), flanchor, lanc);
    if(r)
        rlx = (flx){r, r->gen};
    unlock_world();
    return rlx;
}

err lflist_enq(flx a, type *h, lflist *l){
    (void) h;
    lock_world();
    if(!cas_ok(a.gen + 2, &a.a->gen, a.gen) || a.a->lanc.n)
        return unlock_world(), -1;
    list_add_rear(&a.a->lanc, &l->l);
    unlock_world();
    return 0;
}
 
#endif
