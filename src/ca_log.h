#ifndef __CA_LOG_H_INCLUDED__
#define __CA_LOG_H_INCLUDED__


typedef struct {
    char    *name;      /* log file name */
    int      level;     /* log level */
    int      fd;        /* log file descriptor */
    int      nerror;    /* # log error */
} ca_logger_t;


#define CA_LOG_EMERG       0  /* system in unusable */
#define CA_LOG_ALERT       1  /* action must be taken immediately */
#define CA_LOG_CRIT        2  /* critical conditions */
#define CA_LOG_ERR         3  /* error condition */
#define CA_LOG_WARN        4  /* warning conditions */
#define CA_LOG_NOTICE      5  /* normal buf significant condition (default) */
#define CA_LOG_INFO        6  /* informational */
#define CA_LOG_DEBUG       7  /* debug messages */

#define CA_LOG_MAX_LEN     2048    /* max length of log message */

#define ca_log_emerg(...) do {                     \
    ca_log_core(CA_LOG_EMERG, __VA_ARGS__);        \
} while (0)

#define ca_log_alert(...) do {                     \
    ca_log_core(CA_LOG_ALERT, __VA_ARGS__);        \
} while (0)

#define ca_log_crit(...) do {                      \
    ca_log_core(CA_LOG_CRIT, __VA_ARGS__);         \
} while (0)

#define ca_log_err(...) do {                       \
    ca_log_core(CA_LOG_ERR, __VA_ARGS__);          \
} while (0)

#define ca_log_warn(...) do {                      \
    ca_log_core(CA_LOG_WARN, __VA_ARGS__);         \
} while (0)

#define ca_log_notice(...) do {                    \
    ca_log_core(CA_LOG_INFO,  __VA_ARGS__);        \
} while (0)

#define ca_log_info(...) do {                      \
    ca_log_core(CA_LOG_INFO,  __VA_ARGS__);        \
} while (0)

#define ca_log_debug(...) do {                     \
    ca_log_core(CA_LOG_DEBUG,  __VA_ARGS__);       \
} while (0)


ca_int_t ca_log_init(int level, char *name);
void ca_log_deinit(void);
void ca_log_reopen(void);
void ca_log_level_up(void);
void ca_log_level_down(void);
void ca_log_level_set(int level);
int ca_log_get_level(char *log_level);
void ca_log_core(int level, int err, const char *fmt, ...);
void ca_log_stderr(int err, const char *fmt, ...);
u_char *ca_log_errno(u_char *buf, u_char *last, int err);


#endif /* __CA_LOG_H_INCLUDED__ */
