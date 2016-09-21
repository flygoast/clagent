#include "../clagent.h"
#include <time.h>
#include <sys/statvfs.h>


#define MAX_PARTITION_NUM          50
#define MAX_PATTITION_NAME_LENGTH  50


typedef struct ca_partition_info_s {
    char  name[MAX_PATTITION_NAME_LENGTH];
    int   urate;
} ca_partition_info_t;


typedef struct ca_disk_urate_info_s {
    char                 fs_type[MAX_PARTITION_NUM][MAX_PATTITION_NAME_LENGTH];
    int                  fs_type_num;
    ca_partition_info_t  partition_info[MAX_PARTITION_NUM];
    int                  partition_num;
    int                  partition_max_urate;
    time_t               updated;
} ca_disk_urate_info_t;


static ca_disk_urate_info_t  ca_s_disk_urate_info = { 
    .fs_type_num         = 0,
    .partition_num       = 0,
    .partition_max_urate = -1,
    .updated             = 0,
};


static const char *black_fs_type[] = { "iso9660" };


static void
ca_get_disk_urate_info(void)
{
    int              i, n;
    char             buf[1024];
    char            *fields[6];
    int              numfields;
    const char      *fs_type = NULL;
    FILE            *fh;
    struct statvfs   fs_stat;

    fh = fopen("/proc/filesystems", "r");
    if (fh == NULL) {
        return;
    }

    ca_s_disk_urate_info.fs_type_num = 0;
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        if (strncasecmp(buf, "nodev", 5) == 0) {
            continue;
        }

        fs_type = ca_trim(buf);
        if (ca_in_array(fs_type, black_fs_type, 
                        sizeof(black_fs_type) / sizeof(black_fs_type[0])))
        {
            continue;
        }

        n = ca_s_disk_urate_info.fs_type_num++;
        strncpy(ca_s_disk_urate_info.fs_type[n], fs_type,
                MAX_PATTITION_NAME_LENGTH);
    }

    n = ca_s_disk_urate_info.fs_type_num++;
    strncpy(ca_s_disk_urate_info.fs_type[n], "nfs", MAX_PATTITION_NAME_LENGTH);
    n = ca_s_disk_urate_info.fs_type_num++;
    strncpy(ca_s_disk_urate_info.fs_type[n], "nfs4", MAX_PATTITION_NAME_LENGTH);
    fclose(fh);

    if (ca_s_disk_urate_info.fs_type_num == 0) {
        return;
    }
    
    ca_s_disk_urate_info.partition_num = 0;
    fh = fopen("/etc/mtab", "r");
    if (fh == NULL) {
        return;
    }

    while (fgets(buf, sizeof(buf), fh) != NULL) {
        if (strncasecmp(buf, "none", 4) == 0) {
            continue;
        }

        numfields = ca_strsplit(buf, fields, 6);
        if (numfields < 3) {
            continue;
        }

        for (i = 0; i < ca_s_disk_urate_info.fs_type_num; i++) {
            if (strcmp(fields[2], ca_s_disk_urate_info.fs_type[i]) == 0) {
                n = ca_s_disk_urate_info.partition_num++;
                strncpy(ca_s_disk_urate_info.partition_info[n].name, fields[1],
                        MAX_PATTITION_NAME_LENGTH);
            }
        }
    }

    fclose(fh);

    if (ca_s_disk_urate_info.partition_num == 0) {
        return;
    }
    
    for (i = 0; i < ca_s_disk_urate_info.partition_num; i++) {
        if (statvfs(ca_s_disk_urate_info.partition_info[i].name, &fs_stat) == 0
            && fs_stat.f_blocks > 0)
        {
            ca_s_disk_urate_info.partition_info[i].urate = 
                       ((fs_stat.f_blocks - fs_stat.f_bfree) * 100)
                       / (fs_stat.f_blocks - fs_stat.f_bfree + fs_stat.f_bavail)
                       + 1;

            if (ca_s_disk_urate_info.partition_max_urate
                < ca_s_disk_urate_info.partition_info[i].urate)
            {
                ca_s_disk_urate_info.partition_max_urate = 
                                   ca_s_disk_urate_info.partition_info[i].urate;
            }

        } else {
            ca_s_disk_urate_info.partition_info[i].urate = -1.0;
        }
    }

    ca_s_disk_urate_info.updated = time(NULL);
}


u_char *
ca_get_partition_max_urate(time_t now, time_t freq)
{
    static u_char  partition_max_urate[10];

    if (ca_s_disk_urate_info.updated + freq <= now) {
        ca_get_disk_urate_info();
    }

    if (ca_s_disk_urate_info.partition_max_urate >= 0) {
        ca_snprintf(partition_max_urate, sizeof(partition_max_urate), "%d%Z",
                    ca_s_disk_urate_info.partition_max_urate);
    } else {
        partition_max_urate[0] = '\0';
    }

    ca_s_disk_urate_info.partition_max_urate = -1.0;

    return partition_max_urate;
}
