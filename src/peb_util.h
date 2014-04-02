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

#define PUN(t, s)                                               \
    (((union {__typeof__(s) str; t i;}) (s)).i)


/* #define PUN(t, s) (*(t*)(typeof(s)[]){s}) */

#define eq(a, b) (PUN(uptr, a) == PUN(uptr, b))

#define eq2(a, b) (PUN(dptr, a) == PUN(dptr, b))

/* Aladdin system doesn't have librt installed. I'm sick of wrangling with the
   loader. */
#include <time.h>
#define CLOCK_GETTIME(expr)                                             \
    ({                                                                  \
        struct timespec _start;                                       \
        clock_gettime(CLOCK_MONOTONIC, &_start);                      \
        (expr);                                                         \
        struct timespec _end;                                         \
        clock_gettime(CLOCK_MONOTONIC, &_end);                        \
        1000 * (_end.tv_sec - _start.tv_sec) +                      \
            (double) (_end.tv_nsec - _start.tv_nsec) / 1000000.0;   \
    })                                                                  \

#include <time.h>
#include <sys/resource.h>
#define RUSG_GETTIME(expr)                                              \
    ({                                                                  \
       struct rusage r;                                                 \
       getrusage(RUSAGE_SELF, &r);                                      \
       struct timeval _start = r.ru_utime;                            \
       expr;                                                            \
       getrusage(RUSAGE_SELF, &r);                                      \
       struct timeval _end = r.ru_utime;                              \
       1000 * (_end.tv_sec - _start.tv_sec) +                       \
           (double) (_end.tv_usec - _start.tv_usec) / 1000.0;    \
    })                                                                  \

#include <sys/time.h>
#define TOD_GETTIME(expr)                                               \
    ({                                                                  \
       struct timeval _start;                                         \
       gettimeofday(&_start, NULL);                                   \
       expr;                                                            \
       struct timeval _end;                                           \
       gettimeofday(&_end, NULL);                                     \
       1000 * (_end.tv_sec - _start.tv_sec) +                       \
           (double) (_end.tv_usec - _start.tv_usec) / 1000.0;       \
       })


#define GETTIME(expr) TOD_GETTIME(expr)
#define TIME(expr)                                                      \
    do{                                                                 \
        log(#expr ": %f ms", GETTIME((expr)));                          \
    }while(0)                                                           \


#define printf_ln(...)                                                  \
    printf(FIRST_ARG(__VA_ARGS__) "\n" COMMA_AND_TAIL_ARGS(__VA_ARGS__))

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

char *peb_stpcpy(char *dest, const char *src);
void report_err();
void no_op();

