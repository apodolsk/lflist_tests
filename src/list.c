/**
 * @file   list.c
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief A generic doubly-linked list based on embedded anchors and
 * container_of().
 *
 * Each listworthy object has an embedded lanchor_t. List operations use
 * lanchor_t's as nodes. After using the code here to find the right
 * lanchor_t, I use container_of (via lookup_anchor & co.) to look up the
 * enveloping struct.
 *
 * Lists can contain up to UINT_MAX anchors.
 *
 * Lists can be used with any type of object, and objects can be placed
 * on multiple lists simultaneously, or multiple times on a single list (in
 * theory; I don't do this). This implementation makes minimal use of the
 * preprocessor, but the programmer has to keep track of which anchor in an
 * object is associated with which list.
 *
 * I found this approach to be super comfortable. I'd definitely recommend
 * it to other students. Maybe the suggested approach gives more macro
 * practice, but I'd guess that all of us would get plenty of practice
 * elsewhere in the project.
 *
 */

#define MODULE LIST

#include <list.h>

#include <global.h>

/* These fill the role of dummy nodes. */
static inline lanchor_t **next_field(lanchor_t *anchor, list_t *list){    
    return (anchor == NULL) ? &list->head : &anchor->next;
}

static inline lanchor_t **prev_field(lanchor_t *anchor, list_t *list){
    return (anchor == NULL) ? &list->tail : &anchor->prev;
}

void list_add_rear(lanchor_t *anchor, list_t *list){
    trace(list, p, anchor, p);

    assert(list_valid_quick(list));
    assert(anchor_unused(anchor, list));

    anchor->next = NULL;
    anchor->prev = list->tail;

    *next_field(list->tail, list) = anchor;
    list->tail = anchor;
        
    list->size++;

    assert(lanchor_valid(anchor, list));
}

void list_add_front(lanchor_t *anchor, list_t *list){
    trace(anchor, p, list, p);

    assert(list_valid_quick(list));
    assert(anchor_unused(anchor, list));

    anchor->prev = NULL;
    anchor->next = list->head;

    *prev_field(list->head, list) = anchor;
    list->head = anchor;
    
    list->size++;

    assert(lanchor_valid(anchor, list));
}

/** 
 * @brief Insert 'new_anchor' in the position immediately before that of the
 * anchor which is found by running list_find on the given list, with the
 * given key and comparator.
 *
 * If no such element is found, then insert it at the end of the list.
 */
void list_add_before(lanchor_t *new_anchor, lcomp_t comp, void *key,
                           list_t *list){
    trace(new_anchor, p, key, p, list, p);
    
    lanchor_t *first_match;

    assert(list_valid_quick(list));
    assert(anchor_unused(new_anchor, list));

    first_match = list_find(comp, key, list);

    if(first_match == NULL){
        list_add_rear(new_anchor, list);
        return;
    }

    new_anchor->prev = first_match->prev;
    new_anchor->next = first_match;

    *next_field(first_match->prev, list) = new_anchor;
    first_match->prev = new_anchor;

    list->size++;

    assert(lanchor_valid(new_anchor, list));
    
    return;
}

/** 
 * @brief Return the head of 'list' without popping it.
 */
lanchor_t *list_peek(list_t *list){
    trace(list, p);
    
    assert(list_valid_quick(list));

    return list->head;
}

/** 
 * @brief Remove and return the head of 'list'. 
 */
lanchor_t *list_pop(list_t *list){
    trace(list, p);
    assert(list_valid_quick(list));

    lanchor_t *outnode = list->head;
    if(!outnode)
        return NULL;

    *prev_field(outnode->next, list) = NULL;
    list->head = outnode->next;

    list->size--;

    /* /\* Size subtracted inside. *\/ */
    /* list_remove(outnode, list); */

    return outnode;
}

lanchor_t *list_pop_all(list_t *list){
    trace(list, p);
    assert(list_valid_quick(list));

    lanchor_t *out = list->head;

    list->head = list->tail = NULL;
    list->size = 0;
    
    return out;
}

