#include "clagent.h"


ca_array_t *
ca_array_create(uint32_t n, size_t size)
{
    ca_array_t  *a;

    a = ca_alloc(sizeof(ca_array_t));
    if (a == NULL) {
        return NULL;
    }

    if (ca_array_init(a, n, size) != CA_OK) {
        ca_free(a);
        return NULL;
    }

    return a;
}


void
ca_array_destroy(ca_array_t *a) 
{
    ca_array_deinit(a);
    ca_free(a);
}


int
ca_array_init(ca_array_t *a, uint32_t n, size_t size)
{
    ASSERT(n != 0 && size != 0);

    a->elem = ca_alloc(n * size);
    if (a->elem == NULL) {
        ca_free(a);
        return CA_ERROR;
    }

    a->nelem = 0;
    a->size = size;
    a->nalloc = n;

    return CA_OK;
}


void
ca_array_deinit(ca_array_t *a)
{
    if (a->elem != NULL) {
        ca_free(a->elem);
    }
}


uint32_t
ca_array_idx(ca_array_t *a, void *elem)
{
    u_char   *p, *q;
    uint32_t   off, idx;

    ASSERT(elem >= a->elem);

    p = a->elem;
    q = elem;
    off = (uint32_t)(q - p);

    ASSERT(off % (uint32_t)a->size == 0);

    idx = off / (uint32_t)a->size;

    return idx;
}


void *
ca_array_push(ca_array_t *a)
{
    void   *elem, *new;
    size_t  size;

    if (a->nelem == a->nalloc) {

        /* the array is full; allocate new array */
        size = a->size * a->nalloc;
        new = ca_realloc(a->elem, 2 * size);

        if (new == NULL) {
            return NULL;
        }

        a->elem = new;
        a->nalloc *= 2;
    }

    elem = (u_char *)a->elem + a->size * a->nelem;
    a->nelem++;

    return elem;
}


void *
ca_array_pop(ca_array_t *a)
{
    void *elem;
    ASSERT(a->nelem != 0);

    a->nelem--;
    elem = (u_char *)a->elem + a->size * a->nelem;

    return elem;
}


void *
ca_array_get(ca_array_t *a, uint32_t idx)
{
    void  *elem;

    ASSERT(a->nelem != 0);
    ASSERT(idx < a->nelem);

    elem = (u_char *)a->elem + (a->size * idx);

    return elem;
}


void *
ca_array_top(ca_array_t *a)
{
    ASSERT(a->nelem != 0);

    return ca_array_get(a, a->nelem - 1);
}


void
ca_array_swap(ca_array_t *a, ca_array_t *b)
{
    ca_array_t  tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}


/*
 * Sort nelem elements of the array in ascending order based on the
 * compare comparator.
 */
void
ca_array_sort(ca_array_t *a, ca_array_cmp_pt cmp)
{
    ASSERT(a->nelem != 0);
    qsort(a->elem, a->nelem, a->size, cmp);
}


/*
 * Call the func once for each element in the array as long as func
 * returns success. On failure short-circuits and returns the error.
 */
int
ca_array_each(ca_array_t *a, ca_array_each_pt func, void *data)
{
    uint32_t  i, nelem;
    int       rc;
    void     *elem;

    ASSERT(a->nelem != 0);
    ASSERT(func != NULL);

    for (i = 0, nelem = a->nelem; i < nelem; i++) {
        elem = ca_array_get(a, i);

        rc = func(elem, data);
        if (rc != CA_OK) {
            return rc;
        }
    }

    return CA_OK;
}
