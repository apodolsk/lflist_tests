#ifdef FAKELOCKFREE

#include <lflist.h>
#include <list.h>

flx flx_of(flanchor *a){
    return a;
}
flanchor *flptr(flx a){
    return a;
}

void lflist_add_before(flx a, flx n, heritage *h, lflist *l){
    pthread_mutex_lock(&l->lock);
    list_add_before(a, n, l);
    pthread_mutex_unlock(&l->lock);
}
int lflist_remove(flx a, heritage *h, lflist *l){
    pthread_mutex_lock(&l->lock);
    list_remove(a, l);
    pthread_mutex_unlock(&l->lock);
    return 0;
}

flx lflist_pop_front(heritage *h, lflist *l){
    pthread_mutex_lock(&l->lock);
    flx r = list_pop(l);
    pthread_mutex_unlock(&l->lock);
    return r;
}
void lflist_add_rear(flx a, heritage *h, lflist *l){
    pthread_mutex_lock(&l->lock);
    flx r = list_add_read(a, l);
    pthread_mutex_unlock(&l->lock);
}

#endif
 
