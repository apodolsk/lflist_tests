#pragma once

#include <peb_util.h>

uptr _xadd(uptr s, uptr *p);
uptr _xchg(uptr s, uptr *p);
uptr _xchg2(dptr s, dptr *p);

uptr _cmpxchg(uptr n, volatile uptr *p, uptr old);
dptr _cmpxchg2(dptr n, volatile dptr *p, dptr old);

#define xadd(s, d)                              \
    PUN(typeof(*d),                             \
        _xadd(PUN(uptr, s), (uptr *) d))

#define xchg2(s, d)                             \
    PUN(typeof(*d),                             \
        _xchg2(PUN(dptr, s), (dptr *) d))       \

#define atomic_read2(addr)                      \
    PUN(typeof(*addr),                          \
        _cmpxchg2(0, *addr, 0))               \

#define cas(n, addr, old)                           \
    PUN(typeof(old),                                \
          _cmpxchg(PUN(uptr, n),                    \
                   (uptr *) (addr),                 \
                   PUN(uptr, old)))

#define cas2(n, addr, old)                          \
    PUN(typeof(old),                                \
         _cmpxchg2(PUN(dptr, n),                    \
                   (dptr *) (addr),                 \
                   PUN(dptr, old)))

#define cas_ok(n, addr, old)                    \
    eq(old, cas(n, addr, old))                  \

#define cas2_ok(n, addr, old)                   \
    eq2(old, cas2(n, addr, old))                 \

