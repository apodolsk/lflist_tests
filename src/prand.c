#include <stdlib.h>
#include <global.h>
#include <fcntl.h>
#include <unistd.h>

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
    /* unsigned long archfs = 1; */
    /* assert(!arch_prctl(ARCH_GET_FS, &archfs)); */
    /* printf("tid:%d, fs:%p, leafs:%p, archfs:%X, &seed:%p\n", */
    /*        itid(), readfs(), leafs(), archfs, &seed); */
    assert(&seed != (void*) 0xffffffffffffffe8);
    return (uint) rand_r(&seed) % 100 <= umin(per_centum, 100);
}

uint prand(){
    return rand_r(&seed);
}
