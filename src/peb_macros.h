#pragma once

#include <stddef.h>
#include <stdint.h>

#define DUMMY_ARG 0
#define CONCAT2(x, y) x##y
#define CONCAT(x, y) CONCAT2(x, y)

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

/* Type pun through a union equal in size to s, but use integer conversion
   to zero out extra bits of output if s is smaller than t. */
#define PUN(t, s) ({                                                \
        CASSERT(sizeof(s) == sizeof(t));                            \
        ((union {__typeof__(s) str; t i;}) (s)).i;                  \
        })                                                      

/** 
 * Return the address of the struct s containing member_ptr, assuming s
 * has type container_type and member_ptr points to its field_name
 * field. If member_ptr is NULL, return NULL.
 */
#define cof container_of
#define container_of(member_ptr, container_type, field_name)        \
    ((container_type *)                                             \
     subtract_if_not_null((void *) member_ptr,                      \
                          offsetof(container_type, field_name)))

/* Used to do a NULL check without expanding member_ptr twice.
   At first, I thought "subtrahend" would sound cool. I was wrong.*/
static inline void *subtract_if_not_null(void *ptr, size subtrahend){
    return ptr == NULL ? ptr : (void *)((u8 *)ptr - subtrahend);
}


#define CASSERT(e) _Static_assert(e, #e)

/* #define COMPILE_ASSERT(e)                       \ */
/*     ((void) sizeof(struct { int:-!(e); })) */

/* #define COMPILE_ASSERT(e)                                               \ */
/*     extern struct{ int:-!(e); } CONCAT(__cassert_tmp_var, __COUNTER__)  \ */
/*     __attribute__ ((unused)) */

/* /\**  */
/*  * Trigger a compile-time error if an expression e evaluates to 0. */
/*  *\/ */
/* #define COMPILE_ASSERT(e)                                       \ */
/*     extern int __compile_assert[-!(e)] __attribute__ ((unused)) */

/** 
 * @brief Expands to the number of args which it has been passed, including to
 * 0 when it is passed a single argument containing no preprocessing tokens
 * ("zero" args).
 *
 * Danger: As an arbitrary limit, it's undefined for more than 40 args.
 * 
 * Idea: Take a macro that returns its n'th argument and swallows the rest,
 * and then construct a call to it in a clever way so that the n'th argument
 * happens to be equal to the number of arguments which it was passed. As the
 * number of args in __VA_ARGS__ in _NUM_ARGS increases, the smaller numbers
 * in the hardcoded count are swallowed up.
 *
 * The point of the FIRST_ARG/TAIL_ARGS is to get around the fact that this
 * approach won't correctly distinguish between 1 arg and "zero" args (since
 * __VA_ARGS__ would contain 0 commas in both cases, and would cause the same
 * number of elements of the hardcoded count to get swallowed up). The general
 * idea is to reduce the problem to one of distinguishing between 1 and 2
 * args, which is something that CAN be done. Best way to understand will be
 * to just understand FIRST_ARG/TAIL_ARGS, and then to expand things out step
 * by step.
 *
 * It might seem reasonable to just replace __VA_ARGS__ with FIRST/TAIL in
 * _NUM_ARGS, but then the FIRST/TAIL wouldn't be expanded out until AFTER it
 * was passed to the PATTERN_MATCHER as a single arg, when what you want is
 * for it to expand to 2 or 1 args to PATTERN_MATCHER as necessary.
 *
 * I learned this basic approach from Jens Gutstedt's P99 blog, although the
 * zero-args thing is original. He has his own way to detect "zero"
 * arguments. It involves hilariously exploiting a completely unrelated CPP
 * whitespace rule, but it's even harder to understand.
 *
 * http://gustedt.wordpress.com/2010/06/08/detect-empty-macro-arguments/
 */
#define NUM_ARGS(...)                                                   \
    _NUM_ARGS(FIRST_ARG(__VA_ARGS__)                                    \
              TAIL_ARGS_SURROUNDED_BY_COMMAS(__VA_ARGS__) 40)           \

