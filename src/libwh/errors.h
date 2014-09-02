#pragma once
#include <pumacros.h>
#include <config.h>
#include <errverbs.h>
#include <assert.h>

/* Default break, print, and dbg levels. */

#define LVL_EWTF 0
#define LVL_TODO 1
#define LVL_SUPER_EARG 1
#define LVL_OVERCOMMIT 2
#define LVL_SUPER_RARITY 2
#define LVL_EOOR 3
#define LVL_EARG 4
#define LVL_RARITY 5

#define FIRST(as...) _FIRST(as)
#define SECOND(as...) _SECOND(as)
#define THIRD(as...) _THIRD(as)
#define _FIRST(a, ...) a
#define _SECOND(a, b, ...) b
#define _THIRD(a, b, c) c

#define LOOKUP _LOOKUP(CONCAT(E_, MODULE))
#define _LOOKUP(mod) CONCAT2(_LOOKUP, NUM_ARGS(mod))(mod)
#define _LOOKUP1(mod) DBG, BRK, PRNT
#define _LOOKUP3(a, b, c) a, b, c

#define E_DBG_LVL FIRST(LOOKUP)
#define E_BREAK_LVL SECOND(LOOKUP)
#define E_PRINT_LVL THIRD(LOOKUP)

#ifdef assert
#undef assert
#endif

extern noreturn void panic(const char *, ...);

#define assert(expr...) CONCAT3(_assert, E_DBG_LVL) (expr)
#define _assert1(expr...) (!(expr) ? TODO("Failed assertion: %.", #expr), 1 : 1)
#define _assert0(expr...) (0 ? expr : 0)

/* --- Fatal Errors (for the kernel) --- */

#define EWTF(fmt, as...)                                \
    ({                                                  \
        elog(0, "This can't be. %:%:%. " fmt    \
             , __FILE__ ,__func__, __LINE__, ##as);     \
        ebreakpoint(0);                                      \
        panic("WTF!");                                  \
    })

#define TODO(fmt, as...)                                            \
    ({                                                              \
        elog(0, "My creator has abandoned me. %:%:%. " fmt  \
             , __FILE__ , __func__ , __LINE__, ##as);               \
        ebreakpoint(0);                                             \
    })                                            


/* --- Recoverable Errors (for the kernel) --- */

/* Sequel to EARG on the NES. */
#define SUPER_EARG(fmt, as...)                          \
    ({                                                  \
        elog(1, "Super bad input error. %:%:%. "fmt,    \
             __FILE__, __func__, __LINE__, ##as);       \
        ebreakpoint(1);                                 \
        (dptr) -1;                                       \
    })

#define OVERCOMMIT_ERROR(fmt, as...)                    \
    ({                                                  \
        elog(2,"Overcommit error. %:%:%. "      \
             fmt, __FILE__, __func__, __LINE__, ##as);  \
        ebreakpoint(2);                                      \
        (dptr) -1;                                            \
    })

#define SUPER_RARITY(fmt, as...)                        \
    ({                                                  \
        elog(2,"Super rare event. %:%:%. "              \
             fmt, __FILE__, __func__, __LINE__, ##as);  \
        ebreakpoint(2);                                 \
        (dptr) -1;                                      \
    })                                                  \

#define EOOR(fmt, as...)                            \
    ({                                              \
        elog(3,"Out of resources. %:%:%. " fmt      \
             , __FILE__, __func__, __LINE__, ##as); \
        ebreakpoint(3);                             \
        (dptr) -1;                                  \
    })                                                         

        
#define EARG(fmt, as...)                            \
    ({                                              \
        elog(4, "Input error. %:%:%. " fmt          \
             , __FILE__, __func__, __LINE__, ##as); \
        ebreakpoint(4);                             \
        (dptr) -1;                                  \
    })

#define RARITY(fmt, as...)                              \
    ({                                                  \
        elog(5, "Rarity. %:%:%. "               \
             fmt, __FILE__, __func__, __LINE__, ##as);  \
        ebreakpoint(5);                                      \
    })                                                  \

/* --- Helpers --- */

#define elog(lvl, fmt, ...)                                     \
    ((E_PRINT_LVL >= lvl) ? lprintf(fmt, ##__VA_ARGS__) : 0)

#define ebreakpoint(lvl)                             \
    ((E_BREAK_LVL >= lvl) ? breakpoint() : 0)

/* TODO: macro-expansion into multiple macro args not happening. Same old
   shit. */
#define KERNPTR_MSG(addr)                                   \
    /* "Forbidden pointer to kern memory: %", (void *) addr */

#define BADWRITE_MSG(addr)                      \
    /* "Failed to write to %", (void *) addr */

#define BADREAD_MSG(addr)                       \
    /* "Failed to read from: %", (void *) addr      */

#define BADMEM_MSG(addr)                                     \
    /* "Unreadable or unwriteable memory: %.", (void *) addr    */

