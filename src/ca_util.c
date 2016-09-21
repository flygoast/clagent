#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/syscall.h>
#include "clagent.h"


static struct timespec
dtotimespec(double sec)
{
    double min_representable = TYPE_MINIMUM(time_t);
    double max_representable =
           ((TYPE_MAXIMUM(time_t) * (double)BILLION + (BILLION - 1)) / BILLION);
    struct timespec r;

    if (!(min_representable < sec)) {
        r.tv_sec = TYPE_MINIMUM(time_t);
        r.tv_nsec = 0;
    } else if (!(sec < max_representable)) {
        r.tv_sec = TYPE_MAXIMUM(time_t);
        r.tv_nsec = BILLION - 1;
    } else {
        time_t s = (time_t)sec;
        double frac = BILLION * (sec - s);
        int64_t ns = frac;
        ns += ns < frac;
        s += ns / BILLION;
        ns %= BILLION;

        if (ns < 0) {
            s--;
            ns += BILLION;
        }
        r.tv_sec = s;
        r.tv_nsec = ns;
    }

    return r;
}


int
ca_nanosleep(double seconds)
{
    struct timespec ts_sleep = dtotimespec(seconds);

    for ( ;; ) {
        errno = 0;
        if (nanosleep(&ts_sleep, NULL) == 0) {
            break;
        }

        if (errno != EINTR && errno != 0) {
            return CA_ERROR;
        }
    }

    return CA_OK;
}


void
ca_get_time(int64_t *seconds, int64_t *milliseconds)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    *seconds = tv.tv_sec;
    *milliseconds = tv.tv_usec / 1000;
}


void
ca_add_milliseconds_to_now(int64_t milliseconds,
    int64_t *sec, int64_t *ms)
{
    int64_t cur_sec, cur_ms, when_sec, when_ms;

    ca_get_time(&cur_sec, &cur_ms);

    when_sec = cur_sec + milliseconds / 1000;
    when_ms = cur_ms + milliseconds % 1000;

    /* cur_ms < 1000, when_ms < 2000, so just one time is enough. */
    if (when_ms >= 1000) {
        when_sec++;
        when_ms -= 1000;
    }

    *sec = when_sec;
    *ms = when_ms;
}


int
ca_atoi(u_char *line, size_t n)
{
    int value;

    if (n == 0) {
        return CA_ERROR;
    }

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return CA_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    if (value < 0) {
        return CA_ERROR;
    }

    return value;
}


ssize_t
ca_atosz(u_char *line, size_t n)
{
    ssize_t  value;

    if (n == 0) {
        return CA_ERROR;
    }

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return CA_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    if (value < 0) {
        return CA_ERROR;

    } else {
        return value;
    }
}


int
ca_hextoi(u_char *line, size_t n)
{
    u_char  c, ch;
    int     value;

    if (n == 0) {
        return CA_ERROR;
    }

    for (value = 0; n--; line++) {
        ch = *line;

        if (ch >= '0' && ch <= '9') {
            value = value * 16 + (ch - '0');
            continue;
        }

        c = (u_char) (ch | 0x20);

        if (c >= 'a' && c <= 'f') {
            value = value * 16 + (c - 'a' + 10);
            continue;
        }

        return CA_ERROR;
    }

    if (value < 0) {
        return CA_ERROR;

    } else {
        return value;
    }
}


u_char *
ca_hex_dump(u_char *dst, u_char *src, size_t len)
{
    static u_char  hex[] = "0123456789abcdef";

    while (len--) {
        *dst++ = hex[*src >> 4];
        *dst++ = hex[*src++ & 0xf];
    }

    return dst;
}


uint64_t
ca_time_ms(void)
{
    struct timeval   tv;
    uint64_t         mst;

    gettimeofday(&tv, NULL);
    mst = ((uint64_t)tv.tv_sec) * 1000;
    mst += tv.tv_usec / 1000;
    return mst;
}


uint64_t
ca_time_us(void)
{
    struct timeval  tv;
    uint64_t        ust;

    gettimeofday(&tv, NULL);
    ust = ((uint64_t)tv.tv_sec) * 1000000;
    ust += tv.tv_usec;
    return ust;
}


ssize_t
ca_parse_size(ca_str_t *line)
{
    u_char     unit;
    size_t     len;
    ssize_t    size;
    ca_int_t   scale;

    len = line->len;
    unit = line->data[len - 1];

    switch (unit) {
    case 'K':
    case 'k':
        len--;
        scale = 1024;
        break;

    case 'M':
    case 'm':
        len--;
        scale = 1024 * 1024;
        break;

    case 'G':
    case 'g':
        len--;
        scale = 1024 * 1024 * 1024;
        break;

    default:
        scale = 1;
    }

    size = ca_atosz(line->data, len);
    if (size == CA_ERROR) {
        return CA_ERROR;
    }

    size *= scale;

    return size;
}


