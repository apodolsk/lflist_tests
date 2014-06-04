#pragma once
#ifndef __ASSEMBLER__

#include "pumacros.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef NPUSTR
#define pudef(...)
#define pusnprintf(...)
#define puprintf(...)
#define putrace(_, __, f, as...) f(as)
#define putracev(_, __, f, as...) f(as)
#else

#define pudef(t, ts, fmt, as...)                        \
    static inline size_t CONCAT(pusnprint_, t)          \
    (char *b, size_t l, int is_ptr, volatile const t *a){        \
        if(is_ptr)                                      \
            return pusnprintf(ts, b, l, "%:&"fmt, (void *) a, ##as);    \
        return pusnprintf(ts, b, l, fmt, ##as);             \
    }

typedef size_t (*typed_snprint)(char *, size_t, int, volatile const void *);

typedef struct{
    volatile const void *val;
    int is_ptr;
    typed_snprint typed_snprint;
}pu_arg;

static inline size_t puvsnprintf(char *b, size_t max, char *fmt, va_list args){
    size_t l = 0;
    for(char *c = fmt; *c != '\0'; c++){
        if(*c == '%'){
            pu_arg *a = va_arg(args, pu_arg *);
            l += a->typed_snprint(b + l, max > l ? max - l : 0,
                                  a->is_ptr, a->val);
        }
        else if(l < max)
            b[l++] = *c;
    }
    if(l < max)
        b[l] = '\0';
    return l;
}

#define pusnprintf(ts, b, l, fmt, as...)        \
    _pusnprintf(b, l, fmt, MAP(pu_arg_of, ts, as))

#define pu_arg_of(a, ts, _)                     \
    _Generic((0, a),                                                    \
             MAP2(pu_arg_typed, (typeof((0, a))[]){a},                  \
                  DEFAULT_TYPES COMMAPFX_IF_NZ ts),                     \
             void *:                                                    \
             &(pu_arg){(typeof((0,a))[1]){(0,a)},                       \
                     0, (typed_snprint) pusnprint_ptr}, \
    default: &(pu_arg){(typeof((0,a))[1]){(0,a)}, 0, pusnprint_dflt})

#define pu_arg_typed(t, a, _)                                           \
    typeof( (0, *(t[1]){}) ) : build_pu_arg((t *) a, 0, t),             \
        volatile typeof( (0, *(t[1]){}) ) : build_pu_arg((t *) a, 0, t), \
        const typeof( (0, *(t[1]){}) ) : build_pu_arg((t *) a, 0, t),   \
        typeof( (0, *(t[1]){}) ) *: build_pu_arg(*(t **)a, 1, t),       \
        volatile typeof( (0, *(t[1]){}) ) *: build_pu_arg(*(t **)a, 1, t), \
        const typeof( (0, *(t[1]){}) ) *: build_pu_arg(*(t **)a, 1, t)
        
#define build_pu_arg(v, is_ptr, t)               \
    &(pu_arg){v, is_ptr, (typed_snprint) &CONCAT(pusnprint_, t)}

static inline size_t _pusnprintf(char *b, size_t max, char *fmt, ...){
    va_list args; 
    va_start(args, fmt);
    size_t l = puvsnprintf(b, max, fmt, args);
    va_end(args);
    return l;
}

#define puprintf(ts, fmt, as...)                \
    _puprintf(fmt, MAP(pu_arg_of, ts, as))
#define PU_DFLT_BUF_SZ 248
static inline size_t _puprintf(char *fmt, ...){
    va_list l;
    va_start(l, fmt);
    size_t max = PU_DFLT_BUF_SZ, need;
    for(int i = 0; i < 2; i++){
        char b[max];
        need = 1 + puvsnprintf(b, max, fmt, l);
        if(max >= need){
            fputs(b, stdout);
            break;
        }
        max = need;
    }
    va_end(l);
    return need;
}

#define PU_STORE(arg, _, i)                                     \
    typeof((0,arg)) CONCAT(__pu_arg, i) = arg;
#define PU_REF(arg, _, i) CONCAT(__pu_arg, i)
#define PU_STRFMT(_, __, ___) %


#define putracev(print, ts, fun, as...)                                 \
        ({                                                              \
            MAP_NOCOMMA(PU_STORE, _, as);                               \
            print(ts, "-- Begin %(" STRLIT(MAP(PU_STRFMT, _, as))       \
                  ") in %:%", #fun                                      \
                      COMMAPFX_IF_NZ(MAP3(PU_REF, _, as)),              \
                     __func__, __LINE__);                               \
            fun(MAP(PU_REF, _, as));                                    \
        })

#define putrace(print, ts, fun, as...)                              \
    ({                                                              \
        MAP_NOCOMMA(PU_STORE, _, as);                               \
        print(ts, "-- Begin %(" STRLIT(MAP(PU_STRFMT, _, as))         \
              ") in %:%", #fun                                        \
              COMMAPFX_IF_NZ(MAP3(PU_REF, _, as)),                    \
              __func__, __LINE__);                                    \
        typeof(fun(as)) __pu_ret = fun(MAP(PU_REF, _, as));         \
        print(ts, "-- End % = %(" STRLIT(MAP(PU_STRFMT, _, as))         \
              ") in %:%\n", __pu_ret, #fun                              \
              COMMAPFX_IF_NZ(MAP3(PU_REF, _, as)),                      \
              __func__, __LINE__);                                      \
        __pu_ret;                                                   \
    })

#define DEFAULT_TYPES                           \
    int8_t, int16_t, int32_t, int64_t,          \
        uint8_t, uint16_t, uint32_t, uint64_t, char

#define pudef_dflt(t, fmt)                                              \
    static inline size_t CONCAT(pusnprint_, t)                          \
    (char *b, size_t l, int is_ptr, t *a){                              \
        if(is_ptr)                                                      \
            return snprintf(b, l, "%p:&"fmt, a, *a);                    \
        return snprintf(b, l, fmt, *a);                                 \
    }


pudef_dflt(int8_t, "%"PRId8);
pudef_dflt(int16_t, "%"PRId16);
pudef_dflt(int32_t, "%"PRId32);
pudef_dflt(int64_t, "%"PRId64);
pudef_dflt(uint8_t, "%"PRIu8);
pudef_dflt(uint16_t,  "%"PRIu16);
pudef_dflt(uint32_t, "%"PRIu32);
pudef_dflt(uint64_t, "%"PRIu64);

static inline
size_t pusnprint_ptr(char *b, size_t l, int _, volatile const void **a){
    (void) _;
    return snprintf(b, l, "%p", *a);
}

static inline
size_t pusnprint_char(char *b, size_t l, int is_ptr, volatile const char *a){
    if(is_ptr)
        return snprintf(b, l, "%s", a);
    if(l)
        *b = *a;
    return 1;
}

static inline
size_t pusnprint_dflt(char *b, size_t l, int _, volatile const void *__){
    (void) _, (void) __;
    return snprintf(b, l, "<>");
}

#endif
#endif

