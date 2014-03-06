#ifndef __ATOMICS_H__
#define __ATOMICS_H__

#include <stdint.h>
#include <peb_util.h>
#include <whtypes.h>

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
uptr xadd(uptr src, uptr *dest);
uptr xchg64b(uptr src, uptr *dest);

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
uptr cmpxchg64b(uptr src, volatile uptr *dest, uptr expected_dest);
dblptr cmpxchg128b(dblptr src, volatile dblptr *dest, dblptr expected_dest);

#define cas(n, addr, old)                       \
    PUN(typeof(old),                            \
        cmpxchg64b(PUN(uptr, n),                \
                   (uptr *) (addr),             \
                   PUN(uptr, old)))


#define cas2(n, addr, old)                      \
    PUN(typeof(old),                            \
        cmpxchg128b(PUN(dblptr, n),             \
                    (dblptr *) (addr),          \
                    PUN(dblptr, old)))

#define cas_ok(n, addr, old)                    \
    (cmpxchg64b(PUN(uptr, n),                   \
                (uptr *) (addr),                \
                PUN(uptr, old))                 \
     == PUN(uptr, old))

#define cas2_ok(n, addr, old)                   \
    (cmpxchg128b(PUN(dblptr, n),                \
                 (dblptr *) (addr),             \
                 PUN(dblptr, old))              \
     == PUN(dblptr, old))



#endif  /* __ATOMICS_H__ */
