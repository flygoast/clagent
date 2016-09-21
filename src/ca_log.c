#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include "clagent.h"


static ca_logger_t  ca_logger = {
    NULL,
    CA_LOG_NOTICE,
    STDERR_FILENO,
    0,
};


static ca_str_t err_levels[] = {
    ca_string("emerg"),
    ca_string("alert"),
    ca_string("crit"),
    ca_string("error"),
    ca_string("warn"),
    ca_string("notice"),
    ca_string("info"),
    ca_string("debug"),
    ca_null_string
};


ca_int_t
ca_log_init(int level, char *name)
{
    ca_logger_t  *l = &ca_logger;

    l->level = CA_MAX(CA_LOG_EMERG, CA_MIN(level, CA_LOG_DEBUG));
    l->name = name;
    if (name == NULL || !strlen(name)) {
        l->fd = STDERR_FILENO;

    } else {
        l->fd = open(name, O_WRONLY|O_APPEND|O_CREAT, 0644);
        if (l->fd < 0) {
            ca_log_stderr(errno, "open log file \"%s\" failed", name);
            l->fd = STDERR_FILENO;
            return CA_ERROR;
        }
    }

    return CA_OK;
}


void
ca_log_deinit(void)
{
    ca_logger_t  *l = &ca_logger;

    if (l->fd < 0 || l->fd == STDERR_FILENO) {
        return;
    }

    close(l->fd);
}


void
ca_log_reopen(void)
{
    ca_logger_t  *l = &ca_logger;

    if (l->fd != STDERR_FILENO) {
        close(l->fd);
        l->fd = open(l->name, O_WRONLY|O_APPEND|O_CREAT, 0644);
        if (l->fd < 0) {
            ca_log_stderr(errno, "reopen log file \"%s\" failed, ignored",
                          l->name);
        }
    }
}


void
ca_log_level_up(void)
{
    ca_logger_t  *l = &ca_logger;

    if (l->level < CA_LOG_DEBUG) {
        l->level++;
    }
}


void
ca_log_level_down(void)
{
    ca_logger_t  *l = &ca_logger;

    if (l->level > CA_LOG_EMERG) {
        l->level--;
    }
}


void
ca_log_level_set(int level)
{
    ca_logger_t *l = &ca_logger;

    l->level = CA_MAX(CA_LOG_EMERG, CA_MIN(level, CA_LOG_DEBUG));
}


int
ca_log_get_level(char *log_level)
{
    ca_str_t  *str;
    int        i, len;

    len = strlen(log_level);

    for (str = err_levels, i = 0; str->len; str++, i++) {
        if (str->len != len) {
            continue;
        }

        if (ca_strncasecmp(str->data, (u_char *)log_level, len) != 0) {
            continue;
        }

        return i;
    }

    return CA_ERROR;
}


void
ca_log_core(int level, int err, const char *fmt, ...)
{
    ca_logger_t    *l = &ca_logger;
    u_char          errstr[CA_LOG_MAX_LEN];
    u_char         *p, *last;
    char           *proc_name;
    va_list         args;
    struct tm       local;
    time_t          t;
    ssize_t         n;

    if (l->fd < 0 || level > l->level) {
        return;
    }

    switch (ca_process) {
    case CA_PROCESS_MASTER:
        proc_name = "master";
        break;

    case CA_PROCESS_UPDATE:
        proc_name = "update";
        break;

    case CA_PROCESS_ACQ:
        proc_name = "acq";
        break;
    case CA_PROCESS_WORKER:
        proc_name = "worker";
        break;
    default:
        proc_name = "unknown";
        break;

    }

    last = errstr + CA_LOG_MAX_LEN;
    p = errstr;

    *p++ = '[';

    t = time(NULL);
    localtime_r(&t, &local);
    asctime_r(&local, (char *)p);
    p += 24;

    p = ca_slprintf(p, last, "] [%s] [%d] [%V] ",
                    proc_name, gettid(), &err_levels[level]);

    va_start(args, fmt);
    p = ca_vslprintf(p, last, fmt, args);
    va_end(args);

    if (err) {
        p = ca_log_errno(p, last, err);
    }

    if (p > last - 1) {
        p = last - 1;
    }

    *p++ = LF;

    n = write(l->fd, errstr, p - errstr);
    if (n < 0) {
        l->nerror++;
    }
}


u_char *
ca_log_errno(u_char *buf, u_char *last, int err)
{
    if (buf > last - 50) {

        /* leave a space for an error code */

        buf = last - 50;
        *buf++ = '.';
        *buf++ = '.';
        *buf++ = '.';
    }

    buf = ca_slprintf(buf, last, " (%d: %s", err, strerror(err));

    if (buf < last) {
        *buf++ = ')';
    }

    return buf;
}


void
ca_log_stderr(int err, const char *fmt, ...)
{
    ca_logger_t   *l = &ca_logger;
    u_char         errstr[CA_LOG_MAX_LEN];
    u_char        *p, *last;
    va_list        args;
    ssize_t        n;

    last = errstr + CA_LOG_MAX_LEN;
    p = errstr + 9;

    ca_memcpy(errstr, "clagent: ", 9);

    va_start(args, fmt);
    p = ca_vslprintf(p, last, fmt, args);
    va_end(args);

    if (err) {
        p = ca_log_errno(p, last, err);
    }

    if (p > last - 1) {
        p = last - 1;
    }

    *p++ = LF;

    n = write(STDERR_FILENO, errstr, p - errstr);
    if (n < 0) {
        l->nerror++;
    }
}
