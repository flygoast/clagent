#include "../clagent.h"
#include <time.h>


#define MAX_NAME_LENGTH 20
#define MAX_PARTITION_NUM 20


typedef struct ca_disk_io_s {
    char     name[MAX_NAME_LENGTH];
    int64_t  rio;
    int64_t  rmerge;
    int64_t  rsect;
    int64_t  ruse;
    int64_t  wio;
    int64_t  wmerge;
    int64_t  wsect;
    int64_t  wuse;
    int64_t  use;
    int64_t  aveq;
} ca_disk_io_t;


typedef struct  ca_disk_io_info_s {
    ca_disk_io_t  disk_io[MAX_PARTITION_NUM];
    int           disk_num;
    double        disk_io_util_max;
} ca_disk_io_info_t;


static ca_disk_io_info_t  ca_s_disk_io_info = {
    .disk_num = 0,
    .disk_io_util_max = -1.0
};


static time_t  ca_s_last_time = 0;


static int
ca_get_disk_index(const char *diskname)
{
    int i;

    if (ca_s_disk_io_info.disk_num <= 0) {
        return -1;
    }

    for (i = 0; i < ca_s_disk_io_info.disk_num; i++) {
        if (ca_strcmp(diskname, ca_s_disk_io_info.disk_io[i].name) == 0) {
            return i;
        }
    }

    return -1;
}


static void
ca_get_disk_io_info(void)
{
    char      buf[1024];
    int       count;
    char     *fields[14];
    int       numfields;
    int       index;
    int64_t   rio, rmerge, rsect, ruse, wio, wmerge, wsect, wuse, use, aveq;
    double    util;
    time_t    current_time, diff_time;
    FILE     *fh;
    
    current_time = time(NULL);

    if (ca_s_last_time == 0) {
        ca_s_last_time = current_time;
    }

    diff_time = current_time - ca_s_last_time;
    
    fh = fopen("/proc/partitions", "r");
    if (fh == NULL) {
        return;
    }

    count = 0;
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        if (count++ < 2) {
            continue;
        }

        numfields = ca_strsplit(buf, fields, 4);
        if (numfields < 4) {
            continue;
        }

        if (ca_get_disk_index(fields[3]) < 0) {
            strncpy(ca_s_disk_io_info.disk_io[ca_s_disk_io_info.disk_num].name,
                    fields[3], MAX_NAME_LENGTH);
            ca_s_disk_io_info.disk_io[ca_s_disk_io_info.disk_num].rio = -1;
            ca_s_disk_io_info.disk_num++;
        }
    }

    fclose(fh);

    fh = fopen("/proc/diskstats", "r");
    if (fh == NULL) {
        return;
    }

    while (fgets(buf, sizeof(buf), fh) != NULL) {
        numfields = ca_strsplit(buf, fields, 14);
        if (numfields < 14) {
            continue;
        }

        index = ca_get_disk_index(fields[2]);
        if (index < 0) {
            continue;
        }

        rio    = atoll(fields[3]);
        rmerge = atoll(fields[4]);
        rsect  = atoll(fields[5]);
        ruse   = atoll(fields[6]);
        wio    = atoll(fields[7]);
        wmerge = atoll(fields[8]);
        wsect  = atoll(fields[9]);
        wuse   = atoll(fields[10]);
        use    = atoll(fields[12]);
        aveq   = atoll(fields[13]);

        if (ca_s_disk_io_info.disk_io[index].rio >= 0 && diff_time > 0) {
            util = (use - ca_s_disk_io_info.disk_io[index].use) * 100.0 
                   / (diff_time * 1000);
            if (util > ca_s_disk_io_info.disk_io_util_max) {
                ca_s_disk_io_info.disk_io_util_max = util;
            }
        }

        ca_s_disk_io_info.disk_io[index].rio    = rio;
        ca_s_disk_io_info.disk_io[index].rmerge = rmerge;
        ca_s_disk_io_info.disk_io[index].rsect  = rsect;
        ca_s_disk_io_info.disk_io[index].ruse   = ruse;
        ca_s_disk_io_info.disk_io[index].wio    = wio;
        ca_s_disk_io_info.disk_io[index].wmerge = wmerge;
        ca_s_disk_io_info.disk_io[index].wsect  = wsect;
        ca_s_disk_io_info.disk_io[index].wuse   = wuse;
        ca_s_disk_io_info.disk_io[index].use    = use;
        ca_s_disk_io_info.disk_io[index].aveq   = aveq;
    }

    fclose(fh);
    
    ca_s_last_time = current_time;
}


u_char *
ca_get_disk_io_util_max(time_t now, time_t freq)
{
    static u_char  disk_io_util_max[10];

    if (ca_s_last_time + freq <= now) {
        ca_get_disk_io_info();
    }

    if (ca_s_disk_io_info.disk_io_util_max >= 0) {
        ca_snprintf(disk_io_util_max, sizeof(disk_io_util_max), "%.2f%Z",
                    ca_s_disk_io_info.disk_io_util_max);

    } else {
        disk_io_util_max[0] = '\0';
    }

    ca_s_disk_io_info.disk_io_util_max = -1.0;

    return disk_io_util_max;
}
