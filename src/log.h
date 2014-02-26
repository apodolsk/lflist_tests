/**
 * @file   log.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief Macros to wrap extra info around printfs and to set up execution
 * traces. The various verbosity controls for these are also here.
 * 
 * Idea: treat the expansion of MODULE like an argument. Use it to control
 * the expansion of the trace macros in the files that belong to that
 * module.
 *
 * To set up logging in a module:
 * - Pick a name for it. Inside this file, #define the name of the module
 *   to expand to the verbosity level you want its logging to have.
 * - In the module's source files, #define the macro MODULE to evaluate to
 *   the name you just #defined. This definition has to occur before you
 *   #include log.h.
 *   
 * The main macros are traceN() and logN(). traceN() prints a function's
 * name and parameters and logN() wraps tid and tick info around printf().
 *
 * The titular N is the min verbosity needed for a logger call to be
 * active. If N is 1, it's omitted.
 *
 * The args to traceN() are a sequence of pairs of the form "[param-name],
 * [printf-conversion-spec-for-param]". It'll count the args on its own.
 *
 * Logging can be muted globally at run-time with mute_log() and
 * unmute_log().
 * 
 * If VIP mode is on, then functions on the vip_list in vip_fun.h will
 * ignore all verbosity settings except VIP_VERBOSITY.
 */

#ifndef PEB_LOG_H
#define PEB_LOG_H

#include <stdio.h>
#include <vip_fun.h>

/* The logger might be used for static inline functions in header
   files. (In particular, for DECLARE_STRUCT_PRINTER).*/
int _gettid(void);
unsigned int _get_ticks(void);

/* Set to 0 to disable all non-VIP logging. */
#define LOG_MASTER 1
#define DYNAMIC_LOG 0

/* Set to 1 to enable mute_log() and unmute_log(). Turning it off will lead
   to spam during kern init. */
#define DYNAMIC_GLOBAL_MUTE 0

/* Set to 1 to ignore all other verbosity settings inside functions on the
   vip_list in vip_fun.h. */
#define VIP_MODE 0
#define VIP_VERBOSITY 2

/* -------- Per-module verbosity settings ---------- */
/* Ranges from 0 to 4 in my code. 1 is usually for interface functions,
   and then 2 and below are for increasingly obscure helpers. */

#define LOG_NALLOC 0
#define LOG_ERRORS 0

#define LOG_MAIN 0
#define LOG_PEBRAND 0

#define LOG_LIST 0
#define LOG_STACK 0

#define LOG_KMALLOC 0
#define LOG_POOLS 0
#define LOG_UNIT_TESTS 1

/* Set up dynamic controls that might be touchable in a debugger. */
#if DYNAMIC_LOG && defined MODULE
static volatile int CONCAT(DYN_LOG_, MODULE) = CONCAT(LOG_, MODULE);
#define LOG_LVL CONCAT(DYN_LOG_, MODULE)
#else /*!(DYNAMIC_LOG && MODULE)*/
#define LOG_LVL CONCAT(LOG_, MODULE)
#endif

#if !LOG_LVL
#undef LOG_LVL
#define LOG_LVL 0
#endif


/* --------- System interface ---------- */

#define printf_ln(...)                                                  \
    printf(FIRST_ARG(__VA_ARGS__) "\n" COMMA_AND_TAIL_ARGS(__VA_ARGS__))
#define log_get_ticks() _get_ticks()
#define log_gettid() _gettid()
#define log_printf(...) lprintf(__VA_ARGS__)
#define log_both(...)                           \
    do{                                         \
        printf_ln(__VA_ARGS__);                 \
        log(__VA_ARGS__);                       \
    } while(0);


/* --------- Dynamic global control  ------------ */

#if DYNAMIC_GLOBAL_MUTE > 0

/* extern int mute_flag; */

/* #define mute_log()                              \ */
/*     mute_flag = TRUE */
/* #define unmute_log()                            \ */
/*     mute_flag = FALSE */

