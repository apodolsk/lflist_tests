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
typedef uint8_t u8;
typedef ptrdiff_t ptrdiff;
typedef int err;

#define WORDBITS (8 * sizeof(void *))

#endif
