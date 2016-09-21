#ifndef __CA_ARRAY_H_INCLUDED__
#define __CA_ARRAY_H_INCLUDED__


typedef int (ca_array_cmp_pt)(const void *, const void *);
typedef int (*ca_array_each_pt)(void *, void *);


typedef struct {
    uint32_t    nelem;  /* # element */
    void       *elem;   /* element */
    size_t      size;   /* element size */
    uint32_t    nalloc; /* # allocated element */
} ca_array_t;


#define ca_null_array      { 0, NULL, 0, 0 }


static inline void
ca_array_null(ca_array_t *a)
{
    a->nelem = 0;
    a->elem = NULL;
    a->size = 0;
    a->nalloc = 0;
}


static inline void
ca_array_set(ca_array_t *a, void *elem, size_t size, uint32_t nalloc)
{
    a->nelem = 0;
    a->elem = elem;
    a->size = size;
    a->nalloc = nalloc;
}


ca_array_t *ca_array_create(uint32_t n, size_t size);
void ca_array_destroy(ca_array_t *a);
int ca_array_init(ca_array_t *a, uint32_t n, size_t size);
void ca_array_deinit(ca_array_t *a);
void *ca_array_push(ca_array_t *a);
void *ca_array_pop(ca_array_t *a);
void *ca_array_get(ca_array_t *a, uint32_t idx);
void *ca_array_top(ca_array_t *a);
void ca_array_swap(ca_array_t *a, ca_array_t *b);
void ca_array_sort(ca_array_t *a, ca_array_cmp_pt cmp);
int ca_array_each(ca_array_t *a, ca_array_each_pt func, void *data);


#endif /* __CA_ARRAY_H_INCLUDED__ */
