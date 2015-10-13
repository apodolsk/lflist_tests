#include <time.h>
#define CLOCK_TIME(expr)                                             \
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
#define RUSG_TIME(expr)                                              \
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
#define TOD_TIME(expr)                                       \
    ({                                                          \
        struct timeval _start;                                  \
        gettimeofday(&_start, NULL);                            \
        expr;                                                   \
        struct timeval _end;                                    \
        gettimeofday(&_end, NULL);                              \
        1000 * (_end.tv_sec - _start.tv_sec) +                  \
            (double) (_end.tv_usec - _start.tv_usec) / 1000.0;  \
    })

#define TIME(expr) TOD_TIME(expr)
#define REPORT_TIME(expr)                                                      \
    do{                                                                 \
        double __TIMERET = TIME(expr);                                \
        lprintf( #expr ": % ms", __TIMERET);                            \
    }while(0)                                                           

typedef struct timeval timeval;

static inline timeval get_time(void){
    struct timeval start;
    gettimeofday(&start, NULL);
    return start;
}

static inline double time_diff(timeval start){
    struct timeval end;
    gettimeofday(&end, NULL);
    return 1000 * (end.tv_sec - start.tv_sec) +                      
           (double) (end.tv_usec - start.tv_usec) / 1000.0;      
    
}
