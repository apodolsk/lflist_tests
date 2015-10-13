#pragma once

#include <dial_util.h>

typedef volatile struct sanchor{
    volatile struct sanchor *n;
} sanchor;
#define SANCHOR {}

typedef volatile struct stack{
    sanchor *top;
} stack;
#define STACK {}
#define pudef (stack, "(stack){top:%}", a->top)
#include <pudef.h>

sanchor *stack_pop(stack *s);
void stack_push(sanchor *a, stack *s);
cnt stack_size(const stack *s);
bool stack_empty(const stack *s);

align(sizeof(dptr))
typedef volatile struct lfstack{
    sanchor *top;
    uptr gen;
} lfstack;
#define LFSTACK {}
#define GEN_LFSTACK(_gen) {.gen = _gen}
#define pudef (lfstack, "(lfstack){top:%,gen:%}", a->top, a->gen)
#include <pudef.h>


uptr lfstack_push(sanchor *a, lfstack *s);
sanchor *lfstack_pop(lfstack *s);
stack lfstack_pop_all(cnt incr, lfstack *s);

uptr lfstack_gen(const lfstack *s);
bool lfstack_empty(const lfstack *s);
sanchor *lfstack_peek(const lfstack *s);

uptr lfstack_push_iff(sanchor *a, uptr gen, lfstack *s);
sanchor *lfstack_pop_iff(sanchor *head, uptr gen, lfstack *s);
stack lfstack_pop_all_or_incr(cnt incr, lfstack *s);
lfstack lfstack_pop_all_iff(uptr newg, lfstack *s, uptr oldg);

stack lfstack_convert(lfstack *s);

typedef volatile void spanc;
typedef spanc *(spanc_reader)(spanc *);
typedef void (spanc_writer)(spanc *, spanc *);
cnt lfstack_push_spanc(spanc *a, lfstack *s, void (spanc_write)(spanc *, spanc *));
spanc *lfstack_pop_spanc(lfstack *s, spanc *(spanc_read)(spanc *));
spanc *lfstack_pop_spanc_iff(uptr gen, uptr size, lfstack *s,
                             spanc *(spanc_read)(spanc *));

sanchor *sanchor_next(sanchor *a);

#define lfstack_push(as...) trace(STACKM, 1, lfstack_push, as)
#define lfstack_pop(as...) trace(STACKM, 1, lfstack_pop, as)
#define lfstack_pop_all(as...) trace(STACKM, 1, lfstack_pop_all, as)

#define lfstack_push_iff(as...) trace(STACKM, 1, lfstack_push_iff, as)
#define lfstack_pop_iff(as...) trace(STACKM, 1, lfstack_pop_iff, as)
#define lfstack_pop_all_or_incr(as...) trace(STACKM, 1, lfstack_pop_all_or_incr, as)
#define lfstack_pop_all_iff(as...) trace(STACKM, 1, lfstack_pop_all_iff, as)

#define stack_pop(as...) trace(STACKM, 1, stack_pop, as)
#define stack_push(as...) trace(STACKM, 1, stack_push, as)

