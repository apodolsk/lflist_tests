#pragma once

#include <stdio.h>
#include <vip_fun.h>
#include <pustr.h>

/* 0 to disable non-VIP logging. */
#define LOG_MASTER 0
#define DYNAMIC_LOG 0

/* 1 enables mute_log() and unmute_log(). */
#define DYNAMIC_GLOBAL_MUTE 0

/* 1 supersedes all other log settings inside functions on vip_list. */
#define VIP_MODE 0
#define VIP_VERBOSITY 1

/* Verbosities */

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
#define LOG_UNITESTS 0
#define LOG_LIST_TESTSM 1

#define LOG_LVL CONCAT(LOG_, MODULE)

#if !DYNAMIC_GLOBAL_MUTE
#define mute_flag 0
#define mute_log()
#define unmute_log()
#endif

#define TS ()

#define tlprintf(ts, fmt, as...) puprintf(ts, "T:% "fmt"\n", itid(), ##as)
#define lprintf(fmt, as...) puprintf(TS, "T:% "fmt"\n", itid(), ##as)

#if !LOG_MASTER
#define log(...)
#define log2(...)
#define pp(...)
#define trace(module, ts, f, as...) f(as)

#else

#define log(fmt, as...) (can_log(MODULE, 1) ? lprintf(fmt, ##as) : 0)
#define log2(fmt, as...) (can_log(MODULE, 2) ? lprintf(fmt, ##as) : 0)

#define NAMEFMT(a, _, __) a:%
#define pp(as...) log(STRLIT(MAP(NAMEFMT, _, as)), ##as)

#define trace(module, ts, f, as...)                                     \
    (can_log(module, 1) ? putrace(tlprintf, ts, f, ##as) : f(as))

#define can_log(module, needed)                                     \
    ((LOG_MASTER && CONCAT(LOG_, MODULE) >= needed && !mute_flag)   \
     ||                                                             \
     (VIP_MODE && VIP_VERBOSITY >= needed && fun_is_vip(__func__)))

#endif    
