#define MODULE LIST

#include <list.h>
#include <peb_util.h>
#include <pthread.h>
#include <global.h>

void list_add_before(lanchor_t *a, lanchor_t *n, list_t *l){
    a->n = n;
    a->p = n->p;
    n->p->n = a;
    n->p = a;
    l->size++;
}

void list_add_after(lanchor_t *a, lanchor_t *p, list_t *l){
    a->n = p->n;
    a->p = p;
    p->n->p = a;
    p->n = a;
    l->size++;
}

void list_add_front(lanchor_t *a, list_t *l){
    list_add_after(a, &l->nil, l);
}

void list_add_rear(lanchor_t *a, list_t *l){
    list_add_before(a, &l->nil, l);
}

void list_remove(lanchor_t *a, list_t *l){
    a->n->p = a->p;
    a->p->n = a->n;
    l->size--;
    *a = (lanchor_t) FRESH_LANCHOR;
}

lanchor_t *list_find(lcomp_t comparator, void *key, list_t *l){
    lanchor_t *c;
    LIST_FOR_EACH(c, l)
        if(comparator(c, key))
            return c;
    return NULL;
}

int list_contains(lanchor_t *a, list_t *l){
    lanchor_t *c;
    LIST_FOR_EACH(c, l)
        if(a == c)
            return 1;
    return 0;
}

lanchor_t *list_nth(unsigned int n, list_t *l){
    lanchor_t *c;
    LIST_FOR_EACH(c, l)
        if(n-- == 0)
            return c;
    return c;
}

lanchor_t *list_peek(list_t *l){
    lanchor_t *head = l->nil.n;
    return head == &l->nil ? NULL : head;
}

lanchor_t *list_pop(list_t *l){
    lanchor_t *a = list_peek(l);
    if(a)
        list_remove(a, l);
    return a;
}

lanchor_t *circlist_next(lanchor_t *a, list_t *l){
    return a->n == &l->nil ? l->nil.n : a->n;
}

lanchor_t *circlist_prev(lanchor_t *a, list_t *l){
    return a->p == &l->nil ? l->nil.p : a->p;
}

int lanchor_unused(lanchor_t *a){
    return a->n == NULL && a->p == NULL;
}

int lanchor_valid(lanchor_t *a, list_t *l){
    assert(a != &l->nil);
    rassert(a->n->p, ==, a);
    rassert(a->p->n, ==, a);
    assert2(list_contains(a, l));
    return 1;
}

int list_valid(list_t *l){
    lanchor_t *c;
    LIST_FOR_EACH(c, l)
        rassert(c->p->n, ==, c);
    return 1;
}

