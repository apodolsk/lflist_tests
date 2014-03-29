/**
 * @file   peb_util.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief  Some globally useful utilities. 
 */

#ifndef __PEB_UTIL_H__
#define __PEB_UTIL_H__

#include <peb_macros.h>

#define eq(a, b) ({                                 \
            CASSERT(sizeof(a) == sizeof(b));        \
            bool __eqr = false;                     \
            if(sizeof(a) == sizeof(uptr))           \
                __eqr = PUN(uptr, a) == PUN(uptr, b); \
            else if(sizeof(a) == sizeof(dptr))        \
                __eqr = PUN(dptr, a) == PUN(dptr, b);   \
            else                                    \
                assert(0);                          \
            __eqr;                                  \
        })


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


/* COMPILE_ASSERT doesn't like using the ualign_*() functions because they're
   not integer constants.
   DANGER: up to the programmer to make sure that double-eval of addr is safe.*/
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

typedef char itobsbuf8[8 + 1];
char *itobs_8(int num, itobsbuf8 *bin);
typedef char itobsbuf16[16 + 1];
char *itobs_16(int num, itobsbuf16 *bin);
typedef char itobsbuf32[32 + 1];
char *itobs_32(int num, itobsbuf32 *bin);

void report_err();
void no_op();
int return_neg();
void *return_null();
int return_zero();
int return_zero_rare_event();

#endif /* __PEB_UTIL_H__ */


