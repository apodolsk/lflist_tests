/**
 * @file   peb_macros.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief  Some globally-useful macros.
 *
 * This is going to seem pretty crazy and/or silly. It's something that I
 * did for fun when I was most burnt out.
 *
 * I'm going to put scare quotes around "zero" everywhere. The reason is that
 * it becomes relevant that, in a C function call, '()' is a list of 0
 * expressions, but in a macro call, '()' is an arglist containing a single
 * trivially comma-separated empty argument. Even though a lot of these macros
 * have to do precisely with counting arguments, it's possible and convenient
 * for me to refer, in my comments, to 'MACRO()' as a macro given "zero"
 * arguments.
 */

#ifndef __PEB_MACROS_H__
#define __PEB_MACROS_H__

#include <stddef.h>
#include <stdint.h>

#define DUMMY_ARG 0

#define CONCAT2(x, y) x##y
#define CONCAT(x, y) CONCAT2(x, y)

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

/** 
 * @brief Given a pointer to a member of a struct, and given a description
 * of what struct that is and which field in the struct that pointer
 * occupies, return a pointer to the actual struct containing the member. 
 *
 * Idea: we know the layout of structs at compile time, via offsetof. 
 * 
 * @param member_ptr A pointer to some member of a struct instance.
 * @param container_type The type of the struct to which member_ptr belongs.
 * @param field_name The name of the field which member_ptr occupies inside
 * of its containing struct.
 * 
 * @return A pointer to the struct which contains member_ptr. If member_ptr
 * is actually NULL, then return NULL.
 */
#define container_of(member_ptr, container_type, field_name)    \
    ((container_type *)                                         \
     subtract_if_not_null(member_ptr,                           \
                          offsetof(container_type, field_name)))

/* Used to do a NULL check without expanding member_ptr twice.
   At first, I thought "subtrahend" would sound cool. I was wrong.*/
static inline void *subtract_if_not_null(void *ptr, size_t subtrahend){
    return ptr == NULL ? ptr : (void *)((uint8_t *)ptr - subtrahend);
}

/** 
 * @brief Store the value of the given expression inside a volatile variable
 * with the given name. I used this to make certain values readable from
 * simics.
 */
#ifndef NDEBUG
#define DBG_VOLATIZE(name, expression)                             \
    volatile __typeof__ (expression) name = expression;            \
    (void) name
#else
#define DBG_VOLATIZE(...) ERROR_SOMEONE_FORGOT_TO_REMOVE_DBG_CODE
#endif


/** 
 * @brief Trigger a compile-time error if an expression e evaluates to 0.
 *
 * Only valid at file scope.
 */
/* #define COMPILE_ASSERT(e)                       \ */
/*     ((void) sizeof(struct { int:-!(e); })) */

#define COMPILE_ASSERT(e)                                               \
    extern struct{ int:-!(e); } CONCAT(__cassert_tmp_var, __COUNTER__)  \
    __attribute__ ((unused))   

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

/*
  ----------------------------------------------------------------------
  
   The next two macros, FIRST_ARG and COMMA_AND_TAIL_ARGS, are a pure C99
   way to isolate parts of a variadic arg list without assuming that it's of
   form "(first, ...)" and without using GCC's ##__VA_ARGS__ to handle
   commas.

   Used in _log and the *_ERROR macros.

   ----------------------------------------------------------------------
*/

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
 *
 * I think this general technique is pretty cool. If you can express some
 * information in terms of a number of arguments, then you can do whatever
 * you want with it.
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
 * @brief Macro-expand the given arg(s) and return the second element of the
 * comma-separated list that should result.
 *
 * You can can't pass less than two arguments.
 */
#define SECOND_ARG(...) _SECOND_ARG(__VA_ARGS__, DUMMY)
#define _SECOND_ARG(a, b, ...) b

#define THIRD_ARG(...) _THIRD_ARG(__VA_ARGS__, DUMMY)
#define _THIRD_ARG(a, b, c, ...) c


/** 
 * @brief Apply the given macro function once to each of the args in our
 * argument list, up to 20 args, one by one. Each invocation is also passed
 * the provided global arg, which is intended to let you reuse functions like
 * you would if you could curry them.
 *
 * Idea: hardcode an iterative nested macro expansion, and use NUM_ARGS +
 * CONCAT to decide what depth of this iteration to jump into for a given
 * invocation.
 */
