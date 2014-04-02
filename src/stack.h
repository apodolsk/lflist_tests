#pragma once

#include <peb_util.h>

typedef struct sanchor{
    struct sanchor *n;
} sanchor;
#define SANCHOR {}

typedef volatile struct lfstack{
    sanchor *top;
    struct{
        uptr gen:WORDBITS/2;
        uptr size:WORDBITS/2;
    };
} lfstack;
#define LFSTACK {}
CASSERT(sizeof(lfstack) == sizeof(dptr));

typedef struct stack{
    sanchor *top;
    cnt size;
} stack;
#define STACK {}

cnt lfstack_push(sanchor *a, lfstack *s);
sanchor *lfstack_pop(lfstack *s);
cnt lfstack_size(lfstack *s);
stack lfstack_pop_all(lfstack *s);

sanchor *stack_pop(stack *s);
void stack_push(sanchor *a, stack *s);