#define _NUM_ARGS(...)                                                  \
    NUM_ARGS_PATTERN_MATCHER(                                           \
        __VA_ARGS__,39, 38, 37, 36, 35,                                 \
        34, 33, 32, 31, 30, 29, 28, 27, 26, 25,                         \
        24, 23, 22, 21, 20, 19, 18, 17, 16, 15,                         \
        14, 13, 12, 11, 10, 9, 8, 7, 6, 5,                              \
        4, 3, 2, 1, 0)                                                  \
    
#define NUM_ARGS_PATTERN_MATCHER(a1, a2, a3, a4, a5, a6, a7,        \
                                 a8, a9, a10, a11, a12, a13,        \
                                 a14, a15, a16, a17, a18, a19, a20, \
                                 a21, a22, a23, a24, a25, a26, a27, \
                                 a28, a29, a30, a31, a32, a33, a34, \
                                 a35, a36, a37, a38, a39, a40,      \
                                 N, ...)                            \
    N

/** 
 * @brief A variant of NUM_ARGS that expands to 1 if given 2 or more args.
 * Otherwise, expands to 0. Undefined for > 40 args.
 */
#define NUM_ARGS_G_E_2(...)                                             \
    NUM_ARGS_PATTERN_MATCHER(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                             1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)  \

/** 
 * @brief Expand to the first arg which we are passed. If given "0
 * args", expand to nothing.
 *
 * Idea: since we don't care about our tail args, append a dummy argument to
 * our arglist so that we can always assume that an arglist is of form
 * "(first, ...)".
 *
 */
#define FIRST_ARG(...)                          \
    FIRST_HELPER(__VA_ARGS__, DUMMY_ARG)

#define FIRST_HELPER(first, ...) first

/** 
 * @brief If given more than one arg, expand to a comma followed by all args
 * except for the first one. If given "zero" or one arguments, expand to
 * nothing, with no comma before this nothing.
 *
 * Idea: use CONCAT and NUM_ARGS to redirect each invocation of this macro
 * into an invocation of one of two helper macros. Each helper will properly
 * handle the number of arguments with which a particular invocation of
 * COMMA_AND_TAIL_ARGS has been made.
 *
 * Together, the two helpers cover the two incompatible arg-list forms
 * between which we need to discriminate: "(first)" and "(first, ...)".
 */
#define COMMA_AND_TAIL_ARGS(...)                        \
    CONCAT( TAIL_HELPER,                                \
            NUM_ARGS_G_E_2(__VA_ARGS__) ) (__VA_ARGS__)

#define TAIL_HELPER0(first_arg)                

#define TAIL_HELPER1(first, ...)                \
    , __VA_ARGS__

#define TAIL_ARGS_THEN_COMMA(...)                           \
    CONCAT( TAIL_ARGS_THEN_COMMA,                           \
            NUM_ARGS_G_E_2(__VA_ARGS__)) (__VA_ARGS__)    
    
#define TAIL_ARGS_THEN_COMMA0(first_arg)                

#define TAIL_ARGS_THEN_COMMA1(first, ...)       \
    __VA_ARGS__ ,

#define TAIL_ARGS_SURROUNDED_BY_COMMAS(...)                   \
    CONCAT( TAIL_ARGS_SURROUNDED_BY_COMMAS,                   \
                NUM_ARGS_G_E_2(__VA_ARGS__)) (__VA_ARGS__)    
    
#define TAIL_ARGS_SURROUNDED_BY_COMMAS0(first_arg)                

#define TAIL_ARGS_SURROUNDED_BY_COMMAS1(first, ...)   \
    , __VA_ARGS__ ,

/** 
 * Macro-expand the given arg(s) and return the second element of the
 * comma-separated list that should result.
 *
 * You can can't pass less than two arguments.
 */
#define SECOND_ARG(...) _SECOND_ARG(__VA_ARGS__, DUMMY)
#define _SECOND_ARG(a, b, ...) b

#define THIRD_ARG(...) _THIRD_ARG(__VA_ARGS__, DUMMY)
#define _THIRD_ARG(a, b, c, ...) c


/** 
 * Apply the given macro function to each of the args. Each invocation is
 * also passed the global arg.
 *
 * Idea: hardcode an iterative nested macro expansion, and use NUM_ARGS +
 * CONCAT to decide what depth of this iteration to jump into for a given
 * invocation.
 */
