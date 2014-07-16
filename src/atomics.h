#pragma once

#include <peb_util.h>

uptr _xadd(uptr s, volatile uptr *p);
uptr _xchg(uptr s, volatile uptr *p);
uptr _xchg2(dptr s, volatile dptr *p);
uptr _atomic_read(volatile uptr *p);

uptr _cas(uptr n, volatile uptr *p, uptr old);
dptr _cas2(dptr n, volatile dptr *p, dptr old);

bool _cas_won(uptr n, volatile uptr *p, uptr *old);
bool _cas2_won(dptr n, volatile dptr *p, dptr *old);

uptr _condxadd(uptr n, volatile uptr *p, uptr lim);

typedef enum{
    NOT = 0,
    WON = 1,
    OK = 2,
} howok;
howok _cas_ok(uptr n, volatile uptr *p, uptr *old);
howok _cas2_ok(dptr n, volatile dptr *p, dptr *old);

#define atomic_read(p)                          \
    PUN(typeof(*p), _atomic_read((uptr *) p))   \

#define xadd(as...) _xadd(as)
#define xchg(s, d) PUN(typeof(*d), _xchg(PUN(uptr, s), (uptr *) (d)))
#define xchg2(s, d) PUN(typeof(*d), _xchg2(PUN(dptr, s), (dptr *) (d)))
#define condxadd(n, d, lim)                     \
    ((typeof(*d)) _condxadd(n, (uptr *) d, (uptr) lim))

#define atomic_read2(addr)                      \
    PUN(typeof(*addr),                          \
        _cas2(0, addr, 0))                      \
    
#define cas(n, addr, old)                       \
    PUN(typeof(old),                            \
        trace(ATOMICS, 1, _cas,                 \
              PUN(uptr, n),                     \
              (uptr *) (addr),                  \
              PUN(uptr, old)))

#define cas2(n, addr, old)                      \
    PUN(typeof(old),                            \
        trace(ATOMICS, 1, _cas2,                \
              PUN(dptr, n),                     \
              (dptr *) (addr),                  \
              PUN(dptr, old)))

#define cas_ok(n, addr, op)                     \
    trace(ATOMICS, 1, _cas_ok,                  \
          PUN(uptr, n),                         \
          (uptr *) (addr),                      \
          (uptr *) (op))

#define cas2_ok(n, addr, op)                    \
    trace(ATOMICS, 1, _cas2_ok,                 \
          PUN(dptr, n),                         \
          (dptr *) (addr),                      \
          (dptr *) (op))

#define cas_won(n, addr, op)                    \
    trace(ATOMICS, 1, _cas_won,                 \
          PUN(uptr, n),                         \
          (uptr *) (addr),                      \
          (uptr *) (op))

#define cas2_won(n, addr, op)                   \
    trace(ATOMICS, 1, _cas2_won,                \
          PUN(dptr, n),                         \
          (dptr *) (addr),                      \
          (dptr *) (op))

#define pudef (howok, "%", *a == WON ? "WON" : *a == OK ? "OK" : "NOT")
#include <pudef.h>
