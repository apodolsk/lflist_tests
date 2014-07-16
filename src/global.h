#pragma once

#ifndef __ASSEMBLER__

#define ainline __attribute__((always_inline))
#define pure __attribute__((pure))
#define constfun __attribute__((const))
#define noreturn __attribute__((noreturn))
#define packed __attribute__((packed))
#define align(ment) __attribute__((__aligned__(ment)))
#define checked __attribute__((warn_unused_result))

#include <string.h>

#include <whtypes.h>
#include <limits.h>

#include <config.h>
#include <errors.h>
#include <peb_assert.h>
#include <log.h>
#include <peb_util.h>

#include <syscall.h>

#endif
