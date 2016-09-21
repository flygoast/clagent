#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ca_core.h"
#include "ca_heap.h"


#define HEAP_INIT_SIZE      16


static
void ca_heap_set(ca_heap_t *h, int k, void *data)
{
    h->data[k] = data;
    if (h->record) {
        h->record(data, k);
    }
}


static void
ca_heap_swap(ca_heap_t *h, int a, int b)
{
    void *tmp;
    tmp = h->data[a];
    ca_heap_set(h, a, h->data[b]);
    ca_heap_set(h, b, tmp);
}


static int
ca_heap_less(ca_heap_t *h, int a, int b)
{
    if (h->less) {
        return h->less(h->data[a], h->data[b]);
    } else {
        return (h->data[a] < h->data[b]) ? 1 : 0;
    }
}


static void
ca_heap_siftdown(ca_heap_t *h, int k)
{
    for ( ; ; ) {
        int p = (k - 1) / 2; /* parent */

        if (k == 0 || ca_heap_less(h, p, k)) {
            return;
        }
        ca_heap_swap(h, k, p);
        k = p;
    }
}


static void
ca_heap_siftup(ca_heap_t *h, int k)
{
    for ( ; ; ) {
        int l, r, s;
        l = k * 2 + 1; /* left child */
        r = k * 2 + 2; /* right child */

        /* find the smallest of the three */
        s = k;
        if (l < h->len && ca_heap_less(h, l, s)) s = l;
        if (r < h->len && ca_heap_less(h, r, s)) s = r;

        if (s == k) {
            return; /* statisfies the heap property */
        }

        ca_heap_swap(h, k, s);
        k = s;
    }
}


ca_heap_t *
ca_heap_create(void)
{
    ca_heap_t *h = (ca_heap_t*) ca_alloc(sizeof(*h));
    if (!h) return NULL;
    if (ca_heap_init(h) != 0) {
        ca_free(h);
        return NULL;
    }
    return h;
}


int
ca_heap_init(ca_heap_t *h)
{
    h->cap = HEAP_INIT_SIZE;
    h->len = 0;
    h->data = (void **)ca_calloc(h->cap, sizeof(void *));
    if (!h->data) return -1;
    h->less = NULL;
    h->record = NULL;
    return 0;
}


/* heap_insert insert `data' into heap `h' according
 * to h->less.
 * 0 returned on success, otherwise -1. */
int
ca_heap_insert(ca_heap_t *h, void *data)
{
    int k;

    if (h->len >= h->cap) {
        void **ndata;
        int ncap = (h->len + 1) * 2; /* callocate twice what we need */

        ndata = ca_realloc(h->data, sizeof(void*) * ncap);
        if (!ndata) {
            return -1;
        }
        h->data = ndata;
        h->cap = ncap;
    }
    k = h->len;
    ++h->len;
    ca_heap_set(h, k, data);
    ca_heap_siftdown(h, k);
    return 0;
}


void *
ca_heap_remove(ca_heap_t *h, int k)
{
    void *data;

    if (k >= h->len) {
        return NULL;
    }

    data = h->data[k];
    --h->len;
    ca_heap_set(h, k, h->data[h->len]);
    ca_heap_siftdown(h, k);
    ca_heap_siftup(h, k);
    if (h->record) {
        h->record(data, -1);
    }
    return data;
}


void
ca_heap_destroy(ca_heap_t *h)
{
    void *data;
    while (h->len) {
        data = ca_heap_remove(h, 0);
        if (h->ent_free && data) {
            h->ent_free(data);
        }
    }

    if (h->data) {
        ca_free(h->data);
        h->data = NULL;
    }
    h->cap = 0;
    h->len = 0;
    h->less = NULL;
    h->record = NULL;
    h->ent_free = NULL;
}


void
ca_heap_free(ca_heap_t *h)
{
    ca_heap_destroy(h);
    ca_free(h);
}
