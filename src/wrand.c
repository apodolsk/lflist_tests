#include <stdlib.h>
#include <global.h>
#include <fcntl.h>
#include <unistd.h>
#include <peb_util.h>

static __thread uint seed;

void prand_init(void){
    if(read(open("/dev/urandom", O_RDONLY), &seed, sizeof(seed)) !=
       sizeof(seed))
        EWTF();
}

#include <asm/prctl.h>
#include <sys/prctl.h>
extern int arch_prctl(int code, unsigned long *addr);
uint prandpcnt(uint per_centum){
    assert(&seed != (void*) 0xffffffffffffffe8);
    return (uint) rand_r(&seed) % 100 <= per_centum;
}

uint prand(){
    return rand_r(&seed);
}
