#ifndef __CA_BUF_H_INCLUDED__
#define __CA_BUF_H_INCLUDED__


#define CA_BUF_MAGIC       0x43425546      /* "CBUF" */
#define CA_BUF_MIN_SIZE    512
#define CA_BUF_MAX_SIZE    65536
#define CA_BUF_SIZE        8192
#define CA_BUF_HSIZE       sizeof(ca_buf_t)
#define CA_BUF_MAX_NFREE   128

#define ca_buf_empty(buf)  (((buf)->pos == (buf)->last) ? 1 : 0)
#define ca_buf_full(buf)   (((buf)->last == (buf)->end) ? 1 : 0)


typedef struct ca_buf_s ca_buf_t;
typedef struct ca_buf_hdr_s ca_buf_hdr_t;
typedef void (*ca_buf_copy_pt)(ca_buf_t *, void *);


struct ca_buf_s {
#ifdef WITH_DEBUG
    uint32_t                  magic;
#endif
    STAILQ_ENTRY(ca_buf_s)    next;        /* next ca_buf */
    u_char                   *pos;         /* read marker */
    u_char                   *last;        /* write marker */
    u_char                   *start;       /* start of buffer */
    u_char                   *end;         /* end of buffer */
};


STAILQ_HEAD(ca_buf_hdr_s, ca_buf_s);


/*
 * Rewind the ca_buf by discarding any of the read or unread data that it
 * might hold.
 */
#define ca_buf_rewind(ca_buf)     \
    ca_buf->pos = ca_buf->start; ca_buf->last = ca_buf->start

/*
 * Return the length of data in ca_buf. ca_buf cannot contail more than
 * 2^32 bytes (4G) .
 */
#define ca_buf_length(ca_buf)   (uint32_t) ((ca_buf)->last - (ca_buf)->pos)

/*
 * Return the remaining space size for any new data in ca_buf. ca_buf cannot
 * contain more than 2^32 bytes (4G).
 */
#define ca_buf_size(ca_buf)     (uint32_t) ((ca_buf)->end - (ca_buf)->last)

/*
 * Return the maximum available space size for data in any ca_buf. ca_buf
 * cannot contain more than 2^32 bytes (4G).
 */
#define ca_buf_data_size         ca_buf_offset


void ca_buf_init(uint32_t max_nfree);
void ca_buf_deinit(void);
void ca_buf_queue_rewind(ca_buf_hdr_t *ca_hdr);
void ca_buf_put(ca_buf_t *ca_buf);
void ca_buf_insert(ca_buf_hdr_t *ca_hdr, ca_buf_t *ca_buf);
void ca_buf_remove(ca_buf_hdr_t *ca_hdr, ca_buf_t *ca_buf);
void ca_buf_copy(ca_buf_t *ca_buf, u_char *pos, size_t n);
ca_buf_t *ca_buf_get(void);
ca_buf_t *ca_buf_split(ca_buf_hdr_t *h, u_char *pos, ca_buf_copy_pt cb,
    void *cbarg);


#endif /* __CA_BUF_H_INCLUDED__ */
