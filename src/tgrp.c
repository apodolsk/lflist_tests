#include <pthread.h>
#include <tgrp.h>
#include <list.h>
#include <atomics.h>

static targ tgrp_main(tctxt *t){
    sem_wait(&t->rdy);
    return t->main(t->arg);
}

void mutex_lock(mutex *m){
    muste(pthread_mutex_lock(m));
}

void mutex_unlock(mutex *m){
    muste(pthread_mutex_unlock(m));
}

err tgrp_create(tid *id, targ(*main)(targ), targ arg, tgrp *g){
    tctxt *t = malloc(sizeof(tctxt));
    if(!t)
        return -1;
    *t = (tctxt){.tgid=xadd(1, &g->next_id), .main=main, .arg=arg};
    muste(sem_init(&t->rdy, 0, 0));
    if(pthread_create(&t->ptid, NULL, (targ (*)(targ)) tgrp_main, (targ) t))
        return free(t), -1;
    sem_post(&t->rdy);

    mutex_lock(&g->l);
    list_enq(&t->lanc, &g->threads);
    mutex_unlock(&g->l);
    return 0;
}


tid tgrp_join(tgrp *g){
    mutex_lock(&g->l);
    tctxt *t = cof(list_deq(&g->threads), tctxt, lanc);
    mutex_unlock(&g->l);
    if(!t)
        return -1;
    tid id = t->tgid;
    muste(pthread_join(t->ptid, (void *[1]){}));
    free(t);
    return id;
}
