#pragma once
#include <pumacros.h>
#include <config.h>

/* Set to 1 to have ERROR macros use GCC's builtin stacktracing. */
#define GCC_STACKTRACE 1

/* Default break, print, and dbg levels. */
#define BRK 4
#define PRNT 3
#define DBG 1

#define E_NALLOC DBG, BRK, PRNT
#define E_ERRORS DBG, BRK, PRNT

#define E_MAIN DBG, BRK, PRNT
#define E_PEBRAND DBG, BRK, PRNT

#define E_ATOMICS DBG, BRK, PRNT
#define E_LISTM DBG, BRK, PRNT
#define E_LFLISTM DBG, BRK, PRNT
#define E_STACKM DBG, BRK, PRNT

#define E_KMALLOC DBG, BRK, PRNT
#define E_UNITESTS DBG, BRK, PRNT
#define E_LIST_TESTS DBG, BRK, PRNT

#define LOOKUP CONCAT(E_, MODULE)

#if NUM_ARGS(LOOKUP) != 3
#undef LOOKUP
#define LOOKUP DBG, BRK, PRNT
#endif

#define FIRST(as...) _FIRST(as)
#define SECOND(as...) _SECOND(as)
#define THIRD(as...) _THIRD(as)
#define _FIRST(a, ...) 
#define _SECOND(a, b, ...) b
#define _THIRD(a, b, c) c

#define DBG_LVL DBG
#define ERR_BREAK_LVL BRK
#define ERR_PRINT_LVL PRNT

/* #define DBG_LVL FIRST(LOOKUP) */
/* #define ERR_BREAK_LVL SECOND(LOOKUP) */
/* #define ERR_PRINT_LVL THIRD(LOOKUP) */

/* --- Fatal Errors (for the kernel) --- */

#define EWTF(fmt, as...)                            \
    ({                                              \
        elog(1, "This can't be. %:%:%. " fmt     \
             , __FILE__ ,__func__, __LINE__, ##as); \
        BREAK();                                    \
        -1;                                         \
    })

#define TODO(fmt, as...)                                        \
    ({                                                          \
        elog(1, "My creator has abandoned me. %:%:%. " fmt   \
             , __FILE__ , __func__ , __LINE__, ##as);           \
        BREAK();                                                \
    })                                            


/* --- Recoverable Errors (for the kernel) --- */

/* Sequel to EARG on the NES. */
#define SUPER_EARG(fmt, as...)                          \
    ({                                                  \
        elog(1, "Super bad input error. %:%:%. "fmt, \
             __FILE__, __func__, __LINE__, ##as);       \
        ebreak(1);                                      \
        -1;                                             \
    })

#define OVERCOMMIT_ERROR(fmt, as...)                    \
    ({                                                  \
        elog(2,"Overcommit error. %:%:%. "            \
              fmt, __FILE__, __func__, __LINE__, ##as); \
        ebreak(2);                                      \
        -1;                                             \
    })

#define SUPER_RARITY(fmt, as...)                        \
    ({                                                  \
        elog(2,"Super rare event. %:%:%. "            \
              fmt, __FILE__, __func__, __LINE__, ##as); \
        ebreak(2);                                      \
        -1;                                             \
    })                                                  \

#define EOOR(fmt, as...)                            \
    ({                                              \
        elog(2,"Out of resources. %:%:%. " fmt   \
            , __FILE__, __func__, __LINE__, ##as);  \
      ebreak(3);                                    \
      -1;                                           \
    })                                                         

#define RARITY(fmt, as...)                              \
    ({                                                  \
      elog(3, "Rarity. %:%:%. "                        \
            fmt, __FILE__, __func__, __LINE__, ##as);   \
      ebreak(5);                                        \
    })                                                  \
        
#define EARG(fmt, as...)                                \
    ({                                                  \
        elog(4, "Input error. %:%:%. " fmt           \
              , __FILE__, __func__, __LINE__, ##as);    \
        ebreak(6);                                      \
        -1;                                             \
    })
      
/* --- Helpers --- */

#define elog(verb, fmt, ...)                                    \
    ((ERR_PRINT_LVL >= verb) ? lprintf(fmt, ##__VA_ARGS__) : 0)

#define ebreak(min_break_lvl)                   \
    ((ERR_BREAK_LVL >= min_break_lvl) ? BREAK() : 0)

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

