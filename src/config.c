#include <sys/mman.h>
#include <nalloc.h>

struct slab *new_slabs(cnt batch){
    struct slab *s = mmap(NULL, SLAB_SIZE * batch, PROT_WRITE,
                          MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    static cnt realigns = 0;
    if(!aligned_pow2(s, SLAB_SIZE)){
        assert(!(realigns++));
        munmap(s, SLAB_SIZE * batch);
        s = mmap(NULL, SLAB_SIZE + SLAB_SIZE * batch, PROT_WRITE,
                 MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
        if(s == MAP_FAILED)
            return NULL;
        slab *sa = align_up_pow2(s, SLAB_SIZE);
        munmap(s, SLAB_SIZE * (sa - s));
        s = sa;
    }
    return s == MAP_FAILED ? EWTF(), NULL : s;
}

#include <pthread.h>
#include <thread.h>
__thread thread manual_tls;

struct thread *this_thread(void){
    return (struct thread *) pthread_self();
}

__thread uint _dbg_id;
dbg_id get_dbg_id(void){
    return _dbg_id;
}

void set_dbg_id(uint id){
    _dbg_id = id;
}

#include <timing.h>
u64 rdtsc(void){
    return (u64) GETTIME();
}

bool interrupts_enabled(void){
    return true;
}

err kyield(tid t){
    assert(t == -1);
    pthread_yield();
    return 0;
}

void *heap_start(){
    extern void *end;
    return &end;
}

/* TODO: /proc/self/nonsense */
void *heap_end(){
    return (void *) 0x0101010101010101;
}

#include <stdio.h>
inline
void breakpoint(void){
    fflush(stdout);
    asm volatile("int $3;");
}

void panic(const char *_, ...){
    breakpoint();
    abort();
}
