#define MODULE ATOMICS

#include <prand.h>
#include <global.h>

#define ATOMIC_FUZZ 1000000
#define ATOMIC_FUZZ_PCNT 0

extern uptr xadd(uptr s, volatile uptr *p);
extern uptr xchg(uptr s, volatile uptr *p);
extern uptr cmpxchg(uptr n, volatile uptr *p, uptr old);
extern dptr cmpxchg2(dptr n, volatile dptr *p, dptr old);
    
#include <time.h>
static void fuzz_atomics(){
    if(ATOMIC_FUZZ && randpcnt(ATOMIC_FUZZ_PCNT))
        nanosleep(&(struct timespec){.tv_nsec = ATOMIC_FUZZ}, NULL);
}

uptr _xadd(uptr s, volatile uptr *p){
    assert(aligned_pow2(p, sizeof(*p)));
    fuzz_atomics();
    return xadd(s, p);
}

uptr _xchg(uptr s, volatile uptr *p){
    assert(aligned_pow2(p, sizeof(*p)));
    fuzz_atomics();
    return xchg(s, p);
}

uptr _xchg2(dptr s, volatile dptr *p){
    assert(aligned_pow2(p, sizeof(*p)));
    fuzz_atomics();
    while(1){
        dptr o = *p;
        if(cmpxchg2(s, p, o) == o)
            return o;
    }
}

uptr _cmpxchg(uptr n, volatile uptr *p, uptr old){
    assert(aligned_pow2(p, sizeof(*p)));
    fuzz_atomics();
    return cmpxchg(n, p, old);
}

dptr _cmpxchg2(dptr n, volatile dptr *p, dptr old){
    assert(aligned_pow2(p, sizeof(*p)));
    fuzz_atomics();
    return cmpxchg2(n, p, old);
}

