#include "../clagent.h"
#include <time.h>


typedef struct ca_mem_info_s {
    int64_t  mem_total;
    int64_t  mem_used;
    int64_t  mem_free;
    int64_t  swap_total;
    int64_t  swap_used;
    int64_t  swap_free;
    int64_t  mem_cache;
    int64_t  mem_buffer;
    double   mem_urate;
    double   swap_urate;
    time_t   updated;
} ca_mem_info_t;


static ca_mem_info_t  ca_s_mem_info = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1.0, -1.0, 0
};


static void
ca_get_mem_info(void)
{
    char      buffer[1024];
    char     *fields[8];
    FILE     *fh;
    int       numfields;
    int64_t  *val;

    fh = fopen("/proc/meminfo", "r");
    if (fh == NULL) {
        return;
    }

    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        val = NULL;

		if (strncasecmp(buffer, "MemTotal:", 9) == 0) {
            val = &ca_s_mem_info.mem_total;

        } else if (strncasecmp(buffer, "MemFree:", 8) == 0) {
            val = &ca_s_mem_info.mem_free;

        } else if (strncasecmp(buffer, "Buffers:", 8) == 0) {
            val = &ca_s_mem_info.mem_buffer;

        } else if (strncasecmp(buffer, "Cached:", 7) == 0) {
            val = &ca_s_mem_info.mem_cache;

        } else if (strncasecmp(buffer, "SwapTotal:", 10) == 0) {
            val = &ca_s_mem_info.swap_total;

        } else if (strncasecmp(buffer, "SwapFree:", 9) == 0) {
            val = &ca_s_mem_info.swap_free;

        } else {
            continue;
        }

		numfields = ca_strsplit(buffer, fields, 8);
		if (numfields < 2) {
            continue;
        }

		*val = atoll(fields[1]);
    }

    fclose(fh);

    ca_s_mem_info.mem_free = ca_s_mem_info.mem_free
                             + ca_s_mem_info.mem_buffer
                             + ca_s_mem_info.mem_cache;

    ca_s_mem_info.mem_used = ca_s_mem_info.mem_total - ca_s_mem_info.mem_free;

    ca_s_mem_info.swap_used = ca_s_mem_info.swap_total
                              - ca_s_mem_info.swap_free;

    if (ca_s_mem_info.mem_total > 0) {
        ca_s_mem_info.mem_urate = (ca_s_mem_info.mem_used * 100)
                                   / (double) ca_s_mem_info.mem_total;
    }

    if (ca_s_mem_info.swap_total > 0) {
        ca_s_mem_info.swap_urate = (ca_s_mem_info.swap_used * 100)
                                   / (double) ca_s_mem_info.swap_total;
    }

    ca_s_mem_info.updated = time(NULL);
}


u_char *
ca_get_mem_total(time_t now, time_t freq)
{
    static u_char  mem_total[20];

    if (ca_s_mem_info.updated + freq <= now) {
        ca_get_mem_info();
    }

    if (ca_s_mem_info.mem_total >= 0) {
        ca_snprintf(mem_total, sizeof(mem_total), "%L%Z",
                    ca_s_mem_info.mem_total);

    } else {
        mem_total[0] = '\0';
    }

    ca_s_mem_info.mem_total = -1;

    return mem_total;
}


u_char *
ca_get_mem_used(time_t now, time_t freq)
{
    static u_char  mem_used[20];
    
    if (ca_s_mem_info.updated + freq <= now) {
        ca_get_mem_info();
    }

    if (ca_s_mem_info.mem_used >= 0) {
        ca_snprintf(mem_used, sizeof(mem_used), "%L%Z",
                    ca_s_mem_info.mem_used);

    } else {
        mem_used[0] = '\0';
    }

    ca_s_mem_info.mem_used = -1;

    return mem_used;
}


