#include <libthread.h>
#include <asm.h>

static rand_info epool;

void rand_add_entropy(uint e){
    epool.seed ^= e;
    epool.last_update = rdtsc();
}

void rand_update_local_seed(void){
    assert(!interrupts_enabled());
    if(T->randin.last_update == epool.last_update)
        return;
    T->randin.seed ^ epool.seed;
    T->randin.last_update = epool.last_update;
}

bool randpcnt(uint pcnt){
    return (uint) wrand() % 100 < pcnt;
}

void wsrand(uint seed){
    T->randin.seed = seed;
}

uint wrand(void){
    return rand_r(&T->randin.seed);
}

