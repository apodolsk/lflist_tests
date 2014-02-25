/**
 * @file   stack.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * @date   Fri Oct 19 02:07:12 2012
 * 
 * @brief  
 * 
 */

#ifndef STACK_H
#define STACK_H

#include <peb_macros.h>

typedef struct sanchor_t{
    struct sanchor_t *next;
} sanchor_t;

#define FRESH_SANCHOR { .next = NULL }

typedef union{
    struct{
        int32_t tag;
        int32_t size;
        sanchor_t *ptr;
    };
    /* This is necessary because casting tagptr_t* to __int128_t* ("type
       punning") obviously breaks aliasing rules and GCC likes to optimize out
       things that "can't exist" in standard C. Caused my stack to stop
       working on -O3 on the cluster GCC version, but not at home. That was a
       fun debugging adventure. */
    __int128_t raw;
} tagptr_t;

COMPILE_ASSERT(sizeof(tagptr_t) == 16);

typedef struct{
    /* Yep, the volatile is here because GCC optimized out the read from top
       in the cmpxchg loop. Tune in for next week's edition of Fuck You GCC!
       */
    volatile tagptr_t top __attribute__((__aligned__ (16)));
}lfstack_t;

#define FRESH_STACK                                       \
    {                                                     \
        .top = {.tag = 0, .ptr = NULL, .size = 0},        \
    }
                                                                        
void stack_push(sanchor_t *anc, lfstack_t *stack);
int stack_size(lfstack_t *stack);
sanchor_t *stack_pop(lfstack_t *stack);
sanchor_t *stack_pop_all(lfstack_t *stack, int *size);

#define lookup_sanchor(ptr, container_type, field)    \
    container_of(ptr, container_type, field)          

#define stack_pop_lookup(container_type, field, stack)      \
    lookup_sanchor(stack_pop(stack), container_type, field) 

#define FOR_EACH_SPOP_LOOKUP(cur_struct, struct_type, field_name, stack)\
    for(                                                                \
        cur_struct = stack_pop_lookup(struct_type, field_name, stack);  \
        cur_struct != NULL;                                             \
        cur_struct = stack_pop_lookup(struct_type, field_name, stack)   \
        )                                                               \

/* Hey! You! Close your eyes for a sec. */
#define FOR_EACH_SPOPALL_LOOKUP(cur_struct, tmp, struct_type, field_name, stack) \
    for(                                                                \
        (cur_struct = lookup_sanchor(stack_pop_all(stack),              \
                                     struct_type,                       \
                                     field_name))                       \
            , (tmp = cur_struct ?                                       \
               lookup_sanchor(cur_struct->field_name.next,              \
                              struct_type,                              \
                              field_name)                               \
               : NULL);                                                 \
        cur_struct != NULL;                                             \
        cur_struct = tmp                                                \
            , (tmp = cur_struct ?                                       \
               lookup_sanchor(cur_struct->field_name.next,              \
                              struct_type,                              \
                              field_name)                               \
               : NULL)                                                  \
        )                                                               \
/* You can open your eyes now. */
    

typedef struct simpstack_t{
    sanchor_t *top;
    size_t size;
} simpstack_t;

#define FRESH_SIMPSTACK {.top = NULL, .size = 0 }

void simpstack_push(sanchor_t *sanc, simpstack_t *stack);
sanchor_t *simpstack_pop(simpstack_t *stack);
sanchor_t *simpstack_peek(simpstack_t *stack);
void simpstack_replace(sanchor_t *new_head, simpstack_t *stack, int size);

#define simpstack_pop_lookup(container_type, field, stack)      \
    lookup_sanchor(simpstack_pop(stack), container_type, field) 

#endif