#else

#define mute_flag 0

#define mute_log()
#define unmute_log()

#endif  /* DYNAMIC_GLOBAL_MUTE > 0 */

/* ---------- Main logger plumbing. ---------------*/

/* #define wrap_tid(prnt, ...)                         \ */
/*         prnt("%u THR:%d "FIRST_ARG(__VA_ARGS__),    \ */
/*          log_get_ticks(),                           \ */
/*          log_gettid()                               \ */
/*          COMMA_AND_TAIL_ARGS(__VA_ARGS__))          \ */

/* #define wrap_tid(prnt, ...)                         \ */
/*         prnt("%u THR: "FIRST_ARG(__VA_ARGS__),    \ */
/*              log_get_ticks()                       \ */
/*              COMMA_AND_TAIL_ARGS(__VA_ARGS__))      \ */

#define wrap_tid(prnt, ...)                     \
    prnt("%ld THR:%lo "FIRST_ARG(__VA_ARGS__),   \
         log_get_ticks(),                       \
         log_gettid()                           \
         COMMA_AND_TAIL_ARGS(__VA_ARGS__))      \


#if VIP_MODE > 0
#define fun_is_vip(name) fun_is_very_important(name)
#else
#define fun_is_vip(name) 0
#endif

#define meets_log_criteria(log_lvl, needed)     \
    ((LOG_MASTER > 0 &&                         \
      log_lvl >= needed &&                      \
      mute_flag == 0)                           \
     ||                                         \
     (VIP_VERBOSITY >= needed &&                \
      fun_is_vip(__func__)))

#define log(...) _log(1, __VA_ARGS__)
#define log2(...) _log(2, __VA_ARGS__)
#define log3(...) _log(3, __VA_ARGS__)
#define log4(...) _log(4, __VA_ARGS__)

#define trace(...) _trace(1, __VA_ARGS__)
#define trace2(...) _trace(2, __VA_ARGS__)
#define trace3(...) _trace(3, __VA_ARGS__)
#define trace4(...) _trace(4, __VA_ARGS__)
#define trace5(...) _trace(5, __VA_ARGS__)

#define _log(needed, ...) __log(LOG_LVL, needed, __VA_ARGS__)
#define _trace(needed, ...) __trace(LOG_LVL, needed, __VA_ARGS__)

/** 
 * @brief If the containing module's verbosity level is greater than lvl,
 * append some extra data to the provided printf-style arguments. Then, print
 * the resulting printf-style arguments. 
 *
 * FIRST_ARG and COMMA_AND_TAIL_ARGS replace GCC's ##__VA_ARGS__. See
 * peb_util.h.
 * 
 * @param lvl The min value that MODULE needs to be to prevent this
 * call from being compiled out.
 * @param __VA_ARGS__ Either nothing, or a plain string, or a printf format
 * string with helper args.
 */
#define __log(lvl, needed, ...)                                 \
    do{                                                         \
        if(meets_log_criteria(lvl, needed)){                    \
            wrap_tid(log_printf, __VA_ARGS__);                  \
        }                                                       \
    }while (0)                                                  \

/* @brief Count the number of args passed to traceN(), and invoke the
   appropriate helper macro to handle the given number of arguments.
*/
#define __trace(lvl, needed,...)                                \
    do{                                                         \
        if(meets_log_criteria(lvl, needed)) {                   \
            CONCAT(_trace, NUM_ARGS(__VA_ARGS__))(__VA_ARGS__); \
        }                                                       \
    }while(0)                                                   \

#define _trace0()                               \
    wrap_tid(log_printf, "Entered %s.",         \
               __func__)

/* Note that the number after _trace here is the number of args, NOT the
   verbosity level like in non-underscore-trace */
#define _trace2(a,formata)                      \
    wrap_tid(log_printf, "Entered %s - "        \
             #a": %"#formata" ",                \
             __func__, a)                       


