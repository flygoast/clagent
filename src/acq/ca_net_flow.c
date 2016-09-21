#include "../clagent.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>


#define MAX_NAME_LENGTH     20
#define MAX_IP_LENTH        16
#define MAX_ETH_NUM         10


typedef struct  ca_eth_info_s {
    char     name[MAX_NAME_LENGTH];
    char     ip[MAX_IP_LENTH];
    int64_t  receive_bytes;
    int64_t  receive_pkgs;
    int64_t  transmit_bytes;
    int64_t  transmit_pkgs;
} ca_eth_info_t;


typedef struct  ca_ethstat_info_s {
    ca_eth_info_t  eth_info[MAX_ETH_NUM];
    int            eth_num;
    int64_t        intranet_flow_in;
    int64_t        extranet_flow_in;
    int64_t        intranet_pkgs_in;
    int64_t        extranet_pkgs_in;
    int64_t        intranet_flow_out;
    int64_t        extranet_flow_out;
    int64_t        intranet_pkgs_out;
    int64_t        extranet_pkgs_out;
    int64_t        total_flow_in;
    int64_t        total_pkgs_in;
    int64_t        total_flow_out;
    int64_t        total_pkgs_out;
} ca_ethstat_info_t;


static ca_ethstat_info_t  ca_s_ethstat_info = { 
    .eth_num           = 0,
    .intranet_flow_in  = -1,
    .extranet_flow_in  = -1,
    .intranet_pkgs_in  = -1,
    .extranet_pkgs_in  = -1,
    .intranet_flow_out = -1,
    .extranet_flow_out = -1,
    .intranet_pkgs_out = -1,
    .extranet_pkgs_out = -1,
    .total_flow_in     = -1,
    .total_pkgs_in     = -1,
    .total_flow_out    = -1,
    .total_pkgs_out    = -1,
};


static time_t  ca_s_last_time = 0;


static const char *
ca_get_ip_by_ethname(const char *ethname)
{
    int           sockfd;
    struct ifreq  ifr;

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        return NULL;
    }

    strncpy(ifr.ifr_name, ethname, sizeof(ifr.ifr_name));
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == -1) {
        close(sockfd);
        return NULL;

    } else {
        close(sockfd);
        return inet_ntoa((*((struct sockaddr_in *)&ifr.ifr_addr)).sin_addr);
    }
}


static int
ca_get_eth_index(const char *ethname)
{
    int  i;

    if (ca_s_ethstat_info.eth_num <= 0) {
        return -1;
    }

    for (i = 0; i < ca_s_ethstat_info.eth_num; i++) {
        if (strcmp(ethname, ca_s_ethstat_info.eth_info[i].name) == 0) {
            return i;
        }
    }

    return -1;
}


