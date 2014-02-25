/**
 * @file   pretty_print.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief  Utilities for easily printing numbers and structs.
 *  
*/

#ifndef PEB_PRETTY_PRINT_H
#define PEB_PRETTY_PRINT_H

#include <log.h>
#include <peb_macros.h>

/* #define APPEND_STRING(n, global_ignored)        \ */
/*     #n": " FORMAT_SPECIFIER_OF((n)) */


/* #define APPEND_STRING(n, global_ignored)        \ */
/*     #n": " "%p" */

/* TODO */

#define PRINT(a, b) _PRINT(a,b)                 \

/* #define _PRINT(a, b)                                              \ */
/*     wrap_tid(lprintf, FORMAT_SPECIFIER_OF(a), 1) */

/* #define _PRINT(a, b)                                              \ */
/*     log(lprintf, FORMAT_SPECIFIER_OF(a), a) */

/* #define _wrap_tid(prnt, ...)                         \ */
/*     prnt("%u THR:%d "FIRST_ARG(__VA_ARGS__),         \ */
/*          log_get_ticks(),                           \ */
/*          log_gettid()                               \ */
/*          COMMA_AND_TAIL_ARGS(__VA_ARGS__))          \ */

#define _wrap_tid(prnt, ...)                         \
    prnt("%u THR:%d "FIRST_ARG(__VA_ARGS__),         \
         log_get_ticks(),                           \
         log_gettid()                               \
         NULL)                                      \

#define _PRINT(a, b)                                              \
    lprintf("TEST" FORMAT_SPECIFIER_OF(a), a)

/* #define _PRINT(a, b)                                              \ */
/*     _wrap_tid(lprintf, FORMAT_SPECIFIER_OF(a), a) */

/* OK. */
/* #define _PRINT(a, b)                                              \ */
/*     log("%d\n", NUM_ARGS(FORMAT_SPECIFIER_OF(a), 1)) */

/* #define _PRINT(a, b)                            \ */
/*     lprintf(FIRST_ARG("%s\n", FORMAT_SPECIFIER_OF(a)), "TEST"); */

/* OK */
/* #define _PRINT(a, b)                                            \ */
/*     lprintf("%s\n"                                              \ */
/*             COMMA_AND_TAIL_ARGS(dummy, FORMAT_SPECIFIER_OF(a))); */

/* #define PRINT(a, b)                                              \ */
/*     printf(APPEND_STRING((a), sda),                              \ */
/*            a) */


#define PINT(n)                                 \
    log(#n" = %d", n)
#define PUNT(n)                                 \
    log(#n" = %u", n)
#define PLUNT(n)                                \
    log(#n" = %lu", n)
/* "Print hex unsigned int." */
#define PHUNT(n)                                \
    log(#n" = 0x%08X", (unsigned int)(n))
#define PPNT(n)                                 \
    log(#n" = %p", n)
#define PCHR(n)                                 \
    log(#n" = %c", n)
#define PSTR(n)                                 \
    log(#n" = %s", n)
#define PFLT(n)                                 \
    log(#n" = %f", n)


#define _PINT(lvl, n)                            \
    _log(lvl, #n" = %d", n)
#define _PUNT(lvl, n)                            \
    _log(lvl, #n" = %u", n)
#define _PLUNT(lvl, n)                           \
    _log(lvl, #n" = %lu", n)
#define _PHUNT(lvl, n)                           \
    _log(lvl, #n" = 0x%08X", (unsigned int)(n))
#define _PPNT(lvl, n)                            \
    _log(lvl, #n" = %p", n)
#define _PCHR(lvl, n)                            \
    _log(lvl, #n" = %c", n)
#define _PSTR(lvl, n)                            \
    _log(lvl, #n" = %s", n)

/* "Stringify int." */
#define SINT(n)                                 \
    #n" = %d", n
#define SLINT(n)                                \
    #n" = %ld", n
#define SPNT(n)                                 \
    #n" = %p", p                                

/* @brief Print a struct which has had a printer declared for it via
   DECLARE_STRUCT_PRINTER. */
#define PSTRUCT(n, type_of_n)                     \
    log(#n" = ("#type_of_n" *) %p : ", n);        \
    CONCAT( pretty_print_ , type_of_n )(n)              

/** 
 * @brief Given the type of a struct, along with the names of some subset of
 * its fields, declare a static inline non-macro printer function to print out
 * each of those fields in hex.
 *
 * Danger: For now, this will only print types which can be cast to unsigned
 * int.
 *
 * Danger: You have to pass at least one field name, and you can't pass more
 * than 20.
 *
 * The function will be named "pretty_print_[struct_type_t]", where
 * [struct_type_t] is the declared type of the struct. eg: "print_thread_t".
 *
 * The point is that PSTRUCT() will be able to find this function later on,
 * and so you'll be able to call PSTRUCT() on your struct to print it out.
 *
 */
#define DECLARE_SIMPLE_STRUCT_PRINTER(struct_type_t, ...)               \
    static inline void                                                  \
    CONCAT( pretty_print_ , struct_type_t )(struct_type_t *struct_ptr)  \
    {                                                                   \
        log("{");                                                       \
        ITERATE_FUNC_OVER_ARGS( PRINT_SIMPLE_FIELD, DUMMY, __VA_ARGS__); \
        log("} (size = %d)", sizeof(struct_type_t));                    \
    }

#define PRINT_SIMPLE_FIELD(field, DUMMY)                        \
    log("    "#field" = 0x%08X",                                \
        (unsigned int) struct_ptr->field );


#define DECLARE_STRUCT_PRINTER(struct_type_t, ...)                      \
    static inline void                                                  \
    CONCAT( pretty_print_ , struct_type_t )(struct_type_t *struct_ptr)  \
    {                                                                   \
        log("{");                                                       \
        ITERATE_FUNC_OVER_ARGPAIRS( PRINT_FIELD, DUMMY, __VA_ARGS__);   \
        log("} (size = %d)", sizeof(struct_type_t));                    \
    }

#define PRINT_FIELD(field, field_type, DUMMY)                   \
    log("    "#field" = %"#field_type"",                        \
        struct_ptr->field );


#endif  /* PEB_PRETTY_PRINT_H */

