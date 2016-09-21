#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h>
#include "clagent.h"
#include "json-c/json.h"


#define CA_RCV_BUF_SIZE  128
#define CA_ITEM_DATA_SIZE   8192


ca_acq_item_handler_t  ca_acq_item_handlers[] = {
    { ca_string("CPU_SYSTEM"),          &ca_get_cpu_system,          0 },
    { ca_string("CPU_USER"),            &ca_get_cpu_user,            0 },
    { ca_string("CPU_IDLE"),            &ca_get_cpu_idle,            0 },
    { ca_string("CPU_IO"),              &ca_get_cpu_io,              0 },
    { ca_string("PROC_RUNNING"),        &ca_get_procs_running,       0 },
    { ca_string("PROC_BLOCKED"),        &ca_get_procs_blocked,       0 },
    { ca_string("DISK_IO_UTIL_MAX"),    &ca_get_disk_io_util_max,    0 },
    { ca_string("PARTITION_MAX_URATE"), &ca_get_partition_max_urate, 0 },
    { ca_string("LOADAVG_1"),           &ca_get_loadavg_1,           0 },
    { ca_string("LOADAVG_5"),           &ca_get_loadavg_5,           0 },
    { ca_string("LOADAVG_15"),          &ca_get_loadavg_15,          0 },
    { ca_string("MEM_TOTAL"),           &ca_get_mem_total,           0 },
    { ca_string("MEM_USED"),            &ca_get_mem_used,            0 },
    { ca_string("MEM_FREE"),            &ca_get_mem_free,            0 },
    { ca_string("SWAP_TOTAL"),          &ca_get_swap_total,          0 },
    { ca_string("SWAP_USED"),           &ca_get_swap_used,           0 },
    { ca_string("SWAP_FREE"),           &ca_get_swap_free,           0 },
    { ca_string("MEM_CACHED"),          &ca_get_mem_cache,           0 },
    { ca_string("MEM_BUFFER"),          &ca_get_mem_buffer,          0 },
    { ca_string("MEM_URATE"),           &ca_get_mem_urate,           0 },
    { ca_string("SWAP_URATE"),          &ca_get_swap_urate,          0 },
    { ca_string("INTRANET_FLOW_IN"),    &ca_get_intranet_flow_in,    0 },
    { ca_string("INTRANET_FLOW_OUT"),   &ca_get_intranet_flow_out,   0 },
    { ca_string("EXTRANET_FLOW_IN"),    &ca_get_extranet_flow_in,    0 },
    { ca_string("EXTRANET_FLOW_OUT"),   &ca_get_extranet_flow_out,   0 },
    { ca_string("INTRANET_PKGS_IN"),    &ca_get_intranet_pkgs_in,    0 },
    { ca_string("INTRANET_PKGS_OUT"),   &ca_get_intranet_pkgs_out,   0 },
    { ca_string("EXTRANET_PKGS_IN"),    &ca_get_extranet_pkgs_in,    0 },
    { ca_string("EXTRANET_PKGS_OUT"),   &ca_get_extranet_pkgs_out,   0 },
    { ca_string("TOTAL_FLOW_IN"),       &ca_get_total_flow_in,       0 },
    { ca_string("TOTAL_FLOW_OUT"),      &ca_get_total_flow_out,      0 },
    { ca_string("TOTAL_PKGS_IN"),       &ca_get_total_pkgs_in,       0 },
    { ca_string("TOTAL_PKGS_OUT"),      &ca_get_total_pkgs_out,      0 },
    { ca_null_string,                   NULL }
};


typedef struct ca_acq_data_s      ca_acq_data_t;
typedef struct ca_acq_data_hdr_s  ca_acq_data_hdr_t;


struct ca_acq_data_s {
    json_object                  *json;
    STAILQ_ENTRY(ca_acq_data_s)   next;
};


STAILQ_HEAD(ca_acq_data_hdr_s, ca_acq_data_s);


