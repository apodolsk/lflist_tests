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

#pragma once

#include <stdio.h>
#include <vip_fun.h>

/* 0 to disable non-VIP logging. */
#define LOG_MASTER 1
#define DYNAMIC_LOG 0

/* 1 enables mute_log() and unmute_log(). */
#define DYNAMIC_GLOBAL_MUTE 0

/* 1 supersedes all other log settings inside functions on vip_list. */
#define VIP_MODE 1
#define VIP_VERBOSITY 2

/* -------- Per-module verbosity settings ---------- */

#define LOG_NALLOC 0
#define LOG_ERRORS 0

#define LOG_MAIN 0
#define LOG_PEBRAND 0

#define LOG_ATOMICS 0
#define LOG_LISTM 0
#define LOG_LFLISTM 1
#define LOG_STACKM 0

#define LOG_KMALLOC 0
#define LOG_POOLS 0
#define LOG_UNITESTS 1
#define LOG_LIST_TESTSM 1

#if !LOG_LVL
#undef LOG_LVL
#define LOG_LVL 0
#endif

#if DYNAMIC_GLOBAL_MUTE > 0
/* deleted this junk */
#else
#define mute_flag 0
#define mute_log()
#define unmute_log()
#endif  /* DYNAMIC_GLOBAL_MUTE > 0 */

/* ---------- Main logger plumbing. ---------------*/

#define llprintf(need_lvl, s, ...) do{                                  \
        if(LOG_LVL >= need_lvl)                                         \
            lprintf("%THR:%d "s, _gettid(), ##__VA_ARGS__);              \
    } while(0)                                                          \

#define meets_log_criteria(log_lvl, needed)             \
    ((LOG_MASTER && log_lvl >= needed && !mute_flag)    \
     ||                                                 \
     (VIP_MODE && VIP_VERBOSITY >= needed && fun_is_vip(__func__)))

#define log(...) llprintf(1, __VA_ARGS__)
#define log2(...) llprintf(2, __VA_ARGS__)
#define log3(...) llprintf(3, __VA_ARGS__)
#define log4(...) llprintf(4, __VA_ARGS__)

#define DECLARE_VAR(fmt, arg, i) typeof(arg) trace_arg##i = arg;                                       
#define USE_VAR(fmt, arg, i) trace_arg##i ,
#define USE_VAR_PRECOMMA(fmt, arg, i) , trace_arg##i
#define GET_FMT(fmt, arg, i) #fmt", " 

/*
#define trace(need_lvl, f, msg, ...)                                    \
    ({                                                                  \
        MAP_PAIRS(DECLARE_VAR, TAIL(__VA_ARGS__))                       \
            llprintf(                                                   \
                __FUNC__                                                \
                ":"HEAD_ARG(__VA_ARGS__)                                \
                "("                                                     \
                MAP_PAIRS(GET_FMT, TAIL(__VA_ARGS__))                   \
                ")"                                                     \
                MAP_PAIRS(USE_VAR, TAIL(__VA_ARGS__))                   \
                );                                                      \
        typeof(HEAD(__VA_ARGS__)(SECOND_HALF(TAIL_ARGS(__VA_ARGS__))))  \
            ret = HEAD_ARG(__VA_ARGS__)(SECOND_HALF(TAIL_ARGS(__VA_ARGS__))); \
                                                                        \
    })                                                                  \
*/

#define trace(...)                                              \
    CONCAT(trace, NUM_ARGS(__VA_ARGS__))(1, ##__VA_ARGS__)

#define trace0(n)                               \
    llprintf(n, "Entered %s.", __func__)

/* Note that the number after race here is the number of args, NOT the
   verbosity level like in non-underscore-trace */
#define trace2(n,a,formata)                    \
    llprintf(n, "Entered %s - "                 \
             #a": %"#formata" ",                \
             __func__, a)                       


#define trace4(n, a, formata, b, formatb)   \
    llprintf(n, "Entered %s - "           \
             #a": %"#formata" "                 \
             #b": %"#formatb" ",                \
             __func__, a, b)                    

#define trace6(n, a, formata, b, formatb, c, formatc)    \
    llprintf(n, "Entered %s - "            \
             #a": %"#formata" "                     \
             #b": %"#formatb" "                     \
             #c": %"#formatc" ",                    \
             __func__, a, b, c)                      


#define trace8(n, a, formata, b, formatb, c, formatc, d, formatd)    \
    llprintf(n, "Entered %s - "                        \
             #a": %"#formata" "                                 \
             #b": %"#formatb" "                                 \
             #c": %"#formatc" "                                 \
             #d": %"#formatd" ",                                \
             __func__, a, b, c, d)

#define trace10(n, a, formata, b, formatb, c, formatc, d, formatd, e, formate) \
        llprintf(n, "Entered %s - "                             \
                 #a": %"#formata" "                                     \
                 #b": %"#formatb" "                                     \
                 #c": %"#formatc" "                                     \
                 #d": %"#formatd" "                                     \
                 #e": %"#formate" ",                                    \
                 __func__, a, b, c, d, e)                           

/* ------------ Misc printing functions ---------------- */

