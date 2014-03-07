#define MODULE ATOMICS

#include <atomics.h>
#include <pthread.h>
#include <time.h>
#include <prand.h>
#include <global.h>

uptr _cmpxchg64b(uptr _s, volatile uptr *d, uptr _e){
    void *s = (void *)_s;
    void *e = (void *)_e ;
    trace(s, p, d, p, e, p);

    if(CAS_SLEEP && randpcnt(CAS_SLEEP_PCNT)){
        nanosleep(&(struct timespec){.tv_nsec = CAS_SLEEP},
                  &(struct timespec){});
        pthread_yield();
    }
    return cmpxchg64b(_s, d, _e);
}

dptr _cmpxchg128b(dptr _s, volatile dptr *d, dptr _e){
    void *sl = (void *)_s;
    void *sh = (void *)(_s>>64);
    void *el = (void *)_e;
    void *eh = (void *)(_e>>64);
    log("s:%p%p d:%p e:%p%p", sh, sl, d, eh, el);
    pthread_yield();

    if(CAS_SLEEP && randpcnt(CAS_SLEEP_PCNT)){
        nanosleep(&(struct timespec){.tv_nsec = CAS_SLEEP},
                  &(struct timespec){});
        pthread_yield();
    }
    return cmpxchg128b(_s, d, _e);
}
