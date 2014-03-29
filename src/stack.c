#define MODULE STACKM

#include <stack.h>
#include <atomics.h>
#include <global.h>

cnt lfstack_push(sanchor *a, lfstack *s){
    assert(!a->n);
    while(1){
        lfstack x = *s;
        a->n = x.top;
        CASSERT(sizeof(x) == sizeof(dptr));
        if(cas2_ok(((lfstack){a, x.gen, x.size+1}), s, x))
            return x.size;
    }
}

sanchor *lfstack_pop(lfstack *s){
    while(1){
        lfstack x = *s;
        if(!x.top)
            return NULL;
        if(cas2_ok(((lfstack){x.top->n, x.gen+1, x.size-1}), s, x)){
            x.top->n = NULL;
            return x.top;
        }
    }
}

cnt lfstack_size(lfstack *s){
    return s->size;
}

stack lfstack_pop_all(lfstack *s){
    while(1){
        lfstack x = *s;
        if(!x.top || cas2_ok(((lfstack){NULL, x.gen+1, 0}), s, x))
            return (stack){x.top, x.size};
    }
}

void stack_push(sanchor *a, stack *s){
    assert(!a->n);
    assert(a != s->top);
    
    a->n = s->top;
    s->top = a;
    s->size++;
}

sanchor *stack_pop(stack *s){
    sanchor *t = s->top;
    if(!t)
        return NULL;
    s->top = t->n;
    s->size--;
    
    t->n = NULL;
    return t;
}
