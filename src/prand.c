#include <stdlib.h>
#include <global.h>
#include <fcntl.h>
#include <unistd.h>

static __thread uint seed;

void rand_init(void){
    if(read(open("/dev/urandom", O_RDONLY), &seed, sizeof(seed)) !=
       sizeof(seed))
        EWTF();
}

extern void *readfs();
uint randpcnt(uint per_centum){
    assert(&seed != (void*) 0xffffffffffffffe8); 
    return (uint) rand_r(&seed) % 100 <= umin(per_centum, 100);
}
