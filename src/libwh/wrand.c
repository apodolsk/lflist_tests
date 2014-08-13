#include <libthread.h>
#include <asm.h>

static rand_info epool;

void rand_add_entropy(uint e){
    epool.seed ^= e;
    epool.last_update = rdtsc();
}

void rand_update_local_seed(void){
    if(T->randin.last_update == epool.last_update)
        return;
    T->randin.seed ^ epool.seed;
}

bool randpcnt(uint pcnt){
    return (uint) rand() % 100 < pcnt;
}

