/**
 * @file   list.h
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief  The interface to the list.
 * 
 */

#ifndef __LIST_H__
#define __LIST_H__

#include <peb_util.h>

/** 
 * @brief Embeddable list traversal field.
 */
typedef struct lanchor_t{
    struct lanchor_t *next;
    struct lanchor_t *prev;    
} lanchor_t;

typedef struct {
    lanchor_t *head;
    lanchor_t *tail;

    unsigned int size;
} list_t;

#define FRESH_LANCHOR                     \
    {                                           \
        .next = NULL,                           \
            .prev = NULL,                       \
            }


#define FRESH_LIST                        \
    {                                           \
            .head = NULL,                       \
            .tail = NULL,                       \
            .size = 0,                          \
            }

#define FOR_EACH_LANCHOR(cur_anchor, list)       \
    for(                                        \
        cur_anchor = list->head;                \
        cur_anchor != NULL;                     \
        cur_anchor = cur_anchor->next)                      

#define FOR_EACH_LLOOKUP(cur_struct, struct_type, field_name, list) \
    for(                                                            \
        cur_struct = lookup_lanchor(list_peek(list), struct_type,   \
                                    field_name);                    \
        cur_struct != NULL;                                         \
        cur_struct = lookup_lanchor(cur_struct->field_name.next,    \
                                    struct_type,                    \
                                    field_name))

#define FOR_EACH_LPOP(cur_struct, tmp, struct_type, field_name, list)   \
    for(                                                                \
        cur_struct = lookup_lanchor(list_pop_all(list), struct_type,    \
                                    field_name)                         \
            , tmp = cur_struct ?                                        \
            lookup_lanchor(cur_struct->field_name.next,                 \
                           struct_type,                                 \
                           field_name)                                  \
            : NULL;                                                     \
        cur_struct != NULL;                                             \
        cur_struct = tmp                                                \
            , tmp = cur_struct ?                                        \
            lookup_lanchor(cur_struct->field_name.next,                 \
                           struct_type,                                 \
                           field_name))

/** 
 * @brief Pop the head of the given list, and then translate the resulting
 * anchor address to the address of the struct in which that anchor is
 * located. 
 * 
 * @param struct_type The type of struct to which we shall translate the anchor
 * result of list_pop.
 * @param field_name The name of the field within the definition of
 * struct_type corresponding to the anchor which list_pop will return.
 * @param list The list from which to pop. 
 * 
 * @return Evaluates to the struct to which the result of list_pop belonged.
 */
#define list_pop_lookup(struct_type, field_name, list)      \
    lookup_lanchor(list_pop(list), struct_type, field_name)

#define list_peek_lookup(struct_type, field_name, list)         \
    lookup_lanchor(list_peek(list), struct_type, field_name)

#define list_nth_lookup(struct_type, field_name, n, list)       \
    lookup_lanchor(list_nth(n, list), struct_type, field_name)

#define list_find_lookup(struct_type, field_name, cmp, key, list)       \
    lookup_lanchor(list_find(cmp, key, list), struct_type, field_name)   

/* Wrapper for container_of. See: peb_util.h */
#define lookup_lanchor(member_ptr, struct_type, field_name) \
    container_of(member_ptr, struct_type, field_name)        

typedef int (lcomp_t)(lanchor_t *to_compare, void *key);

void list_add_front(lanchor_t *anchor, list_t *list);
void list_add_rear(lanchor_t *anchor, list_t *list);
void list_add_before(lanchor_t *anchor, lcomp_t *comp,
                     void *key, list_t *list);

lanchor_t *list_peek(list_t *list);

lanchor_t *list_pop(list_t *list);
lanchor_t *list_pop_all(list_t *list);
void list_remove(lanchor_t *anchor, list_t *list);

lanchor_t *list_find(lcomp_t comparator, void *key, list_t *list);

int list_contains(lanchor_t *anchor, list_t *list);
lanchor_t *list_nth(unsigned int n, list_t *list);

int list_empty(list_t *list);
int list_size(list_t *list);

int anchor_unused(lanchor_t *anchor, list_t *list);
int lanchor_valid(lanchor_t *anchor, list_t *list);

void list_invalidate(list_t *list);
int list_valid_quick(list_t *list);
int list_valid_slow(list_t *list);

#endif /* __LIST_H__ */
