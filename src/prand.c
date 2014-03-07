#include <stdlib.h>
#include <global.h>
#include <fcntl.h>
#include <unistd.h>

static __thread uint seed;

void rand_init(void){
    if(read(open("/dev/urandom", O_RDONLY), &seed, sizeof(seed)) !=
       sizeof(seed))
        LOGIC_ERROR();
}

uint randpcnt(uint per_centum){
    return (uint) rand_r(&seed) % 100 <= umin(per_centum, 100);
}