#define ITERATE_FUNC_OVER_ARGS(FUNC, global, ...)                \
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
#define ITERATE_FUNC_OVER_ARGPAIRS(FUNC, global, ...)               \
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


/* This one didn't quite work out. It works, but it iterates from limit to
   0, when you typically (and I definitely, in the place that I wanted to
   use it) want to iterate from 0 to limit. That's not hard to do, but my
   intended application is so trivial that I'm going to stop here. */
#define ITERATE_FUNC_OVER_NUMS(FUNC, limit, ...)        \
    CONCAT( NITR_ ,                                     \
            limit ) (FUNC, __VA_ARGS__)                 \

#define NITR_20(FUNC, ...)                              \
    FUNC(20, __VA_ARGS__) NITR_19(FUNC, __VA_ARGS__)
#define NITR_19(FUNC, ...)                              \
    FUNC(19, __VA_ARGS__) NITR_18(FUNC, __VA_ARGS__)
#define NITR_18(FUNC, ...)                              \
    FUNC(18, __VA_ARGS__) NITR_17(FUNC, __VA_ARGS__)
#define NITR_17(FUNC, ...)                              \
    FUNC(17, __VA_ARGS__) NITR_16(FUNC, __VA_ARGS__)
#define NITR_16(FUNC, ...)                              \
    FUNC(16, __VA_ARGS__) NITR_15(FUNC, __VA_ARGS__)
#define NITR_15(FUNC, ...)                              \
    FUNC(15, __VA_ARGS__) NITR_14(FUNC, __VA_ARGS__)
#define NITR_14(FUNC, ...)                              \
    FUNC(14, __VA_ARGS__) NITR_13(FUNC, __VA_ARGS__)
#define NITR_13(FUNC, ...)                              \
    FUNC(13, __VA_ARGS__) NITR_12(FUNC, __VA_ARGS__)
#define NITR_12(FUNC, ...)                              \
    FUNC(12, __VA_ARGS__) NITR_11(FUNC, __VA_ARGS__)
#define NITR_11(FUNC, ...)                              \
    FUNC(11, __VA_ARGS__) NITR_10(FUNC, __VA_ARGS__)
#define NITR_10(FUNC, ...)                              \
    FUNC(10, __VA_ARGS__) NITR_9(FUNC, __VA_ARGS__)
#define NITR_9(FUNC, ...)                           \
    FUNC(9, __VA_ARGS__) NITR_8(FUNC, __VA_ARGS__)
#define NITR_8(FUNC, ...)                           \
    FUNC(8, __VA_ARGS__) NITR_7(FUNC, __VA_ARGS__)
#define NITR_7(FUNC, ...)                           \
    FUNC(7, __VA_ARGS__) NITR_6(FUNC, __VA_ARGS__)
#define NITR_6(FUNC, ...)                           \
    FUNC(6, __VA_ARGS__) NITR_5(FUNC, __VA_ARGS__)
#define NITR_5(FUNC, ...)                           \
    FUNC(5, __VA_ARGS__) NITR_4(FUNC, __VA_ARGS__)
#define NITR_4(FUNC, ...)                           \
    FUNC(4, __VA_ARGS__) NITR_3(FUNC, __VA_ARGS__)
#define NITR_3(FUNC, ...)                           \
    FUNC(3, __VA_ARGS__) NITR_2(FUNC, __VA_ARGS__)
#define NITR_2(FUNC, ...)                           \
    FUNC(2, __VA_ARGS__) NITR_1(FUNC, __VA_ARGS__)
#define NITR_1(FUNC, ...)                           \
    FUNC(1, __VA_ARGS__) NITR_0(FUNC, __VA_ARGS__)
#define NITR_0(FUNC, ...)                       \
    FUNC(0, __VA_ARGS__)

#define CONSTRUCT_LIST_ELEM(itr, elem) elem , 
#define REPEATING_LIST(elem, repetitions)                           \
    ITERATE_FUNC_OVER_NUMS(CONSTRUCT_LIST_ELEM, repetitions, elem)

#endif /* __PEB_MACROS_H__ */