#define _trace4(a, formata, b, formatb)         \
    wrap_tid(log_printf, "Entered %s - "        \
             #a": %"#formata" "                 \
             #b": %"#formatb" ",                \
             __func__, a, b)                    

#define _trace6(a, formata, b, formatb, c, formatc) \
    wrap_tid(log_printf, "Entered %s - "            \
             #a": %"#formata" "                     \
             #b": %"#formatb" "                     \
             #c": %"#formatc" ",                    \
             __func__, a, b, c)                      


#define _trace8(a, formata, b, formatb, c, formatc, d, formatd) \
    wrap_tid(log_printf, "Entered %s - "                        \
             #a": %"#formata" "                                 \
             #b": %"#formatb" "                                 \
             #c": %"#formatc" "                                 \
             #d": %"#formatd" ",                                \
             __func__, a, b, c, d)

#define _trace10(a, formata, b, formatb, c, formatc, d, formatd, e, formate) \
        wrap_tid(log_printf, "Entered %s - "                             \
                 #a": %"#formata" "                                     \
                 #b": %"#formatb" "                                     \
                 #c": %"#formatc" "                                     \
                 #d": %"#formatd" "                                     \
                 #e": %"#formate" ",                                    \
                 __func__, a, b, c, d, e)                           

/* ------------ Misc printing functions ---------------- */

#define dump_ureg(ureg, prnt)                                   \
    do{                                                         \
        prnt("--------- Dumping ureg ----------");              \
        prnt("%s:%s:%d", __FILE__, __func__, __LINE__);         \
        prnt("eax: 0x%08X   eip   : 0x%08X   cs   : 0x%08X",    \
             ureg->eax, ureg->eip, ureg->cs);                   \
        prnt("ecx: 0x%08X   esp   : 0x%08X   ss   : 0x%08X",    \
             ureg->ecx, ureg->esp, ureg->ss);                   \
        prnt("edi: 0x%08X   cause : 0x%08X   ds   : 0x%08X",    \
             ureg->edi, ureg->cause, ureg->ds);                 \
        prnt("ebx: 0x%08X   cr2   : 0x%08X   es   : 0x%08X",    \
             ureg->ebx, ureg->cr2, ureg->es);                   \
        prnt("ebp: 0x%08X   ecode : 0x%08X   fs:  : 0x%08X ",   \
             ureg->ebp, ureg->error_code, ureg->fs);            \
        prnt("esi: 0x%08X   eflags: 0x%08X   gs   : 0x%08X",    \
             ureg->esi, ureg->eflags, ureg->gs);                \
        prnt("edi: 0x%08X   k_esp : 0x%08X",                    \
             ureg->edi, ureg->zero);                            \
    }while(0)

#define dump_elf_header(elf)                                \
    do{                                                     \
        log("------- Dumping sELF -------");                \
        log("e_fname:       %s",      (elf)->e_fname);      \
        log("e_entry:       0x%lx",   (elf)->e_entry);      \
        log("e_txtoff:      %lu",     (elf)->e_txtoff);     \
        log("e_txtlen:      %lu",     (elf)->e_txtlen);     \
        log("e_txtstart:    0x%lx",   (elf)->e_txtstart);   \
        log("e_datoff:      %lu",     (elf)->e_datoff);     \
        log("e_datlen:      %lu",     (elf)->e_datlen);     \
        log("e_datstart:    0x%lx",   (elf)->e_datstart);   \
        log("e_rodatoff:    %lu",     (elf)->e_rodatoff);   \
        log("e_rodatlen:    %lu",     (elf)->e_rodatlen);   \
        log("e_rodatstart:  0x%lx",   (elf)->e_rodatstart); \
        log("e_bsslen:      %lu",     (elf)->e_bsslen);     \
        log("e_bssstart:    0x%lx", (elf)->e_bssstart);     \
    } while(0)                                              \

#endif  /* PEB_LOG_H */
