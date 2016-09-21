#ifndef __CA_UTIL_H_INCLUDED__
#define __CA_UTIL_H_INCLUDED__


#include <limits.h>


/* True if negative values of the signed integer type T uses two's
 * complement, one's complement, or signed magnitude representation,
 * respectively. */
#define TYPE_TWOS_COMPLEMENT(t)     ((t) ~(t)0 == (t)-1)
#define TYPE_ONES_COMPLEMENT(t)     ((t) ~(t)0 == 0)
#define TYPE_SIGNED_MAGNITUDE(t)    ((t) ~(t)0 < (t)-1)

#define TYPE_SIGNED(t)  (!((t)0 < (t)-1))
#define TYPE_MAXIMUM(t)             \
    ((t)(! TYPE_SIGNED(t)           \
        ? (t)-1                     \
        : ((((t)1 <<(sizeof(t) * CHAR_BIT - 2)) - 1) * 2 + 1)))
#define TYPE_MINIMUM(t)             \
    ((t)(! TYPE_SIGNED(t)           \
        ? (t)0                      \
        : TYPE_SIGNED_MAGNITUDE(t)  \
        ? ~(t)0                     \
        : ~ TYPE_MAXIMUM(t)))
#define BILLION         (1000 * 1000 * 1000)


int ca_nanosleep(double seconds);
void ca_add_milliseconds_to_now(int64_t ms_delta, int64_t *sec, int64_t *ms);
void ca_get_time(int64_t *seconds, int64_t *milliseconds);
int ca_atoi(u_char *line, size_t n);
ssize_t ca_atosz(u_char *line, size_t n);

int ca_hextoi(u_char *line, size_t n);
u_char *ca_hex_dump(u_char *dst, u_char *src, size_t len);
uint64_t ca_time_ms(void);
uint64_t ca_time_us(void);
ca_int_t ca_parse_time(ca_str_t *line, ca_uint_t is_sec);
ssize_t ca_parse_size(ca_str_t *line);
pid_t gettid(void);
int ca_strsplit(char *string, char **fields, size_t size);
int ca_in_array(const char *str, const char *str_list[], int array_size);
char *ca_trim(char *str);


#endif /* __CA_UTIL_H_INCLUDED__ */
