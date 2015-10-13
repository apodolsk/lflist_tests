#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <timing.h>
#include <atomics.h>

extern void profile_report();

cnt nthreads = 100;

/* GDB starts counting threads at 1, so the first child is 2. Urgh. */
const uptr firstborn = 2;

static volatile struct tctxt{
    pthread_t id;
    bool dead;
} *threads;
static sem_t unpauses;
static sem_t pauses;
static pthread_mutex_t thrgrp_state = PTHREAD_MUTEX_INITIALIZER;


void thr_setup(uint id){
    set_dbg_id(id);
    srand(TIME());
    muste(pthread_mutex_lock(&thrgrp_state));
    threads[id - firstborn] = (struct tctxt) {pthread_self()};
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
    if(!cas_won(1, &waiters, (iptr[]){0}))
        return -1;
    muste(pthread_mutex_lock(&thrgrp_state));
    cnt live = 0;
    for(volatile struct tctxt *c = &threads[0]; c != &threads[nthreads]; c++)
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
    for(volatile struct tctxt *c = &threads[0]; c != &threads[nthreads]; c++)
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

void launch_test(void *test(void *)){
    muste(sem_init(&pauses, 0, 0));
    muste(sem_init(&unpauses, 0, 0));
    muste(sigaction(SIGUSR1,
                    &(struct sigaction){.sa_handler=wait_for_universe,
                            .sa_flags=SA_RESTART | SA_NODEFER}, NULL));

    muste(sem_init(&sync_rdy, 0, 0));
    unsynced = nthreads + 1;

    struct tctxt threadscope[nthreads];
    memset(threadscope, 0, sizeof(threadscope));
    threads = threadscope;
    for(uint i = 0; i < nthreads; i++)
        if(pthread_create((pthread_t *) &threads[i].id, NULL,
                          (void *(*)(void*))test,
                          (void *) (firstborn + i)))
            EWTF();

    thr_sync();
    timeval start = get_time();
    for(uint i = 0; i < nthreads; i++)
        pthread_join(threads[i].id, NULL);
    ppl(0, time_diff(start));
    profile_report();
}
