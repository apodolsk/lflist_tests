#pragma once
#ifndef __ASSEMBLER__

#include "pumacros.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef NPUSTR
#define pustr(...)
#define pudef(...)
#define putrace(_, __, f, as...) f(as)
#define putracev(_, __, f, as...) f(as)
#define putracef(_, __, f, as...) f(as)
#define putracevf(_, __, f, as...) f(as)
#define pulog(...)
#define pulogf(...)
#else

/* Note that every _Generic clause must be a well-typed expr. The array
   literals serve to generate a pointer to 'a' so that we can defeat the
   typechecker by casting pointers when passing args to the pu_snprint
   functions, while only the clause where the cast doesn't invoke
   undefined behavior is the one that is ultimately evaluated.

   The (0, a) causes the type of a to decay to a pointer type if it's an
   array type (such as typeof("hi")). Otherwise, you'd have to add
   char[1], char[2], etc clauses. It also strips volatile and const away.
*/
#define _v volatile
#define _c const
#define pustr(a, ts...)                                                 \
    _Generic((0, a),                                                    \
             MAP(pu_ref, (typeof((0, a))[]){a}, DEFAULT_TYPES, ##ts),   \
             char *: a,                                                 \
             void *: _pu_ref_bod(void, 0, *(void **)(typeof(a)[]){a}),  \
    default: "<>")
#define pu_ref(t, a, _)                                                 \
    typeof( (0, *(t[1]){}) ) : _pu_ref_bod(t, 0, (t *) a),             \
    typeof( (0, *(t[1]){}) ) *: _pu_ref_bod(t, 1, *(t **)a)
#define _pu_ref_bod(t, isptr, a) aasprintf(CONCAT(pu_snprint, t), isptr, a)

#define pudef(t, fmt, as...)                                            \
    static inline int CONCAT(pu_snprint, t)                             \
    (char *b, size_t l, int is_really_ptr, t *a){                       \
        if(is_really_ptr)                                               \
            return snprintf(b, l, "%p:&"fmt, a, as);                    \
        else                                                            \
            return snprintf(b, l, fmt, as);                             \
    }

#define aasprintf(snprint, ispt, a)                     \
    ({                                                  \
        size_t __pu_l = 1 + snprint(NULL, 0, ispt, a);  \
        char *__pu_b = alloca(__pu_l);                  \
        snprint(__pu_b, __pu_l, ispt, a);               \
        __pu_b;                                         \
    })

#define _pu_argstr(a, __, ___) a:%s
#define _call_pustr(a, ts, _) __call_pustr(a COMMAPFX_IF_NZ ts)
#define __call_pustr(as...) pustr(as)
#define pulog(print, ts, as...)                                         \
    print("%s:%d - " STRLIT(MAP(_pu_argstr, _, as)),                    \
          __func__, __LINE__, MAP3(_call_pustr, ts, as))

#define _call_strof(a, strof, _) strof(a) 
#define pulogf(print, strof, as...)                                     \
    print(STRLIT(MAP(_pu_argstr, _, as))                                \
           " in %s:%d", MAP2(_call_strof, strof, as),                   \
           __func__, __LINE__)



#define PU_STORE(arg, _, i)                                     \
    typeof((0,arg)) CONCAT(__pu_arg, i) = arg;

#define PU_REF_STRING(arg, strof, i) strof(PU_REF(arg, _, i)) ,
#define PU_REF(arg, _, i) CONCAT(__pu_arg, i)
#define PU_STRFMT(_, __, ___) %s

#define putracev(print, ts, fun, as...)                                 \
        ({                                                              \
            MAP_NOCOMMA(PU_STORE, _, as);                               \
            print("-- Begin %s(" STRLIT(MAP(PU_STRFMT, _, as)) ") in %s:%d", \
                  #fun COMMAPFX_IF_NZ(MAP2(_call_pustr, ts, as)),       \
                  __func__, __LINE__);                                  \
            fun(MAP(PU_REF, _, as));                                    \
        })

#define putrace(print, ts, fun, as...)                              \
    ({                                                              \
        typeof(fun(as)) __pu_ret = putracev(print, ts, fun, as);    \
        print("-- End %s ret: %s", #fun, _call_pustr(__pu_ret, ts, _)); \
        __pu_ret;                                                   \
    })


#define putracevf(print, strof, fun, as...)                            \
    ({                                                                 \
        MAP_NOCOMMA(PU_STORE, _, as);                                  \
        print("Begin %s(" STRLIT(MAP(PU_STRFMT, _, as)) ") in %s:%d",  \
              #fun, MAP_NOCOMMA(PU_REF_STRING, strof, as)              \
              __func__, __LINE__);                                     \
        fun(MAP(PU_REF, _, as));                                       \
    })

#define putracef(print, strof, fun, as...)                             \
    ({                                                                 \
        typeof(fun(as)) __pu_ret = putracevf(print, strof, fun, as);   \
        print("End %s. ret: %s", #fun, strof(__pu_ret));               \
        __pu_ret;                                                      \
    })



#define DEFAULT_TYPES                           \
    int8_t, int16_t, int32_t, int64_t,          \
    uint8_t, uint16_t, uint32_t, uint64_t

pudef(int8_t, "%"PRId8, *a);
pudef(int16_t, "%"PRId16, *a);
pudef(int32_t, "%"PRId32, *a);
pudef(int64_t, "%"PRId64, *a);
pudef(uint8_t, "%"PRIu8, *a);
pudef(uint16_t, "%"PRIu16, *a);
pudef(uint32_t, "%"PRIu32, *a);
pudef(uint64_t, "%"PRIu64, *a);
pudef(void, "%p", a);

#endif
#endif

