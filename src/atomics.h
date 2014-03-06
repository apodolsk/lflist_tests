#ifndef __ATOMICS_H__
#define __ATOMICS_H__

#include <stdint.h>
#include <peb_util.h>

/** 
 * @brief Wrapper for the locked version of the x86 xadd instruction.
 *
 * atomically:
 * int ret = *dest;
 * *dest += source;
 * return ret;
 *
 * 
 * @param source The amount by which to increment the contents of dest.
 * @param dest The address of an int which we will increment.
 * 
 * @return The value which dest contained immediately before the
 * increment. 
 */
int xadd(int source, int *dest);

int64_t xchg64b(int64_t src, int64_t *dest);

/** 
 * @brief Wrapper for the x86 cmpxchg instruction.
 *
 * atomically:
 * if(*dest == expected_dest){
 *  *dest = source;
 *  return expected_dest;
 * } else {
 *  return *dest;
 * }
 * @param source The value to save into dest.
 * @param dest The address of an int which we will atomically compare
 * against expected_dest, and then replace with source.
 * @param expected_dest The value which we shall compare with *dest.
 * 
 * @return The initial value of *dest
 */ 
/* Volatile is here to appease the compiler, not because I think it's a
   magical keyword for "atomic". */
int64_t cmpxchg64b(int64_t src, volatile int64_t *dest,
                   int64_t expected_dest);
__int128_t cmpxchg128b(__int128_t src, volatile __int128_t *dest,
                       __int128_t expected_dest);

#define cas(n, addr, old)                                              \
    PUN(typeof(old),                                                   \
        cmpxchg64b(PUN(int64_t, n),                                    \
                   (int64_t *) (addr),                                 \
                   PUN(int64_t, old)))


#define cas2(n, addr, old)                                              \
    PUN(typeof(old),                                                    \
        cmpxchg128b(PUN(int128_t, n),                                   \
                    (int128_t *) (addr),                                \
                    PUN(int128_t, old)))

#define cas_ok(n, addr, old)                    \
    (cmpxchg64b(PUN(int64_t, n),                \
                 (int64_t *) (addr),            \
                 PUN(int64_t, old))             \
     == PUN(int64_t, old))

#define cas2_ok(n, addr, old)                   \
    (cmpxchg128b(PUN(int128_t, n),              \
                 (int128_t *) (addr),           \
                 PUN(int128_t, old))            \
     == PUN(int128_t, old))



#endif  /* __ATOMICS_H__ */
