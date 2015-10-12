#pragma once

#include <peb_util.h>

typedef volatile struct sanchor{
    volatile struct sanchor *n;
} sanchor;
#define SANCHOR {}

__attribute__((__aligned__(sizeof(dptr))))
typedef volatile struct lfstack{
    sanchor *top;
    struct{
        uptr gen:WORDBITS/2;
        uptr size:WORDBITS/2;
    };
} lfstack;
#define LFSTACK {}
CASSERT(sizeof(lfstack) == sizeof(dptr));
#define pudef (lfstack, "(lfstack){top:%,sz:%}", a->top, a->size)
#include <pudef.h>

typedef volatile struct stack{
    sanchor *top;
    cnt size;
} stack;
#define STACK {}
#define pudef (stack, "(stack){top:%,sz:%}", a->top, a->size)
#include <pudef.h>

cnt lfstack_push(sanchor *a, lfstack *s);
sanchor *lfstack_pop(lfstack *s);
cnt lfstack_size(lfstack *s);
stack lfstack_pop_all(lfstack *s);

sanchor *stack_pop(stack *s);
void stack_push(sanchor *a, stack *s);

#define lfstack_push(as...) trace(STACKM, 1, lfstack_push, as)
#define lfstack_pop(as...) trace(STACKM, 1, lfstack_pop, as)
#define lfstack_pop_all(as...) trace(STACKM, 1, lfstack_pop_all, as)

#define stack_pop(as...) trace(STACKM, 1, stack_pop, as)
#define stack_push(as...) trace(STACKM, 1, stack_push, as)

