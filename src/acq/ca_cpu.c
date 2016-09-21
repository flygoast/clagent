#include "../clagent.h"
#include <ctype.h>
#include <time.h>


#define BUF_SIZE    1024


typedef struct ca_cpu_info_s {
    double  cpu_system;
    double  cpu_user;
    double  cpu_io;
    double  cpu_idle;
    int     procs_running;
    int     procs_blocked;
    time_t  updated;
} ca_cpu_info_t;


static ca_cpu_info_t ca_s_cpu_info = { -1, -1, -1, -1, -1, -1, 0 };


static void
ca_get_cpu_info(void)
{
    static int64_t   last_user   = -1;
    static int64_t   last_nice   = -1;
    static int64_t   last_syst   = -1;
    static int64_t   last_idle   = -1;
    static int64_t   last_iowait = -1;
    static int64_t   last_total  = -1;
    char             buf[BUF_SIZE];
    char            *fields[9];
    int              numfields;
    int64_t          user, nice, syst, idle, iowait, total, diff_total;
    FILE            *fh;

    fh = fopen("/proc/stat", "r");

    if (fh == NULL) {
        return;
    }

    while (fgets(buf, sizeof(buf), fh) != NULL) {
        if (strncasecmp(buf, "cpu ", 4) == 0) {
            numfields = ca_strsplit(buf, fields, 9);

            if (numfields < 5){
                continue;
            }

            user   = atoll(fields[1]);
            nice   = atoll(fields[2]);
            syst   = atoll(fields[3]);
            idle   = atoll(fields[4]);
            iowait = atoll(fields[5]);
            total  = user + nice + syst + idle + iowait;

            if (last_total < 0) {
                last_user   = user;
                last_nice   = nice;
                last_syst   = syst;
                last_idle   = idle;
                last_iowait = iowait;
                last_total  = total;
                continue;
            }

            diff_total = total - last_total;

            if (diff_total > 0) {
                ca_s_cpu_info.cpu_system = ((syst - last_syst) * 100)
                                           / (double) diff_total;
                ca_s_cpu_info.cpu_user = ((user - last_user) * 100)
                                         / (double) diff_total;
                ca_s_cpu_info.cpu_io = ((iowait - last_iowait) * 100)
                                       / (double) diff_total;
                ca_s_cpu_info.cpu_idle = ((idle - last_idle) * 100)
                                         / (double) diff_total;
            }

        } else if (strncasecmp(buf, "procs_running", 13) == 0) {
            numfields = ca_strsplit(buf, fields, 2);
            if (numfields < 2) {
                continue;
            }

            ca_s_cpu_info.procs_running = atoi(fields[1]);

        } else if (strncasecmp(buf, "procs_blocked", 13) == 0) {
            numfields = ca_strsplit(buf, fields, 2);
            if (numfields < 2) {
                continue;
            }
            ca_s_cpu_info.procs_blocked = atoi(fields[1]);

        } else {
            continue;
        }
    }

    fclose(fh);
    ca_s_cpu_info.updated = time(NULL);
}


u_char *
ca_get_cpu_system(time_t now, time_t freq)
{
    static u_char  cpu_system[10];

    if (ca_s_cpu_info.updated + freq <= now) {
        ca_get_cpu_info();
    }

    if (ca_s_cpu_info.cpu_system >= 0) {
        ca_snprintf(cpu_system, sizeof(cpu_system), "%.1f%Z",
                    ca_s_cpu_info.cpu_system);

    } else {
        cpu_system[0] = '\0';
    }

    ca_s_cpu_info.cpu_system = -1;
    
    return cpu_system;
}


u_char *
ca_get_cpu_user(time_t now, time_t freq)
{
    static u_char  cpu_user[10];

    if (ca_s_cpu_info.updated + freq <= now) {
        ca_get_cpu_info();
    }

    if (ca_s_cpu_info.cpu_user >= 0) {
        ca_snprintf(cpu_user, sizeof(cpu_user), "%.1f%Z",
                    ca_s_cpu_info.cpu_user);
    } else {
        cpu_user[0] = '\0';
    }

    ca_s_cpu_info.cpu_user = -1;

    return cpu_user;
}


u_char *
ca_get_cpu_io(time_t now, time_t freq)
{
    static u_char  cpu_io[10];

    if (ca_s_cpu_info.updated + freq <= now) {
        ca_get_cpu_info();
    }

    if (ca_s_cpu_info.cpu_io >= 0) {
        ca_snprintf(cpu_io, sizeof(cpu_io), "%.1f%Z", ca_s_cpu_info.cpu_io);

    } else {
        cpu_io[0] = '\0';
    }

    ca_s_cpu_info.cpu_io = -1;

    return cpu_io;
}


u_char *
ca_get_cpu_idle(time_t now, time_t freq)
{
    static u_char  cpu_idle[10];

    if (ca_s_cpu_info.updated + freq <= now) {
        ca_get_cpu_info();
    }

    if (ca_s_cpu_info.cpu_idle >= 0) {
        ca_snprintf(cpu_idle, sizeof(cpu_idle), "%.1f%Z",
                    ca_s_cpu_info.cpu_idle);

    } else {
        cpu_idle[0] = '\0';
    }

    ca_s_cpu_info.cpu_idle = -1;

    return cpu_idle;
}


u_char *
ca_get_procs_running(time_t now, time_t freq)
{
    static u_char  procs_running[10];

    if (ca_s_cpu_info.updated + freq <= now) {
        ca_get_cpu_info();
    }

    if (ca_s_cpu_info.procs_running >= 0) {
        ca_snprintf(procs_running, sizeof(procs_running), "%d%Z",
                    ca_s_cpu_info.procs_running);

    } else {
        procs_running[0] = '\0';
    }

    ca_s_cpu_info.procs_running = -1;

    return procs_running;
}


u_char *
ca_get_procs_blocked(time_t now, time_t freq)
{
    static u_char  procs_blocked[10];

    if (ca_s_cpu_info.updated + freq <= now) {
        ca_get_cpu_info();
    }

    if (ca_s_cpu_info.procs_blocked >= 0) {
        ca_snprintf(procs_blocked, sizeof(procs_blocked), "%d%Z",
                    ca_s_cpu_info.procs_blocked);

    } else {
        procs_blocked[0] = '\0';
    }

    ca_s_cpu_info.procs_blocked = -1;

    return procs_blocked;  
}
