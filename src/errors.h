/**
 * @file   errors.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief Print error info and report it to users. Maybe BREAK along the
 * way.
 *
 * A thread_t's err_info_t saves info about the last kernel error that it
 * encountered. Users have read-only access to it through the last_errinfo
 * syscall. It consists of a vague error code and, in dbg mode, a stack
 * trace. Hypothetically, it would also have a copy of the strings that get
 * printed by the ERROR macros.
 *
 * Programs can always read last_errinfo to find out why the last syscall
 * failed.
 *
 * This causes trouble for syscall overcommit swexn handlers. errinfo is set
 * as if the syscall got an invalid address, and handlers need to hide that if
 * they want to emulate syscall restart. For now, userspace wrappers can do
 * that. I'm thinking about giving users RW access to last_errinfo in order to
 * deal with this.
 *
 * The error code is vague because I don't need anything more. All that it
 * tells you is "is this error my fault or not?". But maybe it could work,
 * along with the hypothetical string. scanf()'ing would be annoying, but
 * maybe if the annoyance is an issue then you don't really need more than the
 * vague error code?
 * 
 * Likewise, no one needs the string, so I haven't implemented it. The naive
 * way would require a buffer as big as a thread_t itself. But NUM_ARGS makes
 * it easy to marshal printf args. The only issue is that you'd probably still
 * have to read format strings in order to identify pointers.
 *
 * @todo It's time to give each error type its own verbosity setting.
 */

#ifndef __PEB_ERRORS_H__
#define __PEB_ERRORS_H__

#include <peb_macros.h>
#include <log.h>
#include <vip_fun.h>

/* System interface settings. */

#define e_printf(...) lprintf(__VA_ARGS__)
#define e_gettid() _gettid()
#define e_get_ticks() _get_ticks()

/* Set to 1 to have ERROR macros use GCC's builtin stacktracing. */
#define GCC_STACKTRACE 1

/* Default break, print, and dbg levels, so that you don't have to bother
   with per-module settings if you don't want to. */
#define BRK 1
#define PRNT 1
#define DBG 0

#define E_NALLOC DBG, BRK, 2
#define E_STACK 1, BRK, PRNT
#define E_UNIT_TESTS 3, BRK, 3

/* #ifndef MODULE */
/* #define MODULE MAIN */
/* #endif */

#define LOOKUP CONCAT(E_, MODULE)

#if NUM_ARGS(LOOKUP) != 3
#undef LOOKUP
#define LOOKUP 0, 0, 0
#endif

#define DBG_LVL FIRST_ARG(LOOKUP)
#define ERR_BREAK_LVL SECOND_ARG(LOOKUP)
#define ERR_PRINT_LVL THIRD_ARG(LOOKUP)

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

/* --- Fatal Errors (for the kernel) --- */

/** 
 * @brief Append the given optional printf-style arguments to a generic
 * error message. Then BREAK.
 *
 * FIRST_ARG might expand to nothing, to a plain string, or to a format
 * string.
 *
 * COMMA_AND_TAIL_ARGS might expand to nothing, or to a comma followed by the
 * "helper" arguments of a format string.
 */

#define INIT_ERROR(...)                                                 \
    ({                                                                  \
        elog("Failed to load kernel. %s:%s:%d. " FIRST_ARG(__VA_ARGS__) \
             , __FILE__                                                 \
             , __func__                                                 \
             , __LINE__                                                 \
             COMMA_AND_TAIL_ARGS(__VA_ARGS__));                         \
        BREAK;                                                          \
        -1;                                                             \
    })

#define LOGIC_ERROR(...)                                        \
    ({                                                          \
        elog("Logic error. %s:%s:%d. " FIRST_ARG(__VA_ARGS__)   \
             , __FILE__                                         \
             , __func__                                         \
             , __LINE__                                         \
             COMMA_AND_TAIL_ARGS(__VA_ARGS__));                 \
        BREAK;                                                  \
        -1;                                                     \
    })

#define UNIMPLEMENTED(...)                                              \
    ({                                                                  \
        elog("My creator has abandoned me. %s:%s:%d. " FIRST_ARG(__VA_ARGS__) \
             , __FILE__                                                 \
             , __func__                                                 \
             , __LINE__                                                 \
             COMMA_AND_TAIL_ARGS(__VA_ARGS__));                         \
        BREAK;                                                          \
    })                                            


