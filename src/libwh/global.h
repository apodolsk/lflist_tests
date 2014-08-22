#pragma once

#ifndef __ASSEMBLER__

#define ainline __attribute__((always_inline))
#define noinline __attribute__((noinline))
#define pure __attribute__((pure))
#define constfun __attribute__((const))
#define noreturn __attribute__((noreturn))
#define packed __attribute__((packed))
#define align(ment) __attribute__((__aligned__(ment)))
#define checked __attribute__((warn_unused_result))

#include <whtypes.h>
#include <limits.h>

#include <string.h>
#include <stdio.h>
#include <pustr.h>
#include <config.h>
#include <log.h>

#include <errors.h>
#include <peb_util.h>
#include <wrand.h>

#include <syscall.h>

#include <stdlib.h>

#endif
