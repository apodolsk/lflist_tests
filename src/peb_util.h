#pragma once
#include <pumacros.h>

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
static inline void *subtract_if_not_null(void *ptr, size s){
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

/* #define PUN(t, s)                                               \ */
/*     (((union {__typeof__(s) str; t i;}) (s)).i) */

#define PUN(t, s) (*(t*)(typeof(s)[]){s})

#define eq(a, b) ({ typeof(b) __eqa = a; (PUN(uptr, __eqa) == PUN(uptr, b)) })

#define eq2(a, b) ({ typeof(b) __eq2a = a; (PUN(dptr, __eq2a) == PUN(dptr, b)); })

#define is_power_of_2(num)                      \
    (num && !(num & (num - 1)))                 \

static inline int max(int a, int b){
    return a >= b ? a : b;
}
static inline int min(int a, int b){
    return a < b ? a : b;
}
#define umax(a, b) _umax((uptr) (a),(uptr) (b))
static inline uptr _umax(uptr (a), uptr (b)){
    return a >= b ? a : b;
}
#define umin(a, b) _umin((uptr) (a),(uptr) (b))
static inline uptr _umin(uptr a, uptr b){
    return a < b ? a : b;
}

#define aligned(addr, size)                     \
    (((uptr)(addr) % (size)) == 0)

#define align_down(addr, size)                              \
    ualign_down((uptr) (addr), (size))
static inline uptr ualign_down(uptr addr, size size){
    return addr - addr % size;
}

#define align_up(addr, size)                    \
    (void *) ualign_up((uptr) (addr), (size))

static inline uptr ualign_up(uptr addr, size size){
    return ualign_down(addr + size - 1, size);
}


#define const_align_down_pow2(n, size)          \
    ((n) & ~((size) - 1))

#define const_align_up_pow2(n, size)         \
    (const_align_down_pow2((n) + (size) - 1, size))      

#define align_down_pow2(n, size)       \
    (({assert(is_power_of_2(size));}), \
     (uptr) (n) & ~((size) - 1))

#define align_up_pow2(n, size)                  \
    (({assert(is_power_of_2(size));}),          \
     align_down_pow2((uptr) (n) + (size) - 1, size))      

#define mod_pow2(n, mod)                        \
    ((uptr) (n) & ((mod) - 1))

#define aligned_pow2(n, size)                   \
    (mod_pow2(n, size) == 0)

static inline void no_op(){
}
