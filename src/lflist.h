#include <nalloc.h>

typedef volatile struct flanchor{
    struct pxchg{
        union {
            struct flanchor *p;
            struct genptr { int ngen:31; int locked:1 };
        }
        int gen;
    };
    struct nxchg{
        struct flanchor_t *n;
        union{
            struct flanchor *pat;
            struct markptr { int patint:31; int read:1 };
        }
    };
    list *host;
}flanchor;

#define REMOVING ((void *) 0x1)
#define ADDING ((void *) 0x2)

typedef struct nxchg nxchg;
typedef struct pxchg pxchg;
typedef struct genptr genptr;

int lflist_add_rear(flanchor_t *a, list_t *l);
int lflist_remove(flanchor_t *a, list_t l, heritage_t *h);

#define FOR_EACH_SLOPPY_FLOOKUP(cur, list, type, anc_field)             \
    for(flanchor_t *i = (list)->peek();                                 \
        i != (list)->nil && cur = cof(i, type, anc_field);              \
        (i = i->next,                                                   \
         i->host && i->host != (list) ? i = (list)->peek()))            \
