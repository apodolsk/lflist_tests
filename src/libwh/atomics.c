#define MODULE ATOMICS

#include <atomics.h>
#include <asm.h>

#define FUZZ_ATOMICS 1
#define FUZZ_NS 9000
#define FUZZ_PCNT 50
#define FUZZ_MOD 2

extern uptr (xadd)(iptr s, volatile uptr *p);
extern uptr (xchg)(uptr s, volatile uptr *p);
extern uptr (cmpxchg)(uptr n, volatile uptr *p, uptr old);
extern dptr (cmpxchg2)(dptr n, volatile dptr *p, dptr old);
    
#include <time.h>
void fuzz_atomics(){
    if(!interrupts_enabled())
        return;
    if(FUZZ_ATOMICS &&
       0 == mod_pow2(PUN(uptr, get_dbg_id()), FUZZ_MOD)
       && randpcnt(FUZZ_PCNT))
        nanosleep(&(struct timespec){.tv_nsec = wrand() % FUZZ_NS},
                  NULL);
}

uptr _xadd(iptr s, volatile uptr *p){
    assert(aligned_pow2(p, sizeof(*p)));
    fuzz_atomics();
    return (xadd)(s, p);
}

uptr _xchg(uptr s, volatile uptr *p){
    assert(aligned_pow2(p, sizeof(*p)));
    fuzz_atomics();
    return (xchg)(s, p);
}

dptr _xchg2(dptr s, volatile dptr *p){
    assert(aligned_pow2(p, sizeof(*p)));
    fuzz_atomics();
    while(1){
        dptr o = *p;
        if(cmpxchg2(s, p, o) == o)
            return o;
    }
}

uptr _cas(uptr n, volatile uptr *p, uptr old){
    assert(aligned_pow2(p, sizeof(*p)));
    fuzz_atomics();
    return cmpxchg(n, p, old);
}

dptr _cas2(dptr n, volatile dptr *p, dptr old){
    assert(aligned_pow2(p, sizeof(*p)));
    fuzz_atomics();
    return cmpxchg2(n, p, old);
}

howok _cas_ok(uptr n, volatile uptr *p, uptr *old){
    uptr o = *old;
    *old = _cas(n, p, o);
    if(*old == o)
        return WON;
    if(*old == n)
        return OK;
    return NOT;
}

howok _cas2_ok(dptr n, volatile dptr *p, dptr *old){
    dptr o = *old;
    *old = _cas2(n, p, o);
    if(*old == o)
        return WON;
    if(*old == n)
        return OK;
    return NOT;
}

bool _cas_won(uptr n, volatile uptr *p, uptr *old){
    return _cas_ok(n, p, old) == WON;
}

bool _cas2_won(dptr n, volatile dptr *p, dptr *old){
    return _cas2_ok(n, p, old) == WON;
}

uptr _atomic_read(volatile uptr *p){
    fuzz_atomics();
    return *p;
}

uptr _condxadd(uptr n, volatile uptr *p, uptr lim){
    for(uptr r = *p;;){
        assert(r < lim);
        if(r + n >= lim)
            return lim;
        if(_cas_won(r + n, p, &r))
            return r;
    }
}