ca_int_t
ca_parse_time(ca_str_t *line, ca_uint_t is_sec)
{
    u_char      *p, *last;
    ca_int_t     value, total, scale;
    ca_uint_t    max, valid;
    enum {
        st_start = 0,
        st_year,
        st_month,
        st_week,
        st_day,
        st_hour,
        st_min,
        st_sec,
        st_msec,
        st_last
    } step;

    valid = 0;
    value = 0;
    total = 0;
    step = is_sec ? st_start : st_month;
    scale = is_sec ? 1 : 1000;

    p = line->data;
    last = p + line->len;

    while (p < last) {

        if (*p >= '0' && *p <= '9') {
            value = value * 10 + (*p++ - '0');
            valid = 1;
            continue;
        }

        switch (*p++) {

        case 'y':
            if (step > st_start) {
                return CA_ERROR;
            }
            step = st_year;
            max = CA_MAX_INT32_VALUE / (60 * 60 * 24 * 365);
            scale = 60 * 60 * 24 * 365;
            break;

        case 'M':
            if (step >= st_month) {
                return CA_ERROR;
            }
            step = st_month;
            max = CA_MAX_INT32_VALUE / (60 * 60 * 24 * 30);
            scale = 60 * 60 * 24 * 30;
            break;

        case 'w':
            if (step >= st_week) {
                return CA_ERROR;
            }
            step = st_week;
            max = CA_MAX_INT32_VALUE / (60 * 60 * 24 * 7);
            scale = 60 * 60 * 24 * 7;
            break;

        case 'd':
            if (step >= st_day) {
                return CA_ERROR;
            }
            step = st_day;
            max = CA_MAX_INT32_VALUE / (60 * 60 * 24);
            scale = 60 * 60 * 24;
            break;

        case 'h':
            if (step >= st_hour) {
                return CA_ERROR;
            }
            step = st_hour;
            max = CA_MAX_INT32_VALUE / (60 * 60);
            scale = 60 * 60;
            break;

        case 'm':
            if (*p == 's') {
                if (is_sec || step >= st_msec) {
                    return CA_ERROR;
                }
                p++;
                step = st_msec;
                max = CA_MAX_INT32_VALUE;
                scale = 1;
                break;
            }

            if (step >= st_min) {
                return CA_ERROR;
            }
            step = st_min;
            max = CA_MAX_INT32_VALUE / 60;
            scale = 60;
            break;

        case 's':
            if (step >= st_sec) {
                return CA_ERROR;
            }
            step = st_sec;
            max = CA_MAX_INT32_VALUE;
            scale = 1;
            break;

        case ' ':
            if (step >= st_sec) {
                return CA_ERROR;
            }
            step = st_last;
            max = CA_MAX_INT32_VALUE;
            scale = 1;
            break;

        default:
            return CA_ERROR;
        }

        if (step != st_msec && !is_sec) {
            scale *= 1000;
            max /= 1000;
        }

        if ((ca_uint_t) value > max) {
            return CA_ERROR;
        }

        total += value * scale;

        if ((ca_uint_t) total > CA_MAX_INT32_VALUE) {
            return CA_ERROR;
        }

        value = 0;
        scale = is_sec ? 1 : 1000;

        while (p < last && *p == ' ') {
            p++;
        }
    }

    if (valid) {
        return total + value * scale;
    }

    return CA_ERROR;
}


pid_t
gettid()
{
    return syscall(SYS_gettid);
}


int
ca_strsplit(char *string, char **fields, size_t size)
{
	size_t   i = 0;
	char    *ptr = string;
	char    *saveptr = NULL;

	while ((fields[i] = strtok_r (ptr, " \t\r\n", &saveptr)) != NULL) {
		ptr = NULL;
		i++;

		if (i >= size){
            break;
        }
	}

	return ((int) i);
}


int
ca_in_array(const char *str, const char *str_list[], int array_size)
{
    int i;

    for (i = 0; i < array_size; i++) {
        if (ca_strcmp(str, str_list[i]) == 0) {
            return 1;
        }
    }

    return 0;
}


char *
ca_trim(char *str)
{
    char  *start, *end;

    if (str == NULL) {
        return NULL;
    }

    start = str;
    end = str + strlen(str) - 1;

    while (*start) {
        if (*start != ' '
            && *start != '\t'
            && *start != '\r'
            && *start != '\n')
        {
            break;
        }
        start++;
    }

    while (end >= str) {
        if (*end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') {
            break;
        }

        *end = '\0';
        end--;
    }

    return start;
}
