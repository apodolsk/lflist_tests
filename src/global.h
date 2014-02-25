/**
 * @file   global.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief Global configuration settings. Wrapper file for globally-useful
 * utilities and config settings.
 */

#ifndef GLOBAL_H
#define GLOBAL_H

#define KERNEL

/* This has to be uncommented. A lot of the asserts are really slow. For
   instance they'll cause mem_eat_test to take ages. */
/* #define NDEBUG */

/* Used by assertions and errors instead of panic(). */
extern void _halt(void);
#define BREAK do{while(1) continue;}while(0)
#define lprintf printf_ln

enum {FALSE = 0, TRUE};

/* Timer driver reads this. 9000 is still slow enough for the raw kernel code,
   and I ran it this way for most of development. But logging with interrupts
   off becomes dangerous above 7000. */
/* #define TICKS_PER_SEC 7000 */
#define TICKS_PER_SEC 3000
/* #define TICKS_PER_SEC 100 */

#define RUN_UNIT_TESTS 1
#define ENABLE_POOLS 0
#define HEAP_DBG 0
#define ARENA_DBG 0

/* How often will the frame and memory allocators play dead? And if we let
   them play dead, will we also let them emulate overcommit errors?

   As it turns out, %.2 error rate will make it hard to even
   start a program. */
#define RANDOM_FAIL_PER_THOUSAND 0
#define RANDOMLY_OVERCOMMIT_TOO 0

#define RANDOM_TIMER_SKIP_PERCENT 0

#define RANDOM_PAUSE_PERCENT 0

#define LEAKCHK_ON 1

extern int kernel_booted;

#define UNULL ((uintptr_t) NULL)

#include <stdint.h>
typedef uintptr_t uptr_t;
typedef unsigned int uint;

/* Some of the wrapped utils depend on the config settings here. */
#include <log.h>
#include <pretty_print.h>

#include <stdio.h>
#include <limits.h>

#include <peb_util.h>

#include <pthread.h>

#include <assert.h>
#include <peb_assert.h>
#include <errors.h>
#include <stdbool.h>

#define _gettid() pthread_self()
#define _get_ticks() time(NULL)

typedef __int128_t int128_t;

#endif  /* GLOBAL_H */

