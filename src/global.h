#pragma once

#ifndef __ASSEMBLER__

#define ainline __attribute__((always_inline))
#define pure __attribute__((pure))
#define constfun __attribute__((const))
#define noreturn __attribute__((noreturn))

#include <whtypes.h>
#include <limits.h>

#include <config.h>
#include <errors.h>
#include <peb_assert.h>
#include <log.h>

#endif
