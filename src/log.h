#pragma once

#include <stdio.h>
#include <vip_fun.h>
#include <pustr.h>

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

#define LOG_LVL CONCAT(LOG_, MODULE)

#if !DYNAMIC_GLOBAL_MUTE
#define mute_flag 0
#define mute_log()
#define unmute_log()
#endif

#if LOG_MASTER && LOG_LVL > 0
/* #define log(as...) pulog(llprintf1, t) */
/* #define log2(ts, as...) pulog(llprintf1, t) */
#define log(as...) 
#define log2(ts, as...) 

#define trace(ts, f, as...) putrace(llprintf1, ts, as)
#else
#define log(...)
#define log2(...)
#define trace(ts, f, as...) f(as)
#endif

#define llprintf1(a...) llprintf(1, a...)

/* ---------- Main logger plumbing. ---------------*/

#define llprintf(need_lvl, s, ...) do{                                  \
        if(meets_log_criteria(LOG_LVL, need_lvl))                       \
            lprintf(s, ##__VA_ARGS__);                                  \
    } while(0)                                                          \

#define meets_log_criteria(log_lvl, needed)             \
    ((LOG_MASTER && log_lvl >= needed && !mute_flag)    \
     ||                                                 \
     (VIP_MODE && VIP_VERBOSITY >= needed && fun_is_vip(__func__)))

