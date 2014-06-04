#include <sys/mman.h>
#include <nalloc.h>
struct slab *new_slabs(cnt batch){
    struct slab *s = mmap(NULL, SLAB_SIZE * batch, PROT_WRITE | PROT_WRITE,
                   MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    return s == MAP_FAILED ? EWTF(), NULL : s;
}
__thread static uint tid;
uint itid(void){
    return tid_;
}
