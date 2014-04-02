#pragma once
#ifndef __ASSEMBLER__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef unsigned int uint;
typedef uintptr_t uptr;
typedef uptr cnt;
typedef intptr_t iptr;
typedef __int128_t dptr;
typedef size_t size;
typedef ptrdiff_t ptrdiff;
typedef int err;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef __uint128_t u128;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef __int128_t i128;

#define WORDBITS (8 * sizeof(void *))

#endif