u_char *
ca_get_mem_free(time_t now, time_t freq)
{
    static u_char  mem_free[20];

    if (ca_s_mem_info.updated + freq <= now) {
        ca_get_mem_info();
    }

    if (ca_s_mem_info.mem_free >= 0) {
        ca_snprintf(mem_free, sizeof(mem_free), "%L%Z",
                    ca_s_mem_info.mem_free);

    } else {
        mem_free[0] = '\0';
    }

    ca_s_mem_info.mem_free = -1;

    return mem_free;
}


u_char *
ca_get_swap_total(time_t now, time_t freq)
{
    static u_char  swap_total[20];

    if (ca_s_mem_info.updated + freq <= now) {
        ca_get_mem_info();
    }

    if (ca_s_mem_info.swap_total >= 0) {
        ca_snprintf(swap_total, sizeof(swap_total), "%L%Z",
                    ca_s_mem_info.swap_total);

    } else {
        swap_total[0] = '\0';
    }

    ca_s_mem_info.swap_total = -1;

    return swap_total;
}


u_char *
ca_get_swap_used(time_t now, time_t freq)
{
    static u_char  swap_used[20];

    if (ca_s_mem_info.updated + freq <= now) {
        ca_get_mem_info();
    }

    if (ca_s_mem_info.swap_used >= 0) {
        ca_snprintf(swap_used, sizeof(swap_used), "%L%Z",
                    ca_s_mem_info.swap_used);

    } else {
        swap_used[0] = '\0';
    }

    ca_s_mem_info.swap_used = -1;

    return swap_used;
}


u_char *
ca_get_swap_free(time_t now, time_t freq)
{
    static u_char  swap_free[20];

    if (ca_s_mem_info.updated + freq <= now) {
        ca_get_mem_info();
    }

    if (ca_s_mem_info.swap_free >= 0) {
        ca_snprintf(swap_free, sizeof(swap_free), "%L%Z",
                    ca_s_mem_info.swap_free);

    } else {
        swap_free[0] = '\0';
    }

    ca_s_mem_info.swap_free = -1;

    return swap_free;
}


u_char *
ca_get_mem_cache(time_t now, time_t freq)
{
    static u_char  mem_cache[20];

    if (ca_s_mem_info.updated + freq <= now) {
        ca_get_mem_info();
    }

    if (ca_s_mem_info.mem_cache >= 0) {
        ca_snprintf(mem_cache, sizeof(mem_cache), "%L%Z",
                    ca_s_mem_info.mem_cache);
    } else {
        mem_cache[0] = '\0';
    }

    ca_s_mem_info.mem_cache = -1;

    return mem_cache;
}


u_char *
ca_get_mem_buffer(time_t now, time_t freq)
{
    static u_char  mem_buffer[20];

    if (ca_s_mem_info.updated + freq <= now) {
        ca_get_mem_info();
    }

    if (ca_s_mem_info.mem_buffer >= 0) {
        ca_snprintf(mem_buffer, sizeof(mem_buffer), "%L%Z",
                    ca_s_mem_info.mem_buffer);

    } else {
        mem_buffer[0] = '\0';
    }

    ca_s_mem_info.mem_buffer = -1;

    return mem_buffer;
}


u_char *
ca_get_mem_urate(time_t now, time_t freq)
{
    static u_char  mem_urate[20];

    if (ca_s_mem_info.updated + freq <= now) {
        ca_get_mem_info();
    }

    if (ca_s_mem_info.mem_urate >= 0) {
        ca_snprintf(mem_urate, sizeof(mem_urate), "%.0f%Z",
                    ca_s_mem_info.mem_urate);

    } else {
        mem_urate[0] = '\0';
    }

    ca_s_mem_info.mem_urate = -1;

    return mem_urate;
}


u_char *
ca_get_swap_urate(time_t now, time_t freq)
{
    static u_char  swap_urate[20];

    if (ca_s_mem_info.updated + freq <= now) {
        ca_get_mem_info();
    }

    if (ca_s_mem_info.swap_urate >= 0) {
        ca_snprintf(swap_urate, sizeof(swap_urate), "%.0f%Z",
                    ca_s_mem_info.swap_urate);

    } else {
        swap_urate[0] = '\0';
    }

    ca_s_mem_info.swap_urate = -1;

    return swap_urate;
}