#define APPLY_FUNC_TO_ARGS(FUNC, global, ...)                    \
    CONCAT( ITR_ ,                                               \
            NUM_ARGS(__VA_ARGS__) ) (FUNC, global, __VA_ARGS__)  \

#define ITR_20(f, g, arg, ...) f(arg, g) ITR_19(f, g, __VA_ARGS__)
#define ITR_19(f, g, arg, ...) f(arg, g) ITR_18(f, g, __VA_ARGS__)
#define ITR_18(f, g, arg, ...) f(arg, g) ITR_17(f, g, __VA_ARGS__)
#define ITR_17(f, g, arg, ...) f(arg, g) ITR_16(f, g, __VA_ARGS__)
#define ITR_16(f, g, arg, ...) f(arg, g) ITR_15(f, g, __VA_ARGS__)
#define ITR_15(f, g, arg, ...) f(arg, g) ITR_14(f, g, __VA_ARGS__)
#define ITR_14(f, g, arg, ...) f(arg, g) ITR_13(f, g, __VA_ARGS__)
#define ITR_13(f, g, arg, ...) f(arg, g) ITR_12(f, g, __VA_ARGS__)
#define ITR_12(f, g, arg, ...) f(arg, g) ITR_11(f, g, __VA_ARGS__)
#define ITR_11(f, g, arg, ...) f(arg, g) ITR_10(f, g, __VA_ARGS__)
#define ITR_10(f, g, arg, ...) f(arg, g) ITR_9(f, g, __VA_ARGS__)
#define ITR_9(f, g, arg, ...) f(arg, g) ITR_8(f, g, __VA_ARGS__)
#define ITR_8(f, g, arg, ...) f(arg, g) ITR_7(f, g, __VA_ARGS__)
#define ITR_7(f, g, arg, ...) f(arg, g) ITR_6(f, g, __VA_ARGS__)
#define ITR_6(f, g, arg, ...) f(arg, g) ITR_5(f, g, __VA_ARGS__)
#define ITR_5(f, g, arg, ...) f(arg, g) ITR_4(f, g, __VA_ARGS__)
#define ITR_4(f, g, arg, ...) f(arg, g) ITR_3(f, g, __VA_ARGS__)
#define ITR_3(f, g, arg, ...) f(arg, g) ITR_2(f, g, __VA_ARGS__)
#define ITR_2(f, g, arg, ...) f(arg, g) ITR_1(f, g, __VA_ARGS__)
#define ITR_1(f, g, arg) f(arg, g)
#define ITR_0(f, g, arg)

/* Variation on the above. This time we pair the arguments off. 40
   arguments suddenly doesn't seem like that much. */
#define APPLY_FUNC_TO_ARGPAIRS(FUNC, global, ...)                   \
    CONCAT( PITR_ ,                                                 \
            NUM_ARGS(__VA_ARGS__) ) (FUNC, global, __VA_ARGS__)     \


#define PITR_40(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_38(f, g, __VA_ARGS__)
#define PITR_38(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_36(f, g, __VA_ARGS__)
#define PITR_36(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_34(f, g, __VA_ARGS__)
#define PITR_34(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_32(f, g, __VA_ARGS__)
#define PITR_32(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_30(f, g, __VA_ARGS__)
#define PITR_30(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_28(f, g, __VA_ARGS__)
#define PITR_28(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_26(f, g, __VA_ARGS__)
#define PITR_26(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_24(f, g, __VA_ARGS__)
#define PITR_24(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_22(f, g, __VA_ARGS__)
#define PITR_22(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_20(f, g, __VA_ARGS__)
#define PITR_20(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_18(f, g, __VA_ARGS__)
#define PITR_18(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_16(f, g, __VA_ARGS__)
#define PITR_16(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_14(f, g, __VA_ARGS__)
#define PITR_14(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_12(f, g, __VA_ARGS__)
#define PITR_12(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_10(f, g, __VA_ARGS__)
#define PITR_10(f, g, arg1, arg2, ...)          \
    f(arg1, arg2, g) PITR_8(f, g, __VA_ARGS__)
#define PITR_8(f, g, arg1, arg2, ...)           \
    f(arg1, arg2, g) PITR_6(f, g, __VA_ARGS__)
