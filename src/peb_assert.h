#ifndef PEB_ASSERT_H
#define PEB_ASSERT_H

#include <assert.h>
#include <errors.h>

#ifdef NDEBUG
#error NDEBUG is ignored. Use DBG in errors.h instead.
#endif

#ifndef DBG_LVL
#error
#endif

#undef assert

#define assert(expr) _assert(1, expr)
#define assert2(expr) _assert(2, expr)
#define _assert(lvl, expr, ...)                                         \
    ((void) ((DBG_LVL >= lvl && !(expr))                                \
             ? EWTF("Failed assertion %s.", #expr)                      \
             : 0))                                                      \

#define rassert(expr1, expr2, relation) _rassert(1, expr1, expr2, relation)
#define _rassert(_lvl, expr1, rel, expr2, ...)                \
    do{                                                       \
        if(DBG_LVL >= _lvl && !(expr1 rel expr2)){            \
            EWTF("Failed assertion %s with %p : %p",                \
                 #expr1 #rel #expr2, (void*) expr1, (void*) expr2); \
        }                                                           \
    }while(0);

#endif




