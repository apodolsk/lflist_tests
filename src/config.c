#include <sys/mman.h>
#include <nalloc.h>

struct slab *new_slabs(cnt batch){
    struct slab *s = mmap(NULL, SLAB_SIZE * batch, PROT_WRITE | PROT_WRITE,
                   MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    return s == MAP_FAILED ? EWTF(), NULL : s;
}

#include <pthread.h>
#include <libthread.h>
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
