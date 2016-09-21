#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "clagent.h"


/* To change the process title in Linux and Solaris we have to
   set argv[1] to NULL and to copy the title to the same place
   where the argv[0] points to. Howerver, argv[0] may be too 
   small to hold a new title. Fortunately, Linux and Solaris 
   store argv[] and environ[] one after another. So we should
   ensure that is the continuous memory and then we allocate
   the new memory for environ[] and copy it. After this we could
   use the memory starting from argv[0] for our process title.

   The Solaris's standard /bin/ps does not show the changed process
   title. You have to use "/usr/ucb/ps -w" instead. Besides, the 
   USB ps does not show a new title if its length less than the 
   origin command line length. To avoid it we append to a new title
   the origin command line in the parenthesis. */


static char  *arg_start;
static char  *arg_end;
static char  *env_start;
static char  *env_end;


void
ca_argv_free(u_char **daemon_argv)
{
    int  i = 0;

    for (i = 0; daemon_argv[i]; ++i) {
        ca_free(daemon_argv[i]);
    }

    ca_free(daemon_argv);
}


u_char **
ca_argv_dup(int argc, char *argv[])
{
    u_char  **saved_argv;

    arg_start = argv[0];
    arg_end = argv[argc - 1] + strlen(argv[argc - 1]) + 1;
    env_start = environ[0];

    saved_argv = ca_alloc((argc + 1) * sizeof(u_char *));
    if (saved_argv == NULL) {
        return NULL;
    }
    saved_argv[argc] = NULL;

    while (--argc >= 0) {
        saved_argv[argc] = (u_char *) ca_strdup(argv[argc]);
        if (saved_argv[argc] == NULL) {
            ca_argv_free(saved_argv);
            return NULL;
        }
    }

    return saved_argv;
}


void
ca_set_title(const char* fmt, ...)
{
    u_char    title[128];
    int       i, tlen;
    va_list   ap;
    u_char   *p;

    va_start(ap, fmt);
    p = ca_vsnprintf((u_char *) title, sizeof(title) - 1, fmt, ap);
    va_end(ap);
    *p = '\0';

    tlen = ca_strlen(title) + 1;

    if (arg_end - arg_start < tlen && env_start == arg_end) {
        env_end = env_start;
        for (i = 0; environ[i]; i++) {
            env_end = environ[i] + ca_strlen(environ[i]) + 1;
            environ[i] = ca_strdup(environ[i]);
        }
        arg_end = env_end;
    }

    i = arg_end - arg_start;
    p = ca_cpystrn((u_char *) arg_start, (u_char *) title, i);

    if (arg_end > (char *) p) {
        ca_memset(p, CA_SETPROCTITLE_PAD, arg_end - (char *) p);
    }
}


void
ca_title_free(void)
{
    int  i;

    if (env_end) {
        for (i = 0; environ[i]; i++) {
            free(environ[i]);
        }
    }
}


void
redirect_std()
{
    int  fd;

    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }
}


void
ca_daemonize(int nochdir, int noclose)
{
    if (fork() != 0) {
        exit(0);
    }

    if (!nochdir)  {
        if (chdir("/") != 0) {
            ca_log_stderr(errno, "chdir() to \"/\" failed");
            exit(1);
        }
    }

    if (!noclose) {
        redirect_std();
    }

    setsid();
}


static ca_int_t
pid_file_lock(int fd, int enable)
{
    struct flock  f;

    ca_memzero(&f, sizeof(f));

    f.l_type = enable ? F_WRLCK : F_UNLCK;
    f.l_whence = SEEK_SET;
    f.l_start = 0;
    f.l_len = 0;

    if (fcntl(fd, F_SETLKW, &f) < 0) {
        if (enable && errno == EBADF) {
            f.l_type = F_RDLCK;
            if (fcntl(fd, F_SETLKW, &f) >= 0) {
                return CA_OK;
            }
        }

        ca_log_emerg(errno, "fcntl() F_SETLKW failed");
        return CA_ERROR;
    }

    return CA_OK;
}


ca_int_t
pid_file_create(char *pid_file)
{
    int       fd, len, saved_errno;
    ca_int_t  ret, locked;
    u_char    buf[CA_INT64_LEN + 2];
    mode_t    mode;

    ret = CA_ERROR;
    mode = umask(022);

    if ((fd = open(pid_file, O_CREAT|O_RDWR|O_EXCL, 0644)) < 0) {
        goto finish;
    }

    if ((locked = pid_file_lock(fd, 1)) == CA_ERROR) {
        saved_errno = errno;
        unlink(pid_file);
        errno = saved_errno;
        goto finish;
    }

    ca_snprintf(buf, sizeof(buf), "%ud%Z", getpid());
    len = ca_strlen(buf);

    if (write(fd, buf, len) != len) {
        saved_errno = errno;
        unlink(pid_file);
        errno = saved_errno;
        goto finish;
    }

    ret = CA_OK;

finish:
    if (fd >= 0) {
        saved_errno = errno;
        if (locked == CA_OK) {
            pid_file_lock(fd, 0);
        }
        close(fd);
        errno = saved_errno;
    }

    umask(mode);
    return ret;
}


ca_int_t
pid_file_running(char *pid_file)
{
    int       fd, len, saved_errno;
    u_char    buf[CA_INT64_LEN + 2];
    ca_int_t  pid, locked;

    pid = CA_ERROR;

    if ((fd = open(pid_file, O_RDONLY, 0644)) < 0) {
        pid = CA_OK;
        goto finish;
    }

    if ((locked = pid_file_lock(fd, 1)) == CA_ERROR) {
        goto finish;
    }

    if ((len = read(fd, buf, sizeof(buf) - 1)) < 0) {
        goto finish;
    }

    while (len-- && (buf[len] == CR || buf[len] == LF)) { /* void */ };

    pid = ca_atoi(buf, ++len);
    if (pid == CA_ERROR) {
        ca_log_err(0, "PID file [%s] corrupted, removing\n", pid_file);
        unlink(pid_file);
        errno = EINVAL;
        goto finish;
    }

    if (kill((pid_t)pid, 0) != 0 && errno != EPERM) {
        saved_errno = errno;
        unlink(pid_file);
        errno = saved_errno;
        pid = 0;
        goto finish;
    }

finish:

    if (fd >= 0) {
        saved_errno = errno;
        if (locked == CA_OK) {
            pid_file_lock(fd, 0);
        }
        close(fd);
        errno = saved_errno;
    }

    return pid;
}