static void
ca_get_ethstat_info(void)
{
    char         buf[1024];
    FILE        *fh;
    char        *line, *p, *fields[16];
    int          numfields, count, index, n;
    int64_t      receive_bytes, receive_pkgs, transmit_bytes, transmit_pkgs;
    int64_t      old_value, avg_receive_bytes, avg_receive_pkgs;
    int64_t      avg_transmit_bytes, avg_transmit_pkgs;
    const char  *ip;
    time_t       current_time, diff_time;

    fh = fopen("/proc/net/dev", "r");
    if (fh == NULL) {
        return;
    }

    current_time = time(NULL);
    if (ca_s_last_time == 0) {
        ca_s_last_time = current_time;
    }

    count = 0;
    index = -1;
    diff_time = current_time - ca_s_last_time;

    while (fgets(buf, sizeof(buf), fh) != NULL) {
        if (++count < 3) {
            continue;
        }

        line = ca_trim(buf);
        if (strncasecmp(line, "lo:", 3) == 0) {
            continue;
        }

        p = strchr(line, ':');
        if (p == NULL) {
            continue;
        }
        *p++ = '\0';
        numfields = ca_strsplit(p, fields, 16);
        if (numfields < 10) {
            continue;
        }

        receive_bytes  = atoll(fields[0]);
        receive_pkgs   = atoll(fields[1]);
        transmit_bytes = atoll(fields[8]);
        transmit_pkgs  = atoll(fields[9]);
        index = ca_get_eth_index(line);
        ip = ca_get_ip_by_ethname(line);
        if (index < 0) { 
            if (ip == NULL && strncasecmp(line, "eth", 3) != 0) {
                continue;
            }
            n = ca_s_ethstat_info.eth_num;
            strncpy(ca_s_ethstat_info.eth_info[n].name, line, MAX_NAME_LENGTH);
            if (ip != NULL) {
                strncpy(ca_s_ethstat_info.eth_info[n].ip, ip, MAX_IP_LENTH);
            }

            ca_s_ethstat_info.eth_info[n].receive_bytes  = receive_bytes;
            ca_s_ethstat_info.eth_info[n].receive_pkgs   = receive_pkgs;
            ca_s_ethstat_info.eth_info[n].transmit_bytes = transmit_bytes;
            ca_s_ethstat_info.eth_info[n].transmit_pkgs  = transmit_pkgs;
            ca_s_ethstat_info.eth_num++;

        } else {
            if (diff_time <= 0) {
                continue;
            }

            old_value = ca_s_ethstat_info.eth_info[index].receive_bytes;
            avg_receive_bytes = (receive_bytes - old_value) / diff_time;

            old_value = ca_s_ethstat_info.eth_info[index].receive_pkgs;
            avg_receive_pkgs = (receive_pkgs - old_value) / diff_time;

            old_value = ca_s_ethstat_info.eth_info[index].transmit_bytes;
            avg_transmit_bytes = (transmit_bytes - old_value) / diff_time;

            old_value = ca_s_ethstat_info.eth_info[index].transmit_pkgs;
            avg_transmit_pkgs = (transmit_pkgs - old_value) / diff_time;

            if (ca_s_ethstat_info.intranet_flow_in < 0) {
                ca_s_ethstat_info.intranet_flow_in = 0;
            }

            if (ca_s_ethstat_info.intranet_pkgs_in < 0) {
                ca_s_ethstat_info.intranet_pkgs_in = 0;
            }

            if (ca_s_ethstat_info.intranet_flow_out < 0) {
                ca_s_ethstat_info.intranet_flow_out = 0;
            }

            if (ca_s_ethstat_info.intranet_pkgs_out < 0) {
                ca_s_ethstat_info.intranet_pkgs_out = 0;
            }

            if (ca_s_ethstat_info.extranet_flow_in < 0) {
                ca_s_ethstat_info.extranet_flow_in = 0;
            }

            if (ca_s_ethstat_info.extranet_pkgs_in < 0) {
                ca_s_ethstat_info.extranet_pkgs_in = 0;
            }

            if (ca_s_ethstat_info.extranet_flow_out < 0) {
                ca_s_ethstat_info.extranet_flow_out = 0;
            }

            if (ca_s_ethstat_info.extranet_pkgs_out < 0) {
                ca_s_ethstat_info.extranet_pkgs_out = 0;
            }

            if (ca_s_ethstat_info.total_flow_in < 0) {
                ca_s_ethstat_info.total_flow_in = 0;
            }

            if (ca_s_ethstat_info.total_pkgs_in < 0) {
                ca_s_ethstat_info.total_pkgs_in = 0;
            }

            if (ca_s_ethstat_info.total_flow_out < 0) {
                ca_s_ethstat_info.total_flow_out = 0;
            }

            if (ca_s_ethstat_info.total_pkgs_out < 0) {
                ca_s_ethstat_info.total_pkgs_out = 0;
            }

            if (ip != NULL
                && strncmp(ip, "10.", 3) != 0 
                && strncmp(ip, "192.", 4) != 0 
                && strncmp(ip, "172.", 4) != 0)
            {
                ca_s_ethstat_info.extranet_flow_in  += avg_receive_bytes;
                ca_s_ethstat_info.extranet_pkgs_in  += avg_receive_pkgs;
                ca_s_ethstat_info.extranet_flow_out += avg_transmit_bytes;
                ca_s_ethstat_info.extranet_pkgs_out += avg_transmit_pkgs;
            } else {
                ca_s_ethstat_info.intranet_flow_in  += avg_receive_bytes;
                ca_s_ethstat_info.intranet_pkgs_in  += avg_receive_pkgs;
                ca_s_ethstat_info.intranet_flow_out += avg_transmit_bytes;
                ca_s_ethstat_info.intranet_pkgs_out += avg_transmit_pkgs;
            }

            ca_s_ethstat_info.total_flow_in   += avg_receive_bytes;
            ca_s_ethstat_info.total_pkgs_in   += avg_receive_pkgs;
            ca_s_ethstat_info.total_flow_out  += avg_transmit_bytes;
            ca_s_ethstat_info.total_pkgs_out  += avg_transmit_pkgs;

            if (ip != NULL) {
                strncpy(ca_s_ethstat_info.eth_info[index].ip, ip, MAX_IP_LENTH);
            }

            ca_s_ethstat_info.eth_info[index].receive_bytes  = receive_bytes;
            ca_s_ethstat_info.eth_info[index].receive_pkgs   = receive_pkgs;
            ca_s_ethstat_info.eth_info[index].transmit_bytes = transmit_bytes;
            ca_s_ethstat_info.eth_info[index].transmit_pkgs  = transmit_pkgs;
        }
    }

    fclose(fh);

    ca_s_last_time = current_time;
}


