#pragma once

/* Used by assertions and errors instead of panic(). */
/* #define BREAK asm volatile ("int3") */
#define BREAK __assert_fail("", "", 0, "");
#define lprintf(s, ...) printf("T:%d " s "\n", _gettid(), ##__VA_ARGS__)


/* Timer driver reads this. 9000 is still slow enough for the raw kernel
   code, and I ran it this way for most of development. But logging with
   interrupts off becomes dangerous above 7000. */
/* #define TICKS_PER_SEC 7000 */
#define TICKS_PER_SEC 3000
/* #define TICKS_PER_SEC 100 */

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

#define UNULL ((uptr) NULL)

/* Some of the wrapped utils depend on the config settings here. */
#include <log.h>
#include <stdio.h>
#include <peb_util.h>
#include <pthread.h>
#include <errors.h>
#include <peb_assert.h>
#include <stdbool.h>

extern __thread uint tid_;
#define _gettid() tid_
#define _get_ticks() time(NULL)

#define CACHE_SIZE 64
#define PAGE_SIZE 4096

#define C pthread_self()


