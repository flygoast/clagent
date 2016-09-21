#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "clagent.h"

static uint32_t         ca_buf_nfree;          /* # free ca_buf */
static ca_buf_hdr_t     ca_buf_free_queue;     /* free ca_buf queue */
static uint32_t         ca_buf_max_nfree;      /* max # free ca_buf_t */
static size_t           ca_buf_chunk_size;
static size_t           ca_buf_offset;


static ca_buf_t *
ca_buf_get_internal(void)
{
    ca_buf_t  *ca_buf;
    u_char    *buf;

    if (!STAILQ_EMPTY(&ca_buf_free_queue)) {
        ca_buf = STAILQ_FIRST(&ca_buf_free_queue);
        ca_buf_nfree--;
        STAILQ_REMOVE_HEAD(&ca_buf_free_queue, next);
        ASSERT(ca_buf->magic == ca_BUF_MAGIC);
        goto done;
    }

    buf = ca_alloc(ca_buf_chunk_size);
    if (buf == NULL) {
        return NULL;
    }

    /*
     * ca_buf header is at the tail end of the ca_buf. This enables us to 
     * catch buffer overrun early by asserting on the magic value during get
     * or put operations
     *
     *   <------------- ca_buf_chunk_size ------------->
     *   +---------------------------------------------+
     *   |     ca_buf data         |   ca_buf header   |
     *   |   (ca_buf_offset)       |   (ca_buf_t)      |
     *   +---------------------------------------------+
     *   ^           ^        ^    ^^
     *   |           |        |    ||
     *   \           |        |    |\
     *    start      \        |    | end (one byte past valid bound)
     *                pos     |    \
     *                        \     ca_buf
     *                        last (one byte past valid byte)
     *
     */

    ca_buf = (ca_buf_t *)(buf + ca_buf_offset);
    SET_MAGIC(ca_buf, CA_BUF_MAGIC);

done:
    STAILQ_NEXT(ca_buf, next) = NULL;
    return ca_buf;
}


ca_buf_t *
ca_buf_get(void)
{
    ca_buf_t  *ca_buf;
    u_char    *buf;

    ca_buf = ca_buf_get_internal();
    if (ca_buf == NULL) {
        return NULL;
    }

    buf = (u_char *)ca_buf - ca_buf_offset;
    ca_buf->start = buf;
    ca_buf->end = (u_char *)ca_buf;

    ASSERT(ca_buf->end - ca_buf->start == ca_buf_offset);
    ASSERT(ca_buf->start < ca_buf->end);

    ca_buf->pos = ca_buf->start;
    ca_buf->last = ca_buf->start;

    return ca_buf;
}


static void
ca_buf_free(ca_buf_t *ca_buf)
{
    u_char  *buf;

    ASSERT(STAILQ_NEXT(ca_buf, next) == NULL);
    ASSERT(ca_buf->magic == ca_BUF_MAGIC);

    buf = (u_char *)ca_buf - ca_buf_offset;
    ca_free(buf);
}


void
ca_buf_put(ca_buf_t *ca_buf)
{
    ASSERT(STAILQ_NEXT(ca_buf, next) == NULL);
    ASSERT(ca_buf->magic == ca_BUF_MAGIC);

    if (ca_buf_max_nfree != 0 && ca_buf_nfree + 1 > ca_buf_max_nfree) {
        ca_buf_free(ca_buf);

    } else {
        ca_buf_nfree++;
        STAILQ_INSERT_HEAD(&ca_buf_free_queue, ca_buf, next);
    }
}


/*
 * Insert ca_buf at the tail of the ca_buf_hdr Q.
 */
void
ca_buf_insert(ca_buf_hdr_t *ca_hdr, ca_buf_t *ca_buf)
{
    STAILQ_INSERT_TAIL(ca_hdr, ca_buf, next);
}


/*
 * Remove ca_buf from ca_buf_hdr Q.
 */
void
ca_buf_remove(ca_buf_hdr_t *ca_hdr, ca_buf_t *ca_buf)
{
    STAILQ_REMOVE(ca_hdr, ca_buf, ca_buf_s, next);
    STAILQ_NEXT(ca_buf, next) = NULL;
}


/*
 * Copy n bytes from memory area pos to ca_buf.
 *
 * The memory areas should not overlap and the ca_buf should have enough
 * space for n bytes.
 */
void
ca_buf_copy(ca_buf_t *ca_buf, u_char *pos, size_t n)
{
    if (n == 0) {
        return;
    }

    /* ca_buf has space for n bytes. */
    assert(!ca_buf_full(ca_buf) && n <= ca_buf_size(ca_buf));

    /* no overlapping copy */
    assert(pos < ca_buf->start || pos >= ca_buf->end);

    ca_memcpy(ca_buf->last, pos, n);
    ca_buf->last += n;
}


/*
 * Split ca_buf h into h and t by copying data from h to t. Before the
 * copy, we invoke a precopy handler cb that will copy a predefined 
 * string to the head of t.
 *
 * Return new ca_buf t, if the split was successful.
 */
ca_buf_t *
ca_buf_split(ca_buf_hdr_t *h, u_char *pos, ca_buf_copy_pt cb, void *cbarg)
{
    ca_buf_t   *ca_buf, *nbuf;
    size_t       size;

    ASSERT(!STAILQ_EMPTY(h));

    ca_buf = STAILQ_LAST(h, ca_buf_s, next);

    ASSERT(pos >= ca_buf->pos && pos <= ca_buf->last);

    nbuf = ca_buf_get();
    if (nbuf == NULL) {
        return NULL;
    }

    if (cb != NULL) {
        /* precopy nbuf */
        cb(nbuf, cbarg);
    }

    /* copy data from ca_buf to nbuf */
    size = (size_t)(ca_buf->last - pos);
    ca_buf_copy(nbuf, pos, size);

    /* adjust ca_buf */
    ca_buf->last = pos;

    return nbuf;
}


void
ca_buf_init(uint32_t max_nfree)
{
    ca_buf_max_nfree = max_nfree;
    ca_buf_nfree = 0;
    STAILQ_INIT(&ca_buf_free_queue);

    ca_buf_chunk_size = CA_BUF_SIZE;
    ca_buf_offset = ca_buf_chunk_size - CA_BUF_HSIZE;
}


void
ca_buf_deinit(void)
{
    while (!STAILQ_EMPTY(&ca_buf_free_queue)) {
        ca_buf_t *ca_buf = STAILQ_FIRST(&ca_buf_free_queue);
        ca_buf_remove(&ca_buf_free_queue, ca_buf);
        ca_buf_free(ca_buf);
        ca_buf_nfree--;
    }

    ASSERT(ca_buf_nfree == 0);
}


void
ca_buf_queue_rewind(ca_buf_hdr_t *ca_hdr)
{
     ca_buf_t *buf, *nbuf;   /* current and next buf */

    for (buf = STAILQ_FIRST(ca_hdr); buf != NULL; buf = nbuf) {
        nbuf = STAILQ_NEXT(buf, next);
        buf->pos = buf->start;
        buf->last = buf->start;
    }
}