static ca_uint_t          nfree;
static ca_acq_data_hdr_t  free_queue;
static ca_uint_t          max_nfree;
static ca_uint_t          ntask;
static ca_acq_data_hdr_t  task_queue;
static pthread_mutex_t    free_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t    task_mutex = PTHREAD_MUTEX_INITIALIZER;


static void *ca_acq_submit_cycle(void *dummy);
static void *ca_acq_cycle(void *dummy);
static ca_int_t ca_select_submit(ca_int_t *pfd, ca_int_t *pindex, char *buf,
    size_t len, ca_conf_ctx_t *conf);
static ca_int_t ca_select_send_and_recv(ca_int_t fd, char *buf, size_t len,
    ca_conf_ctx_t *conf, ca_server_t *server);


static ca_acq_data_t *
ca_acq_task_get(void)
{
    ca_acq_data_t  *data = NULL;

    pthread_mutex_lock(&task_mutex);

    if (!STAILQ_EMPTY(&task_queue)) {
        data = STAILQ_FIRST(&task_queue);
        ntask--;
        STAILQ_REMOVE_HEAD(&task_queue, next);
        STAILQ_NEXT(data, next) = NULL;
    }

    pthread_mutex_unlock(&task_mutex);

    return data;
}


static void
ca_acq_task_insert(ca_acq_data_t *data)
{
    pthread_mutex_lock(&task_mutex);

    STAILQ_INSERT_TAIL(&task_queue, data, next);
    ntask++;

    pthread_mutex_unlock(&task_mutex);
}


static ca_acq_data_t *
ca_acq_data_get(void)
{
    ca_acq_data_t  *data;

    pthread_mutex_lock(&free_mutex);

    if (!STAILQ_EMPTY(&free_queue)) {
        data = STAILQ_FIRST(&free_queue);
        nfree--;
        STAILQ_REMOVE_HEAD(&free_queue, next);

    } else {

        data = ca_calloc(sizeof(ca_acq_data_t), 1);
        if (data == NULL) {
            pthread_mutex_unlock(&free_mutex);
            return NULL;
        }
    }

    STAILQ_NEXT(data, next) = NULL;

    pthread_mutex_unlock(&free_mutex);

    return data;
}


static void
ca_acq_data_put(ca_acq_data_t *data)
{
    pthread_mutex_lock(&free_mutex);

    if (nfree != 0 && nfree + 1 > max_nfree) {
        ca_free(data);

    } else {
        nfree++;
        STAILQ_INSERT_HEAD(&free_queue, data, next);
    }

    pthread_mutex_unlock(&free_mutex);
}


static void
ca_acq_data_remove(ca_acq_data_hdr_t *queue, ca_acq_data_t *data)
{
    STAILQ_REMOVE(queue, data, ca_acq_data_s, next);
    STAILQ_NEXT(data, next) = NULL;
}


static void
ca_acq_data_init(ca_uint_t max)
{
    nfree = 0;
    STAILQ_INIT(&free_queue);
    max_nfree = max;
    ntask = 0;
    STAILQ_INIT(&task_queue);
}


static void
ca_acq_data_deinit(void)
{
    ca_acq_data_t  *data;

    while (!STAILQ_EMPTY(&free_queue)) {
        data = STAILQ_FIRST(&free_queue);
        ca_acq_data_remove(&free_queue, data);
        ca_free(data);
        nfree--;
    }

    while (!STAILQ_EMPTY(&task_queue)) {
        data = STAILQ_FIRST(&task_queue);
        ca_acq_data_remove(&task_queue, data);
        json_object_put(data->json);
        ca_free(data);
        ntask--;
    }
}