u_char *
ca_get_intranet_flow_in(time_t now, time_t freq)
{
    static u_char  intranet_flow_in[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.intranet_flow_in >= 0) {
        ca_snprintf(intranet_flow_in, sizeof(intranet_flow_in), "%L%Z",
                    ca_s_ethstat_info.intranet_flow_in);

    } else {
        intranet_flow_in[0] = '\0';
    }

    ca_s_ethstat_info.intranet_flow_in = -1;

    return intranet_flow_in;
}


u_char *
ca_get_extranet_flow_in(time_t now, time_t freq)
{
    static u_char  extranet_flow_in[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.extranet_flow_in >= 0) {
        ca_snprintf(extranet_flow_in, sizeof(extranet_flow_in), "%L%Z",
                    ca_s_ethstat_info.extranet_flow_in);

    } else {
        extranet_flow_in[0] = '\0';
    }

    ca_s_ethstat_info.extranet_flow_in = -1;

    return extranet_flow_in;    
}


u_char *
ca_get_intranet_pkgs_in(time_t now, time_t freq)
{
    static u_char  intranet_pkgs_in[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.intranet_pkgs_in >= 0) {
        ca_snprintf(intranet_pkgs_in, sizeof(intranet_pkgs_in), "%L%Z",
                    ca_s_ethstat_info.intranet_pkgs_in);

    } else {
        intranet_pkgs_in[0] = '\0';
    }

    ca_s_ethstat_info.intranet_pkgs_in = -1;

    return intranet_pkgs_in;    
}


u_char *
ca_get_extranet_pkgs_in(time_t now, time_t freq)
{
    static u_char  extranet_pkgs_in[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.extranet_pkgs_in >= 0) {
        ca_snprintf(extranet_pkgs_in, sizeof(extranet_pkgs_in), "%L%Z",
                    ca_s_ethstat_info.extranet_pkgs_in);

    } else {
        extranet_pkgs_in[0] = '\0';
    }

    ca_s_ethstat_info.extranet_pkgs_in = -1;

    return extranet_pkgs_in;    
}


