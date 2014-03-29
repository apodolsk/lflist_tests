/**
 * @file   global.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief Global configuration settings. Wrapper file for globally-useful
 * utilities and config settings.
 */

#ifndef GLOBAL_H
#define GLOBAL_H

/* This has to be uncommented. A lot of the asserts are really slow. For
   instance they'll cause mem_eatest to take ages. */
/* #define NDEBUG */

/* Used by assertions and errors instead of panic(). */
extern void _halt(void);
#include <stdlib.h>
#define BREAK abort();
/* #define BREAK do{while(1) continue;}while(0) */
#define lprintf(s, ...) printf(s"\n", ##__VA_ARGS__)


/* Timer driver reads this. 9000 is still slow enough for the raw kernel code,
   and I ran it this way for most of development. But logging with interrupts
   off becomes dangerous above 7000. */
/* #define TICKS_PER_SEC 7000 */
#define TICKS_PER_SEC 3000
/* #define TICKS_PER_SEC 100 */

#define ATOMIC_FUZZ 1000000
#define ATOMIC_FUZZ_PCNT 30
#define RUN_UNITESTS 1
#define ENABLE_POOLS 0
#define HEAP_DBG 0
#define ARENA_DBG 0

/* How often will the frame and memory allocators play dead? And if we let
   them play dead, will we also let them emulate overcommit errors?

   As it turns out, %.2 error rate will make it hard to even
   start a program. */
#define RANDOM_FAIL_PERHOUSAND 0
#define RANDOMLY_OVERCOMMITOO 0

#define RANDOMIMER_SKIP_PERCENT 0

#define RANDOM_PAUSE_PERCENT 0

#define LEAKCHK_ON 1

extern int kernel_booted;

#define UNULL ((uptr) NULL)

#include <stdint.h>
typedef uptr uptr;
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
#include <whtypes.h>

extern __thread uint tid_;
#define _gettid() tid_
#define _get_ticks() time(NULL)

#define CACHE_SIZE 64
#define PAGE_SIZE 4096

#define C pthread_self()

#endif  /* GLOBAL_H */