void
ca_acq_process_cycle(void *dummy)
{
    int                 s;
    sigset_t            set;
    ca_conf_ctx_t      *conf;
    pthread_t           acq, submit;
    void               *ret;

    conf = dummy;
    ca_process = CA_PROCESS_ACQ;

    ca_acq_data_init(conf->max_nfree);

    sigemptyset(&set);
    if (pthread_sigmask(SIG_SETMASK, &set, NULL) == -1) {
        ca_log_err(errno, "pthread_sigmask() failed");
    }

    pthread_create(&submit, NULL, ca_acq_submit_cycle, conf);
    pthread_create(&acq, NULL, ca_acq_cycle, conf);

    for ( ;; ) {
        if (ca_quit || ca_terminate) {
            break;
        }

        sigsuspend(&set);
        ca_log_debug(0, "sigsuspend");
    }
    
    ca_log_debug(0, "kill SIGINT to submit thread");
    s = pthread_kill(submit, SIGINT);
    if (s != 0) {
        ca_log_err(errno, "kill SIGINT to submit thread failed");
    }

    ca_log_debug(0, "kill SIGINT to acq thread");
    s = pthread_kill(acq, SIGINT);
    if (s != 0) {
        ca_log_err(errno, "kill SIGINT to acq thread failed");
    }

    pthread_join(submit, &ret);
    ca_log_debug(0, "submit thread exit");

    pthread_join(acq, &ret);
    ca_log_debug(0, "acq thread exit");

    ca_acq_data_deinit();

    ca_log_debug(0, "exit");

    exit(0);
}


static void *
ca_acq_cycle(void *dummy)
{
    time_t          now;
    u_char         *p, *tmp, *buf;
    ca_int_t        i, len, buf_size, interval;
    ca_acq_t       *item, *value;
    ca_conf_ctx_t  *conf;
    json_object    *json, *obj, *data_obj, *arr_obj;
    ca_acq_data_t  *data;

    conf = dummy;
    buf_size = CA_ITEM_DATA_SIZE;
    buf = ca_calloc(buf_size, sizeof(u_char));
    if (buf == NULL) {
        return NULL;
    }

    value = conf->acq_items->elem;

    for ( ;; ) {
        if (ca_quit || ca_terminate) {
            break;
        }

        now = time(&now);

        json = NULL;

        for (i = 0; i < conf->acq_items->nelem; i++) {
            item = &value[i];

            interval = now - item->accessed;

            if (interval < 0) {
                item->accessed = 0;

            } else if (interval >= item->freq) {

                ca_log_debug(0, "acq \"%V\"", &item->item);

                p = item->handler(now, item->freq);

                item->accessed = now;

                len = item->id_len + 1 + ca_strlen(p) + 1 + 2 + 1;

                while (len > buf_size) {
                    tmp = ca_realloc(buf, 2 * buf_size);
                    if (tmp == NULL) {

                        if (ca_quit || ca_terminate) {
                            goto over;
                        }

                        sleep(1);

                        if (ca_quit || ca_terminate) {
                            goto over;
                        }

                        continue;
                    }
                    buf = tmp;
                    buf_size = 2 * buf_size;
                }

                data_obj = json_object_new_array();

                ca_slprintf(buf, buf + buf_size, "%d%Z", item->id);
                obj = json_object_new_string((const char *) buf);
                json_object_array_add(data_obj, obj);

                ca_slprintf(buf, buf + buf_size, "%s%Z", p);
                obj = json_object_new_string((const char *)buf);
                json_object_array_add(data_obj, obj);

                obj = json_object_new_string("1");
                json_object_array_add(data_obj, obj);

                if (json == NULL) {
                    json = json_object_new_object();
                    obj = json_object_new_string_len(
                                              (const char *)conf->identify.data,
                                              (int)conf->identify.len);
                    json_object_object_add(json, "host", obj);
        
                    ca_slprintf(buf, buf + buf_size, "%ud%Z", now);
                    obj = json_object_new_string((const char *)buf);
                    json_object_object_add(json, "time", obj);
        
                    arr_obj = json_object_new_array();
                    json_object_object_add(json, "data", arr_obj);
                }

                ASSERT(json != NULL && arr_obj != NULL && data_obj != NULL);

                json_object_array_add(arr_obj, data_obj);
            }
        }

        if (json) {
            data = ca_acq_data_get(); 
            data->json = json;
            ca_acq_task_insert(data);
            json = NULL;
        }

        if (ca_quit || ca_terminate) {
            break;
        }

        sleep(1);
    }

over:

    if (json) {
        json_object_put(json);
        json = NULL;
    }

    ca_free(buf);

    return NULL;
}


