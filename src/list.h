#pragma once
#include <pustr.h>

typedef struct lanchor{
    struct lanchor *n;
    struct lanchor *p;
} lanchor;
#define LANCHOR {}

typedef struct{
    lanchor nil;
    uptr size;
} list;
#define LIST(l) { .nil = {&(l)->nil, &(l)->nil} }

#define LIST_FOR_EACH(cur, list)                                    \
    for(cur = list->nil.n; cur != &list->nil; cur = cur->n)

void list_add_front(lanchor *a, list *l);
void list_add_rear(lanchor *a, list *l);

void list_add_before(lanchor *a, lanchor *beforehis, list *l);
void list_add_after(lanchor *a, lanchor *afterhis, list *l);

void list_remove(lanchor *a, list *l);

typedef int (lcomp)(lanchor *to_compare, void *key);
lanchor *list_find(lcomp comparator, void *key, list *list);

int list_contains(lanchor *anchor, list *list);
lanchor *list_nth(unsigned int n, list *list);

lanchor *list_pop(list *l);
lanchor *list_peek(list *l);

uptr list_size(list *list);

lanchor *circlist_next(lanchor *a, list *l);
lanchor *circlist_prev(lanchor *a, list *l);

int lanchor_unused(lanchor *a);
int lanchor_valid(lanchor *a, list *list);

pudef(lanchor, "{n:%p, p:%p}", a->n, a->p);
pudef(list, "LIST{%s:%s}", pustr(a->nil, lanchor), pustr(a->size));
