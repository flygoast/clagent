#include "../clagent.h"
#include <time.h>


typedef struct ca_loadavg_info_s {
    float   loadavg_1;
    float   loadavg_5;
    float   loadavg_15;
    time_t  updated;
} ca_loadavg_info_t;


static ca_loadavg_info_t  ca_s_loadavg_info = { -1, -1, -1, 0 };


static void
ca_get_loadavg_info(void)
{
    char    buffer[16];
    char   *fields[8];
	int     numfields;
    FILE   *loadavg;
    
    loadavg = fopen("/proc/loadavg", "r");
    if (loadavg == NULL) {
        return;
    }

    if (fgets(buffer, sizeof(buffer), loadavg) == NULL) {
        fclose(loadavg);
        return;
    }
    fclose(loadavg);
    
    numfields = ca_strsplit(buffer, fields, 8);
    if (numfields < 3) {
        return;
    }

    ca_s_loadavg_info.loadavg_1  = atof(fields[0]);
    ca_s_loadavg_info.loadavg_5  = atof(fields[1]);
    ca_s_loadavg_info.loadavg_15 = atof(fields[2]);

    ca_s_loadavg_info.updated = time(NULL);
}


u_char *
ca_get_loadavg_1(time_t now, time_t freq)
{
    static u_char  loadavg_1[10];

    if (ca_s_loadavg_info.updated + freq <= now) {
        ca_get_loadavg_info();
    }

    if (ca_s_loadavg_info.loadavg_1 >= 0) {
        ca_snprintf(loadavg_1, sizeof(loadavg_1), "%.2f%Z",
                    ca_s_loadavg_info.loadavg_1);
    } else {
        loadavg_1[0] = '\0';
    }

    ca_s_loadavg_info.loadavg_1 = -1;

    return loadavg_1;
}


u_char *
ca_get_loadavg_5(time_t now, time_t freq)
{
    static u_char  loadavg_5[10];

    if (ca_s_loadavg_info.updated + freq <= now) {
        ca_get_loadavg_info();
    }

    if (ca_s_loadavg_info.loadavg_5 >= 0) {
        ca_snprintf(loadavg_5, sizeof(loadavg_5), "%.2f%Z",
                    ca_s_loadavg_info.loadavg_5);

    } else {
        loadavg_5[0] = '\0';
    }

    ca_s_loadavg_info.loadavg_5 = -1;

    return loadavg_5;
}


u_char *
ca_get_loadavg_15(time_t now, time_t freq)
{
    static u_char  loadavg_15[10];

    if (ca_s_loadavg_info.updated + freq <= now) {
        ca_get_loadavg_info();
    }

    if (ca_s_loadavg_info.loadavg_15 >= 0) {
        ca_snprintf(loadavg_15, sizeof(loadavg_15), "%.2f%Z",
                    ca_s_loadavg_info.loadavg_15);
    } else {
        loadavg_15[0] = '\0';
    }

    ca_s_loadavg_info.loadavg_15 = -1;

    return loadavg_15;
}
