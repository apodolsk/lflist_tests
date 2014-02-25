/**
 * @file   peb_assert.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * @date   Sun Sep  2 20:41:53 2012
 */

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

#define MAGIC_ASS(pred)                                     \
    do{                                                     \
        if(!(pred)){                                        \
            LOGIC_ERROR("Failed assertion `%s'.",           \
                        #pred);                             \
        }                                                   \
    }while(0)

#define if_dbg(expr) _if_dbg(1, expr)
#define assert(expr) _assert(1, expr)
#define assert2(expr) _assert(2, expr)
#define hassert(expr, handler) _hassert(1, expr, handler)

/* #define _assert(lvl, pred)                           \ */
/*     do{                                              \ */
/*         if(DBG_LVL >= lvl)                           \ */
/*             MAGIC_ASS(pred);                         \ */
/*     }while(0)                                        \ */

#define _if_dbg(lvl, expr)                           \
    do{                                              \
        if(DBG_LVL >= lvl)                           \
            (expr);                                  \
    }while(0)                                        \

/* Carry out some action before breaking. For example, I use this to print a
   ureg upon failed exception assert. */
#define _hassert(lvl, expr, handler)            \
         do{                                    \
             if(DBG_LVL >= lvl && !(expr)){     \
                 handler;                       \
                 MAGIC_ASS(expr);               \
             }                                  \
         }while(0)

#define _assert(lvl, expr, ...)                                 \
    do{                                                         \
        if(DBG_LVL >= lvl && !(expr)){                          \
            LOGIC_ERROR("Failed assertion %s.", #expr);         \
        }                                                       \
    }while(0)

#define rassert(expr1, expr2, relation) _rassert(1, expr1, expr2, relation)
#define _rassert(_lvl, expr1, rel, expr2, ...)                \
    do{                                                       \
        if(DBG_LVL >= _lvl && !(expr1 rel expr2)){            \
            LOGIC_ERROR("Failed assertion %s with %p : %p",   \
                        #expr1 #rel #expr2, (void*) expr1, (void*) expr2); \
        }                                                     \
    }while(0);

#endif




