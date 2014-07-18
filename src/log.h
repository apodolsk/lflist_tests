#pragma once

#include <stdio.h>
#include <pustr.h>


/* If 0 to all logging (even VIP) is disabled. */
#define LOG_MASTER 1
#define DYNAMIC_LOG 0

/* If 1, VIP_VERBOSITY supersedes module verbosities for functions in
   log.c:vips. */
#define VIP_MODE 0
#define VIP_VERBOSITY 1
bool fun_is_vip(const char *fun_name);

/* Verbosities */

#define LOG_UNITESTS 1
#define LOG_NALLOC 1
#define LOG_LFLISTM 2
#define LOG_ATOMICS 0
#define LOG_STACKM 1
#define LOG_LIST_TESTSM 1

#define LOG_LVL CONCAT(LOG_, MODULE)

#define lprintf(fmt, as...) puprintf("T:% "fmt, itid(), ##as)

#if !LOG_MASTER
#define log(...)
#define log2(...)
#define pp(...)
#define trace(module, lvl, f, as...) f(as)

#else

#define log(fmt, as...) (can_log(MODULE, 1) ? lprintf(fmt, ##as) : (void) 0)
#define log2(fmt, as...) (can_log(MODULE, 2) ? lprintf(fmt, ##as) : (void) 0)

#define NAMEFMT(a, _, __) a:%
#define pp(as...) log(STRLIT(MAP(NAMEFMT, _, as)), ##as)

#define trace(module, lvl, f, as...)                            \
    (can_log(module, lvl) ? putrace(lprintf, f, ##as) : f(as))

#define can_log(module, needed)                                     \
    ((!muted() && LOG_MASTER && CONCAT(LOG_, module) >= needed)     \
     ||                                                             \
     (VIP_MODE && VIP_VERBOSITY >= needed && fun_is_vip(__func__)))

extern bool mute_log;
#define unmute_log() mute_log = 0;
#define mute_log() mute_log = 1;
#define muted() mute_log

#endif    
