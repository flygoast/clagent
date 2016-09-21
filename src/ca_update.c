#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include "clagent.h"
#include "curl/curl.h"


#define CA_RESPONSE_SIZE    4096


static ca_uint_t  ca_new_version;


static int ca_need_update(u_char *response);
static pid_t ca_update(char *update_exe);


static size_t
curl_write_cb(char *ptr, size_t size, size_t count, void *buf)
{
    size_t n;
    
    n= CA_MIN((size * count), CA_RESPONSE_SIZE - ca_strlen((u_char *) buf));
    ca_memcpy((u_char *) buf + ca_strlen((u_char *) buf), ptr, n);

    return n;
}


void
ca_update_process_cycle(void *dummy)
{
    sigset_t            set;
    ca_conf_ctx_t      *conf;
    struct curl_slist  *list;
    CURL               *curl;
    CURLcode            res;
    u_char              buf[1024];
    u_char              response[CA_RESPONSE_SIZE];
    long                rc;
    pid_t               ret, pid;
    int                 status;

    conf = dummy;
    ca_process = CA_PROCESS_UPDATE;
    pid = CA_INVALID_PID;

    sigemptyset(&set);
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        ca_log_err(errno, "sigprocmask() failed");
    }

    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        ca_log_alert(0, "curl_global_init() failed");
        return;
    }

    ca_snprintf(buf, sizeof(buf), "%V?identify=%V&version=%s%Z",
                &conf->update_url,
                &conf->identify,
                CA_VERSION);

    list = NULL;
    list = curl_slist_append(list, "User-Agent: " PROG_NAME "/" CA_VERSION);

    curl = curl_easy_init(); 


    while (1) {
        if (ca_quit || ca_terminate) {
            break;
        }

        ca_memzero(response, sizeof(response));

        if (!curl) {
            ca_log_err(0, "curl_easy_init() failed");
            goto next;
        }

        curl_easy_setopt(curl, CURLOPT_URL, buf);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        res = curl_easy_perform(curl);
        if (res != 0) {
            ca_log_err(0, "curl_easy_perform failed: %s",
                       curl_easy_strerror(res));
            goto next;
        }

        res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rc);
        if (res != 0) {
            ca_log_err(0, "curl_easy_getinfo failed: %s",
                       curl_easy_strerror(res));
            goto next;
        }

        if (rc != 200) {
            ca_log_err(0, "HTTP code: %ld", rc);
            goto next;
        }

        response[sizeof(response) - 1] = '\0';

        if (ca_need_update(response)) {
            pid = ca_update((char *) conf->update_exe.data);            
            if (pid == CA_INVALID_PID) {
                goto next;
            }

            for ( ;; ) {
                ret = wait(&status);
                if (ret == -1) {
                    if (errno == EINTR) {
                        if (ca_quit || ca_terminate) {
                            kill(pid, SIGTERM);
                        }
                        continue;
                    }

                    ca_log_alert(errno, "wait() failed, status: %d", status);
                    break;
                }

                if (!WIFEXITED(status)) {
                    ca_log_alert(0,  "update process exit abnormally");
                    break;
                }
                break;
            }

            pid = CA_INVALID_PID;
        }

next:

        if (ca_quit || ca_terminate) {
            break;
        }

        sleep(conf->check_interval);
    }

    if (pid != CA_INVALID_PID) {
        kill(pid, SIGTERM);
    }

    curl_easy_cleanup(curl);

    curl_slist_free_all(list);

    curl_global_cleanup();

    ca_log_debug(0, "exit");

    exit(0);
}


static int
ca_need_update(u_char *response)
{
    u_char    *p, *s, *end;
    ca_int_t   n, m;
    ca_uint_t  version;

    end = response + ca_strlen(response) - 1;

    while (*response && (*response == ' ' || *response == '\t')) {
        response++;
    }

    while (*end == ' ' || *end == '\t') {
        *end-- = '\0';
    }

    p = response;
    n = 0;
    version = 0;

    for ( ;; ) {

        s = p;

        while (*p && *p != '.' && *p != CR && *p != LF) {
            p++;
        }

        n++;

        if (n > 3) {
            ca_log_err(0, "invalid version format: \"%s\"", response);
            return 0;
        }

        if (*p == '\0' || *p == CR || *p == LF) {
            break;
        }

        m = ca_hextoi(s, p - s);
        if (m == CA_ERROR) {
            ca_log_err(0, "invalid version \"%s\"", response);
            return 0;

        } else if (m > 255) {
            ca_log_err(0, "subversion must less than 256: \"%s\"", response);
            return 0;
        }

        version = (version << 8) | m;
    }

    if (version == 0) {
        ca_log_notice(0, "version is 0: \"%s\"", response);
        return 0;
    }

    if (version == CA_VERSION_HEX) {
        ca_log_debug(0, "clagent do not need update, version: ", CA_VERSION);
        return 0;
    }

    ca_new_version = version;

    return 1;
}


static pid_t
ca_update(char *update_exe)
{
    ca_exec_ctx_t   ctx;
    u_char          buf[CA_INT64_LEN + 1];
    char           *argv[3];
    ca_int_t        pid;

    ca_slprintf(buf, buf + CA_INT64_LEN, "%ud%Z", ca_new_version);

    argv[0] = update_exe;
    argv[1] = (char *) buf;
    argv[2] = NULL;

    ca_memzero(&ctx, sizeof(ca_exec_ctx_t));

    ctx.path = update_exe;
    ctx.name = "clagent update process";
    ctx.argv = argv;
    ctx.envp = environ;

    pid = ca_execute(&ctx);

    if (pid == CA_INVALID_PID) {
        ca_log_alert(0, "ca_execute \"%s\", \"%s\" failed",
                     update_exe, buf);
    }

    return pid;
}