/* --- Recoverable Errors (for the kernel) --- */

/* Sequel to INPUT_ERROR on the NES. */
#define SUPER_INPUT_ERROR(...)                      \
    ({                                              \
        set_errinfo(EINPUT);                        \
        elog("Really bad input error. %s:%s:%d. "   \
             FIRST_ARG(__VA_ARGS__)""               \
             , __FILE__                             \
             , __func__                             \
             , __LINE__                             \
             COMMA_AND_TAIL_ARGS(__VA_ARGS__));     \
        _break(1);                                  \
        -1;                                         \
     })

/** 
 * Kernel couldn't resolve an OOM or ZFOD page fault.
 */
#define OVERCOMMIT_ERROR(...)                       \
    ({                                              \
        elog2("Overcommit error. %s:%s:%d. "        \
              FIRST_ARG(__VA_ARGS__)                \
              , __FILE__                            \
              , __func__                            \
              , __LINE__                            \
              COMMA_AND_TAIL_ARGS(__VA_ARGS__));    \
        _break(2);                                  \
        -1;                                         \
    })

/* Something legal but fascinating happened. */
#define SUPER_RARE_EVENT(...)                               \
    ({                                                      \
        elog2("Super rare event. %s:%s:%d. "                \
              FIRST_ARG(__VA_ARGS__)""                      \
              , __FILE__                                    \
              , __func__                                    \
              , __LINE__                                    \
              COMMA_AND_TAIL_ARGS(__VA_ARGS__));            \
        _break(2);                                          \
        -1;                                                 \
    })                                                      \

/* Kernel's out of resources. */
#define SYSTEM_ERROR(...)                                       \
    ({                                                          \
        elog2("System error. %s:%s:%d. " FIRST_ARG(__VA_ARGS__) \
              , __FILE__                                        \
              , __func__                                        \
              , __LINE__                                        \
              COMMA_AND_TAIL_ARGS(__VA_ARGS__));                \
        _break(3);                                              \
        -1;                                                     \
    })                                                         

#define RARE_EVENT(...)                                     \
    ({                                                      \
        elog3("Rare event. %s:%s:%d. "                      \
              FIRST_ARG(__VA_ARGS__)                        \
              , __FILE__                                    \
              , __func__                                    \
              , __LINE__                                    \
              COMMA_AND_TAIL_ARGS(__VA_ARGS__));            \
        _break(5);                                          \
    })                                                      \
        
#define INPUT_ERROR(...)                                        \
    ({                                                          \
        elog4("Input error. %s:%s:%d. " FIRST_ARG(__VA_ARGS__)  \
              , __FILE__                                        \
              , __func__                                        \
              , __LINE__                                        \
              COMMA_AND_TAIL_ARGS(__VA_ARGS__));                \
        _break(6);                                              \
        -1;                                                     \
    })
      
/* --- Helpers --- */

static inline int meets_errlog_criteria(int min_verb_lvl){
    return
        ERR_PRINT_LVL >= min_verb_lvl
        ||                     
        (VIP_VERBOSITY >= min_verb_lvl && fun_is_vip(__func__)); 
}

#define elog(...) _elog(1, __VA_ARGS__)
#define elog2(...) _elog(2, __VA_ARGS__)
#define elog3(...) _elog(3, __VA_ARGS__)
#define elog4(...) _elog(4, __VA_ARGS__)

/* Same trick to emulate GCC ##__VA_ARGS__. None of the current error
   macros should ever pass an empty __VA_ARGS__, but we may as well handle
   that case anyway. */
#define _elog(N, ...)                                       \
    if(meets_errlog_criteria(N)){                           \
        e_printf("%ld - THR:%lo - " FIRST_ARG(__VA_ARGS__)    \
                 , e_get_ticks()                            \
                 , e_gettid()                               \
                 COMMA_AND_TAIL_ARGS(__VA_ARGS__));         \
    } else (void)0;                                         

#include <global.h>
static inline void _break(int min_break_lvl){
    if(ERR_BREAK_LVL >= min_break_lvl)
        BREAK;
}

#endif


