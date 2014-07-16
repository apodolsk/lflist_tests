#pragma once
#include <pumacros.h>
#include <peb_assert.h>

#define CASSERT(e) _Static_assert(e, #e)

#define ARR_LEN(a) (sizeof(a)/sizeof(*a))

/** 
 * Return the address of the struct s containing member_ptr, assuming s
 * has type container_type and member_ptr points to its field_name
 * field. If member_ptr is NULL, return NULL.
 */
#define cof container_of
#define container_of(member_ptr, container_type, field_name)        \
    ((container_type *)                                             \
     subtract_if_not_null((void *) member_ptr,                      \
                          offsetof(container_type, field_name)))

/* Used to do a NULL check without expanding member_ptr twice. */
static inline const void *subtract_if_not_null(const void *ptr, cnt s){
    return ptr == NULL ? ptr : (void *)((u8 *)ptr - s);
}

/* Clang has a buggy statement-expression implementation. */
/* #define PUN(t, s) ({                                                \ */
/*         CASSERT(sizeof(s) == sizeof(t));                            \ */
/*         ((union {__typeof__(s) str; t i;}) (s)).i;                  \ */
/*         })                                                       */

/* #define PUN(t, s)                                               \ */
/*     (assert(sizeof(s) == sizeof(t)),                            \ */
/*      ((union {__typeof__(s) str; t i;}) (s)).i) */

#define PUN(t, s)                                               \
    (((union {__typeof__(s) str; t i;}) (s)).i)

/* #define PUN(t, s) (*(t*)(typeof(s)[]){s}) */

#define eq(a, b) ({ typeof(b) __eqa = a; (PUN(uptr, __eqa) == PUN(uptr, b)); })

#define eq2(a, b) ({ typeof(b) __eq2a = a; (PUN(dptr, __eq2a) == PUN(dptr, b)); })

static inline void must(err e){
    assert(!e);
}

static inline void *mustp(void *p){
    assert(p);
    return p;
}

#define in_struct(p, s)                                         \
    ((uptr) (p) >= (uptr) (s) && (uptr) (p) < (uptr)((s) + 1))  \

#define is_pow2(n) ((n) && !((n) & ((n) - 1)))

/* static inline bool is_pow2(uint n){ */
/*     return n && !(n & (n - 1)); */
/* } */

static inline uint div_pow2(uint n, uint by){
    if(!n)
        return 0;
    assert(is_pow2(by));
    return n >> __builtin_ctz(by);
}

static inline uint div_rup_pow2(uint n, uint by){
    if(!n)
        return 0;
    assert(is_pow2(by));
    uint q = n >> __builtin_ctz(by);
    bool r_notzero = !!(n & (by - 1));
    return q + r_notzero;
}

#define aligned(addr, size)                     \
    (((uptr)(addr) % (size)) == 0)

#define align_down(addr, size)                              \
    ((void *) ualign_down((uptr) (addr), (size)))
static inline uptr ualign_down(uptr addr, size size){
    return addr - addr % size;
}

#define align_up(addr, size)                    \
    ((void *) ualign_up((uptr) (addr), (size)))
static inline uptr ualign_up(uptr addr, size size){
    return ualign_down(addr + size - 1, size);
}

#define align_down_pow2(n, size)                \
    (typeof (n)) (({assert(is_pow2(size));}),   \
     (uptr) (n) & ~((size) - 1))

#define align_up_pow2(n, size)                          \
    (typeof (n)) (({assert(is_pow2(size));}),           \
     align_down_pow2((uptr) (n) + (size) - 1, size))      

#define mod_pow2(n, mod)                        \
    ((uptr) (n) & ((mod) - 1))

#define aligned_pow2(n, size)                   \
    (!mod_pow2(n, size))

static inline void no_op(){}
static inline err zero(){return 0;}

static inline bool ptr_overflow(const void *b, cnt off){
    return 0 - (uptr) b < off;
}

char* stpcpy(char *dest, const char *src);
