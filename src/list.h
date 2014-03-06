#pragma once

typedef struct lanchor_t{
    struct lanchor_t *n;
    struct lanchor_t *p;
} lanchor_t;
#define FRESH_LANCHOR {}

typedef struct{
    lanchor_t nil;
    unsigned int size;
} list_t;
#define FRESH_LIST(l) { .nil = {&(l)->nil, &(l)->nil} }

#define LIST_FOR_EACH(cur, list)                                    \
    for(cur = list->nil.n; cur != &list->nil; cur = cur->n);

void list_add_front(lanchor_t *a, list_t *l);
void list_add_rear(lanchor_t *a, list_t *l);

void list_add_before(lanchor_t *a, lanchor_t *before_this, list_t *l);
void list_add_after(lanchor_t *a, lanchor_t *after_this, list_t *l);

void list_remove(lanchor_t *a, list_t *l);

typedef int (lcomp_t)(lanchor_t *to_compare, void *key);
lanchor_t *list_find(lcomp_t comparator, void *key, list_t *list);

int list_contains(lanchor_t *anchor, list_t *list);
lanchor_t *list_nth(unsigned int n, list_t *list);

lanchor_t *list_pop(list_t *l);
lanchor_t *list_peek(list_t *l);

lanchor_t *circlist_next(lanchor_t *a, list_t *l);
lanchor_t *circlist_prev(lanchor_t *a, list_t *l);

int lanchor_unused(lanchor_t *a);
int lanchor_valid(lanchor_t *a, list_t *list);
