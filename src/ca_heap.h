#ifndef __CA_HEAP_H_INCLUDED__
#define __CA_HEAP_H_INCLUDED__


typedef int  (*cmp_fn)(void *, void *);
typedef void (*record_fn)(void *, int);
typedef void (*free_fn)(void *);


typedef struct heap_st {
    int          cap;
    int          len;
    void       **data;
    cmp_fn       less;
    record_fn    record;
    free_fn      ent_free;
} ca_heap_t;


#define ca_heap_set_less(h, l)     (h)->less = l
#define ca_heap_set_record(h, r)   (h)->record = r
#define ca_heap_set_free(h, f)     (h)->ent_free = f


ca_heap_t *ca_heap_create(void);
int ca_heap_init(ca_heap_t *h);
int ca_heap_insert(ca_heap_t *h, void *data);
void *ca_heap_remove(ca_heap_t *h, int k);
void ca_heap_destroy(ca_heap_t *h);
void ca_heap_free(ca_heap_t *h);


#endif /* __CA_HEAP_H_INCLUDED__ */