/** 
 * @brief Remove 'anchor' from 'list'.
 *
 * It's illegal for 'anchor' to not be on 'list.
 */
void list_remove(lanchor_t *anchor, list_t *list){
    trace(anchor, p, list, p);

    assert(list_valid_quick(list));
    assert(lanchor_valid(anchor, list));
    assert2(list_contains(anchor, list));

    list->size--;

    *next_field(anchor->prev, list) = anchor->next;
    *prev_field(anchor->next, list) = anchor->prev;

    anchor->next = NULL;
    anchor->prev = NULL;
        
    return;
}

/** 
 * @brief Run the given comparator with the given key on every element of
 * 'list', and return the first element for which the comparator returns 1.
 */
lanchor_t *list_find(lcomp_t comparator, void *key, list_t *list){
    trace(comparator, p, key, p, list, p);
   
    lanchor_t *cur_anchor;

    assert(list_valid_quick(list));

    FOR_EACH_LANCHOR(cur_anchor, list){
        assert(lanchor_valid(cur_anchor, list));
        
        if(comparator(cur_anchor, key))
            return cur_anchor;
    }
    
    return NULL;
}

int list_contains(lanchor_t *anchor, list_t *list){
    trace(anchor, p, list, p);

    lanchor_t *cur_anchor;

    assert(list_valid_quick(list));

    if(anchor_unused(anchor, list))
        return 0;

    FOR_EACH_LANCHOR(cur_anchor, list){
        assert(lanchor_valid(cur_anchor, list));
        
        if(cur_anchor == anchor)
            return 1;
    }

    return 0;
}

/** 
 * @brief Return the n'th element of 'list', where list->head is defined as
 * the 0'th element.
 */
lanchor_t *list_nth(unsigned int n, list_t *list){
    trace(n, u, list, p);

    int i;
    lanchor_t *cur;
    
    assert(list_valid_quick(list));

    if(n + 1 > list->size)
        return NULL;
    
    if(n + 1 == list->size)
        return list->tail;

    i = 0;
    for(cur = list->head;
        cur != NULL && i < n;
        cur = cur->next){
        
        i++;
    }

    return cur;        
}

/** 
 * @brief Return the number of anchors on 'list'.
 *
 * This function is safe to call without a list lock.
 */
int list_size(list_t *list){
    return list->size;
}

/** 
 * @brief Return 1 iff 'list' is empty, or else 0.
 */
int list_empty(list_t *list){
    trace(list, p);

    assert(list_valid_quick(list));

    return list->head == NULL;
}

void list_invalidate(list_t *list){
    trace(list, p);

    assert(list_empty(list));
}

int anchor_unused(lanchor_t *anchor, list_t *list){
    assert(anchor != NULL);

    return
        (anchor->next == NULL) &&
        (anchor->prev == NULL) &&
        (list->head != anchor) &&
        (list->tail != anchor);
}

int lanchor_valid(lanchor_t *anchor, list_t *list){
    assert(anchor != NULL);
    assert(list != NULL);

    assert((anchor->prev != NULL) ^ (list->head == anchor));
    assert(anchor->prev == NULL || anchor->prev->next == anchor);
    assert(anchor->prev != anchor);

    assert((anchor->next != NULL) ^ (list->tail == anchor));
    assert(anchor->next == NULL || anchor->next->prev == anchor);
    assert(anchor->next != anchor);

    return 1;
}

int list_valid_quick(list_t *list){
    assert(list != NULL);
    
    assert((list->tail == NULL) ^ (list->head != NULL));
    assert((list->tail == NULL) ^ (list->size > 0));

    assert(list->size != UINT_MAX);

    return 1;
}

int list_valid_slow(list_t *list){
    lanchor_t *cur_anchor;
    int count;

    assert(list_valid_quick(list));

    count = 0;
    FOR_EACH_LANCHOR(cur_anchor, list){
        assert(count < list->size);
        count++;
        
        assert(lanchor_valid(cur_anchor, list));
    }

    assert(count == list->size);

    return 1;
}
