#define MODULE STACKM

#include <stack.h>
                  

void (stack_push)(sanchor *a, stack *s){
    assert(!a->n);
    assert(a != s->top);
    
    a->n = s->top;
    s->top = a;
    s->size++;
}

sanchor *(stack_pop)(stack *s){
    sanchor *t = s->top;
    if(!t)
        return NULL;
    s->top = t->n;
    s->size--;
    
    t->n = NULL;
    return t;
}

sanchor *sanchor_next(sanchor *a){
    return a->n;
}

bool stack_empty(const stack *s){
    return !s->top;
}

cnt stack_size(const stack *s){
    return s->size;
}

sanchor *sanc_read(sanchor *a){
    return a->n;
}

void sanc_write(sanchor *n, sanchor *to){
    to->n = n;
}

cnt (lfstack_push)(sanchor *a, lfstack *s){
    assert(!a->n);
    return lfstack_push_spanc(a, s, (void (*)(spanc *, spanc *)) sanc_write);
}

cnt (lfstack_push_spanc)(spanc *a, lfstack *s, void (spanc_write)(spanc *, spanc *)){
    for(lfstack x = *s;;){
        spanc_write(x.top, a);
        if(cas2_won(rup(x, .top=a), s, &x))
            return x.gen;
    }
}

sanchor *(lfstack_pop)(lfstack *s){
    sanchor *a = lfstack_pop_spanc(s, (spanc *(*)(spanc *)) sanc_read);
    if(a)
        a->n = NULL;
    return a;
}

spanc *(lfstack_pop_spanc)(lfstack *s, spanc *(spanc_read)(spanc *)){
    for(lfstack x = *s;;){
        if(!x.top)
            return NULL;
        if(cas2_won(rup(x, .top=spanc_read(x.top), .gen++), s, &x)){
            return x.top;
        }
    }
}

sanchor *(lfstack_pop_iff)(sanchor *head, uptr gen, lfstack *s){
    if(!head)
        return NULL;
    for(lfstack x = *s;;){
        if(x.top != head || x.gen != gen)
            return NULL;
        if(cas2_won(rup(x, .top=x.top->n, .gen++), s, &x)){
            x.top->n = NULL;
            return x.top;
        }
    }
}

bool lfstack_empty(const lfstack *s){
    return !s->top;
}

uptr lfstack_gen(const lfstack *s){
    return s->gen;
}

sanchor *lfstack_peek(const lfstack *s){
    return s->top;
}

uptr (lfstack_push_iff)(sanchor *a, uptr gen, lfstack *s){
    for(lfstack x = *s;;){
        if(x.gen != gen)
            return x.gen;
        a->n = x.top;
        if(cas2_won(rup(x, .top = a), s, &x))
            return x.gen;
    }
}

/* Callers can avoid gen updates if they only ever pop_all from s, or can
   arrange for it to communicate something extra. */
stack (lfstack_pop_all)(cnt incr, lfstack *s){
    for(lfstack x = *s;;){
        if((!x.top && !incr)
           || cas2_won(rup(x, .top=NULL, .gen+=incr), s, &x))
            return (stack){x.top, 0};
    }
}

stack lfstack_pop_all_or_incr(cnt incr, lfstack *s){
    for(lfstack x = *s;;){
        if(!x.top){
            if(cas2_won(rup(x, .gen += incr), s, &x))
                return (stack){x.top, 0};
        }else if(cas2_won(rup(x, .top=NULL), s, &x))
            return (stack){x.top, 0};
    }
}

lfstack (lfstack_pop_all_iff)(uptr newg, lfstack *s, uptr oldg){
    for(lfstack x = *s;;){
        if(x.gen != oldg)
            return x;
        if(cas2_won(rup(x, .top=NULL, .gen=newg), s, &x))
            return x;
    }
}

