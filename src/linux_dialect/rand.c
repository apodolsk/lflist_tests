#include <thread.h>

int rand(void){
    return rand_r(&T->randin.seed);
}

void srand(uint seed){
    T->randin.seed ^= seed;
}
