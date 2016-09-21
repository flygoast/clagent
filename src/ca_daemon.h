#ifndef __CA_DAEMON_H_INCLUDED__
#define __CA_DAEMON_H_INCLUDED__


#include <stdarg.h>
#include <unistd.h>


#define CA_SETPROCTITLE_PAD     '\0'


extern char **environ;


u_char **ca_argv_dup(int argc, char *argv[]);
void ca_argv_free(u_char **daemon_argv);
void ca_set_title(const char* fmt, ...);
void ca_title_free(void);

void ca_redirect_std(void);
void ca_daemonize(int nochdir, int noclose);

ca_int_t pid_file_running(char *pid_file);
ca_int_t pid_file_create(char *pid_file);


#endif /* __CA_DAEMON_H_INCLUDED__ */
