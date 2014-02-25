/**
 * @file   asm_util.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief  Function prototypes for a variety of miscellanious
 * x86 assembly helper functions. 
 *
 * @note I'm unsure of the best way to prototype these functions, which
 * assume a certain operand size. It's cleaner to prototype the wrapper args
 * as "int", so that you keep the real operand size isolated inside the asm
 * files and can switch out just the asm if the size of "int" ever
 * changes. But then I think it might be even more useful to tie the C code
 * to a certain representation, consequently forcing a compiler error if the
 * representation ever changes - ie. force programmers to reconsider all the
 * places where a change in representation could *possibly* have an
 * unintended effect. Does this seem reasonable? 
 * 
 */

#ifndef __ASM_UTIL_H__
#define __ASM_UTIL_H__

#include <stdint.h>

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

#endif  /* __ASM_UTIL_H__ */
