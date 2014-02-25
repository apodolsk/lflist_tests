/**
 * @file   stack.c
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * @date   Sun Oct 14 04:37:31 2012
 * 
 * @brief  
 * 
 * 
 */

#define MODULE STACK

#include <stack.h>
#include <asm_util.h>
#include <global.h>

#define MAX_FAILURES 5

void stack_push(sanchor_t *anc, lfstack_t *stack){
    trace(anc, p);

    tagptr_t top;
    tagptr_t new_top;
    int loops = 0;
    (void) loops;

    rassert(anc->next, ==, NULL);
    rassert(stack->top.ptr, !=, anc);
    
    /* ABA is impossible with just pushes, so it's better not to make
       tagptr collisions more likely by incrementing tag. */
    do{
        /* assert(loops++ <= MAX_FAILURES); */
        top = stack->top;
        anc->next = top.ptr;
        new_top.ptr = anc;
        new_top.tag = top.tag;
        new_top.size = top.size + 1;
    } while(stack->top.ptr != top.ptr ||
            cmpxchg128b(new_top.raw,
                       &stack->top.raw,
                       top.raw)
            != top.raw);

    /* xadd(1, &stack->size); */
}

sanchor_t *stack_pop(lfstack_t *stack){
    trace(stack, p);

    tagptr_t old;
    tagptr_t new;
    int loops = 0;
    (void) loops;

    assert(aligned(&stack->top, 8));

    do{
        /* assert(loops++ <= MAX_FAILURES); */
        old = stack->top;
        if(old.ptr == NULL)
            return NULL;
        new.tag = old.tag + 1;
        /* Even if out of date, this should always be a readable ptr. */
        new.ptr = old.ptr->next;
        new.size = old.size - 1;
    } while(stack->top.tag != old.tag ||
            cmpxchg128b(new.raw,
                        &stack->top.raw,
                        old.raw)
            != old.raw);

    old.ptr->next = NULL;
    
    return old.ptr;
}

int stack_size(lfstack_t *stack){
    return stack->top.size;
}

sanchor_t *stack_pop_all(lfstack_t *stack, int *size){
    /* This would be simpler with xchg128b, but no such instruction
       exists. However, it turns out that xchg is worse than cmpxchg because
       it always assert #LOCK on the bus while cmpxchg is able to use the
       cache coherency mechanism to avoid that. The exact details are
       uspecified for good reasons by Intel, but you can kind of imagine what
       might be going on. See Intel Programmer's Guide section Bus Locking as
       well as:
       https://lkml.org/lkml/2010/12/18/59
    */

    tagptr_t new_top = {0};
    tagptr_t top;
    do{
        if(!stack->top.ptr)
            return NULL;
        top = stack->top;
    } while(cmpxchg128b(new_top.raw,
                        &stack->top.raw,
                        top.raw)
            != top.raw);

    *size = top.size;
    return top.ptr;
}


void simpstack_push(sanchor_t *sanc, simpstack_t *stack){
    trace(sanc, p, stack, p);
    
    assert(!sanc->next);
    assert(sanc != stack->top);
    
    sanc->next = stack->top;
    stack->top = sanc;
    stack->size++;
}
sanchor_t *simpstack_pop(simpstack_t *stack){
    sanchor_t *out = stack->top;
    if(!out)
        return NULL;
    stack->top = out->next;
    out->next = NULL;
    stack->size--;

    trace(stack, p, out, p);
    
    return out;
}

void simpstack_replace(sanchor_t *new_head, simpstack_t *stack, int size){
    trace(new_head, p, stack, p, size, d);
    
    assert(stack->top == NULL);
    stack->top = new_head;
    stack->size = size;
}


sanchor_t *simpstack_peek(simpstack_t *stack){
    return stack->top;
}



    
