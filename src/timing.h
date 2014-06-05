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
#define TOD_GETTIME(expr)                                       \
    ({                                                          \
        struct timeval _start;                                  \
        gettimeofday(&_start, NULL);                            \
        expr;                                                   \
        struct timeval _end;                                    \
        gettimeofday(&_end, NULL);                              \
        1000 * (_end.tv_sec - _start.tv_sec) +                  \
            (double) (_end.tv_usec - _start.tv_usec) / 1000.0;  \
    })


#define GETTIME(expr) TOD_GETTIME(expr)
#define TIME(expr)                                                      \
    do{                                                                 \
        double __TIMERET = GETTIME(expr);                                \
        lprintf( #expr ": % ms", __TIMERET);                            \
    }while(0)                                                           \