#define PITR_6(f, g, arg1, arg2, ...)           \
    f(arg1, arg2, g) PITR_4(f, g, __VA_ARGS__)
#define PITR_4(f, g, arg1, arg2, ...)           \
    f(arg1, arg2, g) PITR_2(f, g, __VA_ARGS__)
#define PITR_2(f, g, arg1, arg2)                \
    f(arg1, arg2, g)
#define PITR_0(f, g, empty)


#define ITERATE_FUNC_UP_TO(FUNC, limit, ...)            \
    CONCAT( NITR_ ,                                     \
            limit ) (FUNC, limit, __VA_ARGS__)          \

#define NITR_20(FUNC, l, ...)                           \
    FUNC(l - 20, __VA_ARGS__) NITR_19(FUNC, l, __VA_ARGS__)
#define NITR_19(FUNC, l, ...)                           \
    FUNC(l - 19, __VA_ARGS__) NITR_18(FUNC, l, __VA_ARGS__)
#define NITR_18(FUNC, l, ...)                           \
    FUNC(l - 18, __VA_ARGS__) NITR_17(FUNC, l, __VA_ARGS__)
#define NITR_17(FUNC, l, ...)                           \
    FUNC(l - 17, __VA_ARGS__) NITR_16(FUNC, l, __VA_ARGS__)
#define NITR_16(FUNC, l, ...)                           \
    FUNC(l - 16, __VA_ARGS__) NITR_15(FUNC, l, __VA_ARGS__)
#define NITR_15(FUNC, l, ...)                           \
    FUNC(l - 15, __VA_ARGS__) NITR_14(FUNC, l, __VA_ARGS__)
#define NITR_14(FUNC, l, ...)                           \
    FUNC(l - 14, __VA_ARGS__) NITR_13(FUNC, l, __VA_ARGS__)
#define NITR_13(FUNC, l, ...)                           \
    FUNC(l - 13, __VA_ARGS__) NITR_12(FUNC, l, __VA_ARGS__)
#define NITR_12(FUNC, l, ...)                           \
    FUNC(l - 12, __VA_ARGS__) NITR_11(FUNC, l, __VA_ARGS__)
#define NITR_11(FUNC, l, ...)                           \
    FUNC(l - 11, __VA_ARGS__) NITR_10(FUNC, l, __VA_ARGS__)
#define NITR_10(FUNC, l, ...)                       \
    FUNC(l - 10, __VA_ARGS__) NITR_9(FUNC, l, __VA_ARGS__)
#define NITR_9(FUNC, l, ...)                        \
    FUNC(l - 9, __VA_ARGS__) NITR_8(FUNC, l, __VA_ARGS__)
#define NITR_8(FUNC, l, ...)                        \
    FUNC(l - 8, __VA_ARGS__) NITR_7(FUNC, l, __VA_ARGS__)
#define NITR_7(FUNC, l, ...)                        \
    FUNC(l - 7, __VA_ARGS__) NITR_6(FUNC, l, __VA_ARGS__)
#define NITR_6(FUNC, l, ...)                        \
    FUNC(l - 6, __VA_ARGS__) NITR_5(FUNC, l, __VA_ARGS__)
#define NITR_5(FUNC, l, ...)                        \
    FUNC(l - 5, __VA_ARGS__) NITR_4(FUNC, l, __VA_ARGS__)
#define NITR_4(FUNC, l, ...)                        \
    FUNC(l - 4, __VA_ARGS__) NITR_3(FUNC, l, __VA_ARGS__)
#define NITR_3(FUNC, l, ...)                        \
    FUNC(l - 3, __VA_ARGS__) NITR_2(FUNC, l, __VA_ARGS__)
#define NITR_2(FUNC, l, ...)                        \
    FUNC(l - 2, __VA_ARGS__) NITR_1(FUNC, l, __VA_ARGS__)
#define NITR_1(FUNC, l, ...)                        \
    FUNC(l - 1, __VA_ARGS__) NITR_0(FUNC, l, __VA_ARGS__)
#define NITR_0(FUNC, l, ...)                    \
    FUNC(0, __VA_ARGS__)
