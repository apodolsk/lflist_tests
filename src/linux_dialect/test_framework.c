#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <timing.h>
#include <atomics.h>

cnt nthreads = 100;

/* GDB starts counting threads at 1, so the first child is 2. Urgh. */
const uptr firstborn = 2;

static volatile struct test_thread{
    pthread_t id;
    bool dead;
} *threads;
typedef volatile struct test_thread test_thread;

static sem_t unpauses;
static sem_t pauses;
static pthread_mutex_t thrgrp_state = PTHREAD_MUTEX_INITIALIZER;


void thr_setup(uint id){
    set_dbg_id(id);
    srand(wall_ns());
    muste(pthread_mutex_lock(&thrgrp_state));
    threads[id - firstborn] = (test_thread) {pthread_self()};
    muste(pthread_mutex_unlock(&thrgrp_state));
}

void thr_destroy(uint id){
    muste(pthread_mutex_lock(&thrgrp_state));
    threads[id - firstborn].dead = true;
    muste(pthread_mutex_unlock(&thrgrp_state));    
}

static volatile cnt unsynced;
static sem_t sync_rdy;

void thr_sync(void){
    if(xadd(-1, &unsynced) != 1){
        sem_wait(&sync_rdy);
        return;
    }

    assert(!unsynced);
    unsynced = nthreads + 1;
        
    for(cnt i = 0; i < nthreads ; i++)
        sem_post(&sync_rdy);
}

uptr waiters;
err pause_universe(void){
    assert(waiters < nthreads);
    if(cas(1, &waiters, 0))
        return -1;
                  
    muste(pthread_mutex_lock(&thrgrp_state));
    cnt live = 0;
    for(test_thread *c = &threads[0]; c != &threads[nthreads]; c++)
        if(!c->dead && c->id != pthread_self()){
            live++;
            pthread_kill(c->id, SIGUSR1);
        }
    waiters += live;
    muste(pthread_mutex_unlock(&thrgrp_state));
    
    for(uint i = 0; i < live; i++)
        muste(sem_wait(&pauses));
    return 0;
}

void resume_universe(void){
    cnt live = 0;
    for(test_thread *c = &threads[0]; c != &threads[nthreads]; c++)
        if(!c->dead && c->id != pthread_self())
            live++;
    assert(live == waiters - 1);
    for(cnt i = 0; i < live; i++)
        muste(sem_post(&unpauses));
    xadd(-1, &waiters);
}

void wait_for_universe(){
    muste(sem_post(&pauses));
    muste(sem_wait(&unpauses));
    xadd(-1, &waiters);
}

static void *(*test)(void *);

static void launch_test_thread(dbg_id i){
    thr_setup(i);
    test((void *) i);
    thr_destroy(i);
}

void launch_test(void *t(void *), const char *test_name){
    muste(sem_init(&pauses, 0, 0));
    muste(sem_init(&unpauses, 0, 0));
    muste(sigaction(SIGUSR1,
                    &(struct sigaction){.sa_handler=wait_for_universe,
                            .sa_flags=SA_RESTART | SA_NODEFER}, NULL));

    muste(sem_init(&sync_rdy, 0, 0));
    unsynced = nthreads + 1;

    struct test_thread threadscope[nthreads];
    memset(threadscope, 0, sizeof(threadscope));
    threads = threadscope;
    test = t;
    
    for(uint i = 0; i < nthreads; i++)
        muste(pthread_create((pthread_t *) &threads[i].id, NULL,
                             (void *(*)(void*))launch_test_thread,
                             (void *) (firstborn + i)));

    struct timespec start = job_get_time();
    thr_sync();
    for(uint i = 0; i < nthreads; i++)
        pthread_join(threads[i].id, NULL);
    ppl(0, test_name, (iptr) job_time_diff(start));
}