u_char *
ca_get_intranet_flow_out(time_t now, time_t freq)
{
    static u_char  intranet_flow_out[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.intranet_flow_out >= 0) {
        ca_snprintf(intranet_flow_out, sizeof(intranet_flow_out), "%L%Z",
                    ca_s_ethstat_info.intranet_flow_out);

    } else {
        intranet_flow_out[0] = '\0';
    }

    ca_s_ethstat_info.intranet_flow_out = -1;

    return intranet_flow_out;
}


u_char *
ca_get_extranet_flow_out(time_t now, time_t freq)
{
    static u_char  extranet_flow_out[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.extranet_flow_out >= 0) {
        ca_snprintf(extranet_flow_out, sizeof(extranet_flow_out), "%L%Z",
                    ca_s_ethstat_info.extranet_flow_out);

    } else {
        extranet_flow_out[0] = '\0';
    }

    ca_s_ethstat_info.extranet_flow_out = -1;

    return extranet_flow_out;    
}


u_char *
ca_get_intranet_pkgs_out(time_t now, time_t freq)
{
    static u_char  intranet_pkgs_out[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.intranet_pkgs_out >= 0) {
        ca_snprintf(intranet_pkgs_out, sizeof(intranet_pkgs_out), "%L%Z",
                    ca_s_ethstat_info.intranet_pkgs_out);

    } else {
        intranet_pkgs_out[0] = '\0';
    }

    ca_s_ethstat_info.intranet_pkgs_out = -1;

    return intranet_pkgs_out;    
}


u_char *
ca_get_extranet_pkgs_out(time_t now, time_t freq)
{
    static u_char  extranet_pkgs_out[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.extranet_pkgs_out >= 0) {
        ca_snprintf(extranet_pkgs_out, sizeof(extranet_pkgs_out), "%L%Z",
                    ca_s_ethstat_info.extranet_pkgs_out);

    } else {
        extranet_pkgs_out[0] = '\0';
    }

    ca_s_ethstat_info.extranet_pkgs_out = -1;

    return extranet_pkgs_out;    
}


u_char *
ca_get_total_flow_in(time_t now, time_t freq)
{
    static u_char  total_flow_in[20];
    
    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.total_flow_in >= 0) {
        ca_snprintf(total_flow_in, sizeof(total_flow_in), "%L%Z",
                    ca_s_ethstat_info.total_flow_in);

    } else {
        total_flow_in[0] = '\0';
    }

    ca_s_ethstat_info.total_flow_in = -1;

    return total_flow_in;     
}


u_char *
ca_get_total_flow_out(time_t now, time_t freq)
{
    static u_char  total_flow_out[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.total_flow_out >= 0) {
        ca_snprintf(total_flow_out, sizeof(total_flow_out), "%L%Z",
                    ca_s_ethstat_info.total_flow_out);

    } else {
        total_flow_out[0] = '\0';
    }

    ca_s_ethstat_info.total_flow_out = -1;

    return total_flow_out;      
}


u_char *
ca_get_total_pkgs_in(time_t now, time_t freq)
{
    static u_char  total_pkgs_in[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.total_pkgs_in >= 0) {
        ca_snprintf(total_pkgs_in, sizeof(total_pkgs_in), "%L%Z",
                    ca_s_ethstat_info.total_pkgs_in);

    } else {
        total_pkgs_in[0] = '\0';
    }

    ca_s_ethstat_info.total_pkgs_in = -1;

    return total_pkgs_in;        
}


u_char *
ca_get_total_pkgs_out(time_t now, time_t freq)
{
    static u_char  total_pkgs_out[20];

    if (ca_s_last_time + freq <= now) {
        ca_get_ethstat_info();
    }

    if (ca_s_ethstat_info.total_pkgs_out >= 0) {
        ca_snprintf(total_pkgs_out, sizeof(total_pkgs_out), "%L%Z",
                    ca_s_ethstat_info.total_pkgs_out);

    } else {
        total_pkgs_out[0] = '\0';
    }

    ca_s_ethstat_info.total_pkgs_out = -1;

    return total_pkgs_out;     
}