static void *
ca_acq_submit_cycle(void *dummy)
{
    ca_conf_ctx_t  *conf;
    ca_acq_data_t  *data;
    ca_int_t        index, ret;
    ca_int_t        fd;
    char           *buf;
    size_t          len;

    conf = dummy;
    index = -1;
    fd = CA_ERROR;

    for ( ;; ) {
        if (ca_quit || ca_terminate) {
            break;
        }

        data = ca_acq_task_get();
        if (data == NULL) {
            if (ca_quit || ca_terminate) {
                break;
            }

            sleep(1);
            continue;
        }

        buf = (char *) json_object_get_string(data->json);
        len = ca_strlen(buf);

        ca_log_debug(0, "submit %d '%s'", len, buf);

        ret = ca_select_submit(&fd, &index, buf, len, conf);
        if (ret == CA_ERROR) {
            ca_log_err(0, "submit %d '%s' failed", len, buf);
        }

        json_object_put(data->json);
        ca_acq_data_put(data);
    }

    return NULL;
}


static ca_int_t
ca_net_nonblock(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        return CA_ERROR;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return CA_ERROR;
    }

    return CA_OK;
}


static ca_int_t 
tcp_connect_timeout(struct sockaddr *sa, int timeout)
{
    int             ret, fd, error, len;
    fd_set          w;
    struct timeval  tv;

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return CA_ERROR;
    }

    ca_net_nonblock(fd);

    if (connect(fd, sa, sizeof(struct sockaddr_in)) == 0) {
        return fd;
    }

    if (errno != EINPROGRESS) {
        return CA_ERROR;
    }

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    FD_ZERO(&w);
    FD_SET(fd, &w);

    ret = select(fd + 1, NULL, &w, NULL, &tv);

    if (ret == -1) {
        close(fd);
        return CA_ERROR;

    } else if (ret == 0) {
        close(fd);
        return CA_AGAIN;
    }

    if (FD_ISSET(fd, &w)) {
        len = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len) < 0)
        {
            close(fd);
            ca_log_alert(errno, "getsockopt SO_ERROR failed");
            return CA_ERROR;
        }

        if (error != 0) {
            errno = error;
            return CA_ERROR;
        }

    } else {
        close(fd);
        return CA_ERROR;
    }
    
    return fd;
}


static ca_int_t 
tcp_send_timeout(int fd, char *buf, size_t len, int timeout)
{
    ca_int_t        ret;
    fd_set          w;
    struct timeval  tv;

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    FD_ZERO(&w);
    FD_SET(fd, &w);

    ret = select(fd + 1, NULL, &w, NULL, &tv);
    if (ret == -1) {
        return CA_ERROR;

    } else if (ret == 0) {
        return CA_AGAIN;
    }

    if (!FD_ISSET(fd, &w)) {
        return CA_ERROR;
    }

    ret = send(fd, buf, len, 0);

    return ret;
}


static ca_int_t 
tcp_recv_timeout(int fd, char *buf, size_t len, int timeout)
{
    ca_int_t        ret;
    fd_set          r;
    struct timeval  tv;

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    FD_ZERO(&r);
    FD_SET(fd, &r);

    ret = select(fd + 1, &r, NULL, NULL, &tv);
    if (ret == -1) {
        return CA_ERROR;

    } else if (ret == 0) {
        return CA_AGAIN;
    }

    if (!FD_ISSET(fd, &r)) {
        return CA_ERROR;
    }

    ret = recv(fd, buf, len, 0);

    return ret;
}


