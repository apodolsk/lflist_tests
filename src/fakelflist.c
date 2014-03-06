#include <lflist.h>
#include <list.h>

#ifdef FAKELOCKFREE

flx flx_of(flanchor *a){
    return a;
}
flanchor *flptr(flx a){
    return a;
}

void lflist_add_before(flx a, flx n, type *h, lflist *l){
    pthread_mutex_lock(&l->lock);
    list_add_before(a, n, &l->l);
    pthread_mutex_unlock(&l->lock);
}
int lflist_remove(flx a, type *h, lflist *l){
    pthread_mutex_lock(&l->lock);
    list_remove(a, &l->l);
    pthread_mutex_unlock(&l->lock);
    return 0;
}

flx lflist_pop_front(type *h, lflist *l){
    pthread_mutex_lock(&l->lock);
    flx r = list_pop(&l->l);
    pthread_mutex_unlock(&l->lock);
    return r;
}
void lflist_add_rear(flx a, type *h, lflist *l){
    pthread_mutex_lock(&l->lock);
    list_add_rear(a, &l->l);
    pthread_mutex_unlock(&l->lock);
}

#endif
 
