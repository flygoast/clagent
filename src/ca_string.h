#ifndef __CA_STRING_H_INCLUDED__
#define __CA_STRING_H_INCLUDED__


#include <string.h>


typedef struct {
    size_t     len;
    u_char    *data;
} ca_str_t;


typedef struct {
    ca_str_t  key;
    ca_str_t  value;
} ca_keyval_t;


#define ca_string(str)     { sizeof(str) - 1, (u_char *) str }
#define ca_null_string     { 0, NULL }
#define ca_str_set(str, text)  \
    (str)->len = sizeof(text) - 1; (str)->data = (u_char *) text
#define ca_str_null(str)   (str)->len = 0; (str)->data = NULL

#define ca_tolower(c)      (u_char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define ca_toupper(c)      (u_char) ((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)

void ca_strlow(u_char *dst, u_char *src, size_t n);


#define ca_strncmp(s1, s2, n)  \
    strncmp((const char *)(s1), (const char *)(s2), n)

#define ca_strcmp(s1, s2)      strcmp((const char *)(s1), (const char *)(s2))
#define ca_strstr(s1, s2)      strstr((const char *)(s1), (const char *)(s2))
#define ca_strlen(s)           strlen((const char *)(s))

#define ca_strchr(s1, c)       strchr((char char *)s1, (int)c)


static inline u_char *
ca_strlchr(u_char *p, u_char *last, u_char c) 
{
    while (p < last) {

        if (*p == c) {
            return p;
        }

        p++;
    }

    return NULL;
}


#define ca_memzero(buf, n)         (void) memset(buf, 0, n)
#define ca_memset(buf, c, n)       (void) memset(buf, c, n)

#define ca_memcpy(dst, src, n)     (void) memcpy(dst, src, n)
#define ca_cpymem(dst, src, n)     (((u_char *)memcpy(dst, src, n)) + (n))

#define ca_memmove(dst, src, n)    (void) memmove(dst, src, n)
#define ca_movemem(dst, src, n)    (((u_char *) memmove(dst, src, n)) + (n))

#define ca_memcmp(s1, s2, n)   memcmp((const char *)s1, (const char *)s2, n)


u_char *ca_cpystrn(u_char *dst, u_char *src, size_t n);
u_char *ca_sprintf(u_char *buf, const char *fmt, ...);
u_char *ca_snprintf(u_char *buf, size_t max, const char *fmt, ...);
u_char *ca_slprintf(u_char *buf, u_char *last, const char *fmt, ...);
u_char *ca_vslprintf(u_char *buf, u_char *last, const char *fmt,
    va_list args);
#define ca_vsnprintf(buf, max, fmt, args)  \
    ca_vslprintf(buf, buf + (max), fmt, args)

int ca_strcasecmp(u_char *s1, u_char *s2);
int ca_strncasecmp(u_char *s1, u_char *s2, size_t n);
u_char *ca_strstrn(u_char *s1, char *s2, size_t n);
u_char *ca_strcasestrn(u_char *s1, char *s2, size_t n);


#endif /* __CA_STRING_H_INCLUDED__ */