static ca_int_t
ca_select_submit(ca_int_t *pfd, ca_int_t *pindex, char *buf, size_t len,
    ca_conf_ctx_t *conf)
{
    ca_int_t      i, index, fd;
    ca_uint_t     count;
    ca_server_t  *servers, *server;

    index = *pindex;
    fd = *pfd;
    count = conf->servers->nelem;
    servers = conf->servers->elem;

    if (fd != CA_ERROR) {
        server = &servers[index];
        if (ca_select_send_and_recv(fd, buf, len, conf, server) == CA_OK) {
            return CA_OK;
        }

        close(fd);
    }

    for (i = 0; i < count; i++) {
        server = &servers[(index + 1) % count];
        index = (index + 1) % count;

        fd = tcp_connect_timeout((struct sockaddr *) &server->sin,
                                 conf->connect_timeout);

        if (fd == CA_AGAIN) {
            ca_log_alert(0, "connect to \"%s\" timeout", server->addr_str);
            continue;

        } else if (fd == CA_ERROR) {
            ca_log_alert(errno, "connect to \"%s\" failed", server->addr_str);

            if (ca_quit || ca_terminate) {
                return CA_ERROR;
            }

            continue;
        }

        if (ca_select_send_and_recv(fd, buf, len, conf, server) == CA_OK) {
            *pfd = fd;
            *pindex = index;
            return CA_OK;
        }

        close(fd);
    }

    *pindex = index;
    *pfd = CA_ERROR;

    ca_log_alert(0, "send to all server failed");
    return CA_ERROR;
}


static ca_int_t
ca_select_send_and_recv(ca_int_t fd, char *buf, size_t len,
    ca_conf_ctx_t *conf, ca_server_t *server)
{
    size_t  ret, total;
    char    rcv_buf[CA_RCV_BUF_SIZE];
    char    header[10];

    ca_snprintf((u_char *) header, sizeof(header), "%010z", len);

    total = 0;

    while (total < 10) {
        ret = tcp_send_timeout(fd, header + total, 10 - total,
                               conf->send_timeout);
        if (ret == CA_AGAIN) {
            ca_log_alert(0, "send header to \"%s\" timeout",
                         server->addr_str);
            return CA_AGAIN;

        } else if (ret == CA_ERROR) {
            if (errno == EINTR) {
                if (!ca_quit && !ca_terminate) {
                    continue;
                }
            }

            ca_log_alert(errno, "send heaer to \"%s\" failed",
                         server->addr_str);
            return CA_ERROR;

        } else {
            total += ret;
        }
    }

    if (total < 10) {
        return CA_ERROR;
    }

    total = 0;
    while (total < len) {
        ret = tcp_send_timeout(fd, buf + total, len - total,
                               conf->send_timeout);
        if (ret == CA_AGAIN) {
            ca_log_alert(0, "send body to \"%s\" timeout",
                         server->addr_str);
            return CA_AGAIN;

        } else if (ret == CA_ERROR) {
            if (errno == EINTR) {
                if (!ca_quit && !ca_terminate) {
                    continue;
                }
            }

            ca_log_alert(errno, "send body to \"%s\" failed",
                         server->addr_str);
            total = 0;
            break;

        } else {
            total += ret;
        }
    }

    if (total < len) {
        return CA_ERROR;
    }

    total = 0;
    for ( ;; ) {
        ret = tcp_recv_timeout(fd, rcv_buf + total, sizeof(rcv_buf) - total,
                               conf->recv_timeout);
        if (ret == CA_AGAIN) {
            ca_log_alert(0, "recv response from \"%s\" timeout",
                         server->addr_str);
            return CA_AGAIN;

        } else if (ret == CA_ERROR) {

            if (errno == EINTR) {
                if (!ca_quit && !ca_terminate) {
                    continue;
                }
            }

            ca_log_alert(errno, "recv response from \"%s\" failed",
                         server->addr_str);
            return CA_ERROR;
        }

        total += ret;

        if (total < sizeof("ok\n") - 1) {
            continue;
        }

        if (rcv_buf[0] != 'o' || rcv_buf[1] != 'k' || rcv_buf[2] != '\n') {
            ca_log_alert(0, "invalid response from \"%s\": \"%*s\"",
                         server->addr_str, total, rcv_buf);

            return CA_ERROR;
        }

        return CA_OK;
    }

    return CA_ERROR;
}
