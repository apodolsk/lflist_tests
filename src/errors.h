#pragma once
#include <peb_util.h>
#include <vip_fun.h>
#include <log.h>

/* Set to 1 to have ERROR macros use GCC's builtin stacktracing. */
#define GCC_STACKTRACE 1

/* Default break, print, and dbg levels. */
#define BRK 4
#define PRNT 4
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

#define EBOOT(fmt, as...)                               \
    ({                                                  \
        elog("Failed to load kernel. %s:%s:%d. " fmt    \
             , __FILE__, __func__, __LINE__, ##as);     \
        BREAK;                                          \
        -1;                                             \
    })

#define EWTF(fmt, as...)                            \
    ({                                              \
        elog("Logic error. %s:%s:%d. " fmt          \
             , __FILE__ ,__func__, __LINE__, ##as); \
        BREAK;                                      \
        -1;                                         \
    })

#define ETODO(fmt, as...)                                   \
    ({                                                      \
        elog("My creator has abandoned me. %s:%s:%d. " fmt  \
             , __FILE__ , __func__ , __LINE__, ##as);       \
        BREAK;                                              \
    })                                            


/* --- Recoverable Errors (for the kernel) --- */

/* Sequel to EARG on the NES. */
#define SUPER_EARG(fmt, as...)                          \
    ({                                                  \
        elog("Really bad input error. %s:%s:%d. "fmt,   \
             __FILE__, __func__, __LINE__, ##as);       \
        _break(1);                                      \
        -1;                                             \
    })

#define OVERCOMMIT_ERROR(fmt, as...)                    \
    ({                                                  \
        elog2("Overcommit error. %s:%s:%d. "            \
              fmt, __FILE__, __func__, __LINE__, ##as); \
        _break(2);                                      \
        -1;                                             \
    })

#define SUPER_RARITY(fmt, as...)                        \
    ({                                                  \
        elog2("Super rare event. %s:%s:%d. "            \
              fmt, __FILE__, __func__, __LINE__, ##as); \
        _break(2);                                      \
        -1;                                             \
    })                                                  \

#define ESYS(fmt, as...)                            \
    ({                                              \
      elog2("System error. %s:%s:%d. " fmt          \
            , __FILE__, __func__, __LINE__, ##as);  \
      _break(3);                                    \
      -1;                                           \
    })                                                         

#define RARITY(fmt, as...)                              \
    ({                                                  \
      elog3("Rarity. %s:%s:%d. "                        \
            fmt, __FILE__, __func__, __LINE__, ##as);   \
      _break(5);                                        \
    })                                                  \
        
#define EARG(fmt, as...)                                \
    ({                                                  \
        elog4("Input error. %s:%s:%d. " fmt             \
              , __FILE__, __func__, __LINE__, ##as);    \
        _break(6);                                      \
        -1;                                             \
    })
      
/* --- Helpers --- */

#define elog(...) _elog(1, __VA_ARGS__)
#define elog2(...) _elog(2, __VA_ARGS__)
#define elog3(...) _elog(3, __VA_ARGS__)
#define elog4(...) _elog(4, __VA_ARGS__)

#define _elog(N, s, ...)                                                \
    do{if(ERR_PRINT_LVL >= N ||                                           \
          (VIP_VERBOSITY >= N && fun_is_vip(__func__)))                 \
            lprintf(s, ##__VA_ARGS__);                                  \
       } while(0)

#define _break(min_break_lvl)                   \
    if(ERR_BREAK_LVL >= min_break_lvl)          \
        BREAK;                                  

/* Prepackaged format strings to be passed to error macros. Intentional
   lack of parens. */
#define KERNPTR_MSG(addr)                                   \
    "Illegal pointer to kernel memory: %p.", (void *) addr

#define BADWRITE_MSG(addr)                      \
    "Unwriteable memory: %p.", (void *) addr

#define BADREAD_MSG(addr)                       \
    "Unreadable memory: %p.", (void *) addr     

#define BADMEM_MSG(addr)                                     \
    "Unreadable or unwriteable memory: %p.", (void *) addr   

