
typedef struct heritage heritage;
struct list;
struct flanchor;
typedef volatile struct flanchor flanchor;

struct flanchor{
    struct pxchg{
        union genptr {
            flanchor *p;
            struct { int64_t ngen:63; int locked:1; };
        };
        int gen;
    };
    struct nxchg{
        flanchor *n;
        union markptr {
            flanchor *pat;
            struct { int64_t patint:63; int read:1; };
        };
    };
    struct list *host;
};

typedef struct list{
    flanchor nil;
} list;

#define REMOVING ((void *) 0x1)
#define ADDING ((void *) 0x2)

typedef union markptr markptr;
typedef union genptr genptr;
typedef struct nxchg nxchg;
typedef struct pxchg pxchg;

list *lflist_add_rear(flanchor *a, heritage *h, list *l);
list *lflist_remove(flanchor *a, heritage *h, list *l);
list *lflist_remove_any(flanchor *a, heritage *h);
