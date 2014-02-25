/**
 * @file   peb_util.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief  Some globally useful utilities. 
 */

#ifndef __PEB_UTIL_H__
#define __PEB_UTIL_H__

#include <peb_macros.h>

/* Aladdin system doesn't have librt installed. I'm sick of wrangling with the
   loader. */
#include <time.h>
#define CLOCK_GETTIME(expr)                                             \
    ({                                                                  \
        struct timespec __tstart;                                       \
        clock_gettime(CLOCK_MONOTONIC, &__tstart);                      \
        (expr);                                                         \
        struct timespec __tend;                                         \
        clock_gettime(CLOCK_MONOTONIC, &__tend);                        \
        1000 * (__tend.tv_sec - __tstart.tv_sec) +                      \
            (double) (__tend.tv_nsec - __tstart.tv_nsec) / 1000000.0;   \
    })                                                                  \

#include <time.h>
#include <sys/resource.h>
#define RUSG_GETTIME(expr)                                              \
    ({                                                                  \
       struct rusage r;                                                 \
       getrusage(RUSAGE_SELF, &r);                                      \
       struct timeval __tstart = r.ru_utime;                            \
       expr;                                                            \
       getrusage(RUSAGE_SELF, &r);                                      \
       struct timeval __tend = r.ru_utime;                              \
       1000 * (__tend.tv_sec - __tstart.tv_sec) +                       \
           (double) (__tend.tv_usec - __tstart.tv_usec) / 1000.0;    \
    })                                                                  \

#include <sys/time.h>
#define TOD_GETTIME(expr)                                               \
    ({                                                                  \
       struct timeval __tstart;                                         \
       gettimeofday(&__tstart, NULL);                                   \
       expr;                                                            \
       struct timeval __tend;                                           \
       gettimeofday(&__tend, NULL);                                     \
       1000 * (__tend.tv_sec - __tstart.tv_sec) +                       \
           (double) (__tend.tv_usec - __tstart.tv_usec) / 1000.0;       \
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
#define umax(a, b) _umax((uintptr_t) (a),(uintptr_t) (b))
static inline uintptr_t _umax(uintptr_t (a), uintptr_t (b)){
    return a >= b ? a : b;
}
#define umin(a, b) _umin((uintptr_t) (a),(uintptr_t) (b))
static inline uintptr_t _umin(uintptr_t a, uintptr_t b){
    return a < b ? a : b;
}

#define aligned(addr, size)                     \
    (((uintptr_t)(addr) % (size)) == 0)

#define align_down(addr, size)                              \
    ualign_down((uintptr_t) (addr), (size))
static inline uintptr_t ualign_down(uintptr_t addr, size_t size){
    return addr - addr % size;
}

#define align_up(addr, size)                    \
    (void *) ualign_up((uintptr_t) (addr), (size))

static inline uintptr_t ualign_up(uintptr_t addr, size_t size){
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
     (uptr_t) (n) & ~((size) - 1))

#define align_up_pow2(n, size)                  \
    (({assert(is_power_of_2(size));}),          \
     align_down_pow2((uptr_t) (n) + (size) - 1, size))      

#define mod_pow2(n, mod)                        \
    ((uintptr_t) (n) & ((mod) - 1))

#define aligned_pow2(n, size)                   \
    (mod_pow2(n, size) == 0)

extern ptrdiff_t ptrdiff(void *a, void *b);

char *peb_stpcpy(char *dest, const char *src);

typedef char itobsbuf8_t[8 + 1];
char *itobs_8(int num, itobsbuf8_t *bin);
typedef char itobsbuf16_t[16 + 1];
char *itobs_16(int num, itobsbuf16_t *bin);
typedef char itobsbuf32_t[32 + 1];
char *itobs_32(int num, itobsbuf32_t *bin);

void report_err();
void no_op();
int return_neg();
void *return_null();
int return_zero();
int return_zero_rare_event();

#endif /* __PEB_UTIL_H__ */


