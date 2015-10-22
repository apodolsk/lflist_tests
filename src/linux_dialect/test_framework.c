#define MODULE TESTM

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <timing.h>
#include <atomics.h>
#include <test_framework.h>

cnt nthreads = 100;

volatile pthread_t *threads;
pthread_t invalid;

struct syncx{
    volatile cnt expected;
    volatile cnt unsynced;
    sem_t synced;
};
typedef struct syncx syncx;

void syncx_init(syncx *x, cnt expected){
    *x = (syncx){expected, expected};
    muste(sem_init(&x->synced, 0, 0));
}

void (thr_sync)(syncx *x){
    if(xadd(-1, &x->unsynced) != 1)
        sem_wait(&x->synced);
    else
        for(cnt i = 0; i < x->expected - 1 ; i++)
            sem_post(&x->synced);
}

static volatile uptr pausing;
static volatile cnt sigs;
static sem_t paused;
static sem_t unpaused;

err (pause_universe)(void){
    if(cas(1, &pausing, 0))
        return -1;

    sigs = 0;
    for(cnt i = 0; i < nthreads; i++){
        pthread_t t =  threads[i];
        if(!pthread_equal(t, invalid) && !pthread_equal(t, pthread_self())){
            sigs++;
            muste(pthread_kill(t, SIGUSR1));
        }
    }
    pausing = sigs;
    
    for(cnt i = 0; i < sigs; i++)
        muste(sem_wait(&paused));
    return 0;
}

void resume_universe(void){
    for(cnt i = 0; i < sigs; i++)
        muste(sem_post(&unpaused));
}

void wait_for_universe(){
    muste(sem_post(&paused));
    muste(sem_wait(&unpaused));
    /* Without the xadd, another pause_universe could signal after post
       but before wait. Then, a wait_for_universe could exit
       sem_wait(&unpaused) before resume_universe is called, since it
       still has an "unconsumed" post from this previous pause. */
    xadd(-1, &pausing);
}


syncx terminal_sync;
static void *(*test)(void *);
static syncx universe_ready;

static void launch_test_thread(dbg_id i){
    set_dbg_id(firstborn + i);
    srand(wall_ns());
    thr_sync(&universe_ready);

    test((void *) (firstborn + i));

    threads[i] = invalid;
    /* TODO: Bad way to make sure that pause_universe doesn't signal a
    thread "after the end of its lifetime." Could use a mutex or
    rwlock. But I want pause_universe to happen ASAP, and anyway locking
    doesn't rule out a valid signal being deferred till it becomes
    erroneous. Sync does. */
    thr_sync(&terminal_sync);
}

void launch_test(void *t(void *), const char *test_name){
    muste(sem_init(&paused, 0, 0));
    muste(sem_init(&unpaused, 0, 0));
    muste(sigaction(SIGUSR1,
                    &(struct sigaction){.sa_handler=wait_for_universe,
                            .sa_flags=SA_RESTART | SA_NODEFER}, NULL));

    start_timing = &(syncx){};
    syncx_init(start_timing, nthreads + 1);
    stop_timing = &(syncx){};
    syncx_init(stop_timing, nthreads + 1);
    
    syncx_init(&terminal_sync, nthreads);
    syncx_init(&universe_ready, nthreads);
    
    test = t;

    pthread_t threadscope[nthreads];
    threads = threadscope;
    invalid = pthread_self();
    
    for(cnt i = 0; i < nthreads; i++)
        muste(pthread_create((pthread_t *) &threads[i], NULL,
                             (void *(*)(void*))launch_test_thread,
                             (void *) (uptr) (i)));

    thr_sync(start_timing);
    struct timespec start = job_get_time();
    thr_sync(stop_timing);
    ppl(0, test_name, (iptr) job_time_diff(start));
    
    for(cnt i = 0; i < nthreads; i++)
        pthread_join(threads[i], NULL);

    /* TODO: bit hacky */
    pausing = true;
}
