#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "clagent.h"


typedef void (*child_proc_pt)(void *);


typedef struct {
    int      signo;
    char    *signame;
    int      flags;
    void   (*handler)(int signo);
} ca_signal_t;


static int ca_signal_init(void);
static void ca_signal_handler(int signo);
static void ca_process_get_status(void);
static pid_t ca_spawn_process(child_proc_pt proc, void *data, const char *name,
    int respawn);
static void ca_signal_child_processes(int signo);
static char *ca_conf_acq_block(ca_conf_t *cf, ca_command_t *cmd, void *conf);
static char *ca_conf_acq_item(ca_conf_t *cf, ca_command_t *dummy, void *conf);
static char *ca_conf_server(ca_conf_t *cf, ca_command_t *cmd, void *conf);
static char *ca_conf_log(ca_conf_t *cf, ca_command_t *cmd, void *conf);


static ca_signal_t ca_signals[] = {
    { SIGUSR1, "SIGUSR1", 0, ca_signal_handler },
    { SIGUSR2, "SIGUSR2", 0, ca_signal_handler },
    { SIGHUP,  "SIGHUP",  0, ca_signal_handler },
    { SIGINT,  "SIGINT",  0, ca_signal_handler },
    { SIGQUIT, "SIGQUIT", 0, ca_signal_handler },
    { SIGTERM, "SIGTERM", 0, ca_signal_handler },
    { SIGCHLD, "SIGCHLD", 0, ca_signal_handler },
    { SIGALRM, "SIGALRM", 0, ca_signal_handler },
    { SIGPIPE, "SIGPIPE", 0, SIG_IGN },
    { 0,       NULL,      0, NULL}
};


typedef struct {
    pid_t           pid;
    int             status;
    child_proc_pt   proc;
    void           *data;
    const char     *name;

    unsigned respawn:1;
    unsigned just_spawn:1;
    unsigned detached:1;
    unsigned exiting:1;
    unsigned exited:1;
} ca_process_t;


int                     ca_process;
int                     ca_process_slot;
int                     ca_last_process;
ca_process_t            ca_processes[CA_MAX_PROCESSES];
static ca_str_t         conf_file;
static ca_uint_t        ca_action;
static int              ca_argc;
static u_char         **ca_argv;
sig_atomic_t            ca_reap;
sig_atomic_t            ca_quit;
sig_atomic_t            ca_terminate;
sig_atomic_t            ca_sigalrm;
sig_atomic_t            ca_change_binary;
pid_t                   ca_new_binary;
static ca_conf_ctx_t    conf_ctx;


static ca_command_t  ca_conf_commands[] = {

    { ca_string("daemon"),
      CA_CONF_FLAG,
      ca_conf_set_flag_slot,
      0,
      offsetof(ca_conf_ctx_t, daemon),
      NULL },

    { ca_string("log"),
      CA_CONF_TAKE12,
      ca_conf_log,
      0,
      0,
      NULL },

    { ca_string("check_interval"),
      CA_CONF_TAKE1,
      ca_conf_set_num_slot,
      0,
      offsetof(ca_conf_ctx_t, check_interval),
      NULL },

    { ca_string("connect_timeout"),
      CA_CONF_TAKE1,
      ca_conf_set_num_slot,
      0,
      offsetof(ca_conf_ctx_t, connect_timeout),
      NULL },

    { ca_string("send_timeout"),
      CA_CONF_TAKE1,
      ca_conf_set_num_slot,
      0,
      offsetof(ca_conf_ctx_t, send_timeout),
      NULL },

    { ca_string("recv_timeout"),
      CA_CONF_TAKE1,
      ca_conf_set_num_slot,
      0,
      offsetof(ca_conf_ctx_t, recv_timeout),
      NULL },

    { ca_string("max_free_object"),
      CA_CONF_TAKE1,
      ca_conf_set_num_slot,
      0,
      offsetof(ca_conf_ctx_t, max_nfree),
      NULL },

    { ca_string("pid"),
      CA_CONF_TAKE1,
      ca_conf_set_str_slot,
      0,
      offsetof(ca_conf_ctx_t, pid),
      NULL },

    { ca_string("update_url"),
      CA_CONF_TAKE1,
      ca_conf_set_str_slot,
      0,
      offsetof(ca_conf_ctx_t, update_url),
      NULL },

    { ca_string("update_exe"),
      CA_CONF_TAKE1,
      ca_conf_set_str_slot,
      0,
      offsetof(ca_conf_ctx_t, update_exe),
      NULL },

    { ca_string("identify"),
      CA_CONF_TAKE1,
      ca_conf_set_str_slot,
      0,
      offsetof(ca_conf_ctx_t, identify),
      NULL },

    { ca_string("acq"),
      CA_CONF_BLOCK|CA_CONF_NOARGS,
      ca_conf_acq_block,
      0,
      0,
      NULL },

    { ca_string("server"),
      CA_CONF_TAKE2,
      ca_conf_server,
      0,
      0,
      NULL },

    ca_null_command
};


static char *
ca_conf_log(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    ca_conf_ctx_t   *ctx = conf;
    ca_str_t        *value;
    int              level;

    if (ctx->log_file.len != 0) {
        return "is duplicate";
    }

    value = cf->args->elem;

    ctx->log_file = value[1];

    if (cf->args->nelem > 2) {
        ctx->log_level_str = value[2];
        level = ca_log_get_level((char *) value[2].data);
        if (level == CA_ERROR) {
            ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                              "invalid log level \"%V\" in directive \"log\"",
                              &value[2]);
            return CA_CONF_ERROR;
        }

    } else {

        ca_str_set(&ctx->log_level_str, "notice");
        level = CA_LOG_NOTICE;
    }

    ctx->log_level = level;

    return CA_CONF_OK;
}


static char *
ca_conf_server(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    ca_conf_ctx_t   *ctx = conf;
    ca_str_t        *value;
    ca_server_t     *server;
    u_char          *p;
    struct hostent  *he;
    ca_int_t         i, port;
    in_port_t        in_port;
    struct in_addr   in_addr;

    if (ctx->servers == NULL) {
        ctx->servers = ca_array_create(4, sizeof(ca_server_t));
        if (ctx->servers == NULL) {
            return CA_CONF_ERROR;
        }
    }

    value = cf->args->elem;

    port = ca_atoi(value[2].data, value[2].len);
    if (port == CA_ERROR) {
        ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                          "invalid port in \"server\" directive");
        return CA_CONF_ERROR;
    }

    if (port < 0 || port > 65535) {
        ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                          "invalid port in \"server\" directive, "
                          "port must between 0 and 65535");
        return CA_CONF_ERROR;
    }

    in_port = htons(port);

    if (inet_aton((const char *) value[1].data, &in_addr) == 0) {

        he = gethostbyname((const char *) value[1].data);
        if (he == NULL || he->h_addr_list[0] == NULL) {
            ca_conf_log_error(CA_LOG_EMERG, cf, 0, "host \"%V\" not found",
                              &value[1]);
            return CA_CONF_ERROR;
        }

        for (i = 0; he->h_addr_list[i] != NULL; i++) {
            server = ca_array_push(ctx->servers);
            if (server == NULL) {
                return CA_CONF_ERROR;
            }

            server->sin.sin_family = AF_INET;
            server->sin.sin_addr.s_addr = *(in_addr_t *) (he->h_addr_list[i]);
            server->sin.sin_port = in_port;

            p = (u_char *) &server->sin.sin_addr;

            server->addr_str = ca_calloc(CA_SOCKADDR_STRLEN + 1, 1);
            if (server->addr_str == NULL) {
                return CA_CONF_ERROR;
            }

            ca_snprintf(server->addr_str, CA_SOCKADDR_STRLEN + 1,
                        "%ud.%ud.%ud.%ud:%V%Z",
                        p[0], p[1], p[2], p[3], &value[2]);

            server->host_str = value[1];
            server->port_str = value[2];
        }

    } else {

        server = ca_array_push(ctx->servers);
        if (server == NULL) {
            return CA_CONF_ERROR;
        }

        server->sin.sin_family = AF_INET;
        server->sin.sin_addr = in_addr;
        server->sin.sin_port = in_port;

        server->addr_str = ca_calloc(CA_SOCKADDR_STRLEN + 1, 1);
        if (server->addr_str == NULL) {
            return CA_CONF_ERROR;
        }

        ca_snprintf(server->addr_str, CA_SOCKADDR_STRLEN + 1,
                    "%V:%V%Z", &value[1], &value[2]);

        server->host_str = value[1];
        server->port_str = value[2];
    }

    return CA_CONF_OK;
}


static char *
ca_conf_acq_item(ca_conf_t *cf, ca_command_t *dummy, void *conf)
{
    ca_conf_ctx_t          *ctx = conf;
    ca_str_t               *value;
    ca_int_t                id, i;
    ca_uint_t               freq;
    ca_int_t                type;
    ca_acq_t               *item;
    ca_acq_item_handler_t  *handler;

    if (cf->args->nelem != 4) {
        ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                          "invalid number of acq parameters");
        return CA_CONF_ERROR;
    }

    value = cf->args->elem;
    handler = NULL;

    for (i = 0; ca_acq_item_handlers[i].name.len != 0; i++) {
        if (value[0].len != ca_acq_item_handlers[i].name.len
            || ca_strcasecmp(ca_acq_item_handlers[i].name.data, value[0].data)
               != 0)
        {
            continue;
        }

        handler = &ca_acq_item_handlers[i];
        ca_acq_item_handlers[i].exist = 1;
        break;
    }

    if (handler == NULL) {
        ca_conf_log_error(CA_LOG_EMERG, cf, 0, "invalid acq item \"%V\"",
                          &value[0]);
        return CA_CONF_ERROR;
    }

    id = ca_atoi(value[1].data, value[1].len);
    if (id == CA_ERROR) {
        ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                          "invalid \"id\" field of acq parameters");
        return CA_CONF_ERROR;
    }

    freq = ca_parse_time(&value[2], 1);
    if (freq == CA_ERROR) {
        ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                          "invalid \"freq\" field of acq parameters");
        return CA_CONF_ERROR;
    }

    type = ca_atoi(value[3].data, value[3].len);
    if (type == CA_ERROR) {
        ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                          "invalid \"type\" field of acq parameters");
        return CA_CONF_ERROR;
    }

    item = ca_array_push(ctx->acq_items);
    if (item == NULL) {
        return CA_CONF_ERROR;
    }

    item->item = value[0];
    item->id = id;
    item->id_len = value[1].len;
    item->freq = freq;
    item->type = type;
    item->handler = handler->item_handler;
    item->accessed = 0;

    return CA_CONF_OK;
}


static char *
ca_conf_acq_block(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char           *rv;
    ca_conf_ctx_t  *ctx;
    ca_conf_t       save;

    ctx = conf;

    if (ctx->acq_items != NULL) {
        return "is duplicate";
    }

    ctx->acq_items = ca_array_create(16, sizeof(ca_acq_t));
    if (ctx->acq_items == NULL) {
        return CA_CONF_ERROR;
    }

    save.handler = cf->handler;
    save.handler_conf = conf;

    cf->handler = ca_conf_acq_item;
    cf->handler_conf = conf;

    rv = ca_conf_parse(cf, NULL);

    cf->handler = save.handler;
    cf->handler_conf = save.handler_conf;

    return rv;
}


static void
ca_execute_proc(void *data)
{
    ca_exec_ctx_t  *ctx = data;

    if (execve(ctx->path, ctx->argv, ctx->envp) == -1) {
        ca_log_alert(errno, "execve() failed while executing %s \"%s\"",
                     ctx->name, ctx->path);
    }

    exit(1);
}


pid_t
ca_execute(ca_exec_ctx_t *ctx)
{
    return ca_spawn_process(ca_execute_proc, ctx, ctx->name,
                            CA_PROCESS_DETACHED);
}


static pid_t
ca_exec_new_binary(char **argv)
{
    pid_t          pid;
    ca_exec_ctx_t  ctx;

    ca_memzero(&ctx, sizeof(ca_exec_ctx_t));

    ctx.path = argv[0];
    ctx.name = "new binary process";
    ctx.argv = argv;
    ctx.envp = environ;

    if (rename((char *) conf_ctx.pid.data,
               (char *) conf_ctx.oldpid.data) != CA_OK)
    {
        ca_log_alert(errno,
                     "rename %s to %s failed "
                     "before executing new binary process \"%s\"",
                     conf_ctx.pid.data, conf_ctx.oldpid.data, argv[0]);

        return CA_INVALID_PID;
    }

    pid = ca_execute(&ctx);

    if (pid == CA_INVALID_PID) {
        if (rename((char *) conf_ctx.oldpid.data,
                   (char *) conf_ctx.pid.data) != CA_OK)
        {
            ca_log_alert(errno,
                         "rename %s to %s failed "
                         "before executing new binary process \"%s\"",
                         conf_ctx.oldpid.data, conf_ctx.pid.data, argv[0]);
        }
    }

    return pid;
}


static void
ca_init_processes(void)
{
    int  i = 0;

    for (i = 0; i < CA_MAX_PROCESSES; i++) {
        ca_processes[i].pid = -1;
    }
}


static pid_t
ca_spawn_process(child_proc_pt proc, void *data, const char *name, int respawn)
{
    int    s;
    pid_t  pid;

    if (respawn >= 0) {
        s = respawn;

    } else {

        for (s = 0; s < ca_last_process; s++) {
            if (ca_processes[s].pid == -1) {
                break;
            }
        }

        if (s == CA_MAX_PROCESSES) {
            return -1;
        }
    }

    ca_process_slot = s;

    pid = fork();
    switch (pid) {
    case -1:
        return -1;
    case 0:
        ca_set_title(name);
        proc(data);
        exit(0);
    default:
        break;
    }

    ca_processes[s].pid = pid;
    ca_processes[s].exited = 0;

    if (respawn >= 0) {
        return pid;
    }

    ca_processes[s].proc = proc;
    ca_processes[s].data = data;
    ca_processes[s].name = name;
    ca_processes[s].exiting = 0;

    switch (respawn) {
    case CA_PROCESS_NORESPAWN:
        ca_processes[s].respawn = 0;
        ca_processes[s].just_spawn = 0;
        ca_processes[s].detached = 0;
        break;

    case CA_PROCESS_JUST_SPAWN:
        ca_processes[s].respawn = 0;
        ca_processes[s].just_spawn = 1;
        ca_processes[s].detached = 0;
        break;

    case CA_PROCESS_RESPAWN:
        ca_processes[s].respawn = 1;
        ca_processes[s].just_spawn = 0;
        ca_processes[s].detached = 0;
        break;

    case CA_PROCESS_JUST_RESPAWN:
        ca_processes[s].respawn = 1;
        ca_processes[s].just_spawn = 1;
        ca_processes[s].detached = 0;
        break;

    case CA_PROCESS_DETACHED:
        ca_processes[s].respawn = 0;
        ca_processes[s].just_spawn = 0;
        ca_processes[s].detached = 1;
        break;
    }

    if (s == ca_last_process) {
        ca_last_process++;
    }

    return pid;
}


static int
ca_reap_children()
{
    int  i;
    int  live = 0;

    for (i = 0; i < ca_last_process; i++) {
        if (ca_processes[i].pid == -1) {
            continue;
        }

        if (ca_processes[i].exited) {
            if (ca_processes[i].respawn && !ca_processes[i].exiting
                && !ca_quit)
            {
                if (ca_spawn_process(ca_processes[i].proc,
                                     ca_processes[i].data,
                                     ca_processes[i].name, i) == -1)
                {
                    continue;
                }
                live = 1;
            }

        } else {
            live = 1;
        }
    }
    return live;
}


static void
ca_create_processes(child_proc_pt func, void *data, char *proc_name, int n,
    int type)
{
    int  i;

    for (i = 0; i < n; i++) {
        if (ca_spawn_process(func, data, proc_name, type) < 0) {
            continue;
        }
    }
}


static void
print_info(void)
{
    printf("[%s]: Qihoo Cloud Agent.\n"
           "Version: %s\n"
           "Copyright (c) Qihoo.corp, 2013\n"
           "Compiled at %s %s\n", PROG_NAME, CA_VERSION, __DATE__, __TIME__);
}


static void
usage(int status)
{
    if (status != EXIT_SUCCESS) {
        fprintf(stderr, "Try `%s --help' for more information.\n", PROG_NAME);

    } else {
        printf("Usage: clagent [-hv] [-c filename] [start|stop]" CA_LINEFEED
               CA_LINEFEED
               "Options:" CA_LINEFEED
               "  -h, --help            : this help" CA_LINEFEED
               "  -v, --version         : show version and exit" CA_LINEFEED
               "  -c, --conf=filename   : set configuration file (default: "
                                          CA_CONF_PATH ")" CA_LINEFEED
               CA_LINEFEED);
    }
}


static struct option const long_options[] = {
    {"conf",    required_argument, NULL, 'c'},
    {"help",    no_argument,       NULL, 'h'},
    {"version", no_argument,       NULL, 'v'},
    {NULL, 0, NULL, 0}
};


static void
ca_parse_options(int argc, char **argv)
{
    int  c;

    while ((c = getopt_long(argc, argv, "c:hv", long_options, NULL)) != -1) {
        switch (c) {
        case 'c':
            conf_file.data = (u_char *)optarg;
            conf_file.len = ca_strlen(optarg);
            break;

        case 'h':
            usage(EXIT_SUCCESS);
            exit(EXIT_SUCCESS);

        case 'v':
            print_info();
            exit(EXIT_SUCCESS);

        default:
            usage(EXIT_FAILURE);
            exit(1);
        }
    }

    if (optind + 1 == argc) {
        if (ca_strcasecmp((u_char *) argv[optind], (u_char *) "stop") == 0) {
            ca_action = CA_STOP;

        } else if (ca_strcasecmp((u_char *) argv[optind], (u_char *) "start")
                   == 0)
        {
            ca_action = CA_START;

        } else {
            usage(EXIT_FAILURE);
            exit(1);
        }

    } else if (optind == argc) {
        ca_action = CA_START;

    } else {
        usage(EXIT_FAILURE);
        exit(1);
    }
}


static void
ca_master_process_cycle(void)
{
    int                i, live, delay;
    sigset_t           set;
    size_t             size;
    char              *title;
    u_char            *p;
    struct itimerval   itv;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        ca_log_alert(errno, "sigprocmask() failed");
    }

    sigemptyset(&set);

    size = sizeof(MASTER_PROCESS_NAME);

    for (i = 0; i < ca_argc; i++) {
        size += ca_strlen(ca_argv[i]) + 1;
    }

    title = ca_calloc(size, 1);
    if (title == NULL) {
        ca_log_emerg(0, "out of memory");
        return;
    }

    p = ca_cpymem(title, MASTER_PROCESS_NAME, sizeof(MASTER_PROCESS_NAME) - 1);
    for (i = 0; i < ca_argc; i++) {
        *p++ = ' ';
        p = ca_cpystrn(p, ca_argv[i], size);
    }

    ca_set_title(title);

    ca_free(title);

    ca_create_processes(ca_worker_process_cycle, (void *) &conf_ctx,
                        WORKER_PROCESS_NAME, 1, CA_PROCESS_RESPAWN);

    ca_create_processes(ca_update_process_cycle, (void *) &conf_ctx,
                        UPDATE_PROCESS_NAME, 1, CA_PROCESS_RESPAWN);

    ca_create_processes(ca_acq_process_cycle, (void *) &conf_ctx,
                        ACQ_PROCESS_NAME, 1, CA_PROCESS_RESPAWN);


    ca_process = CA_PROCESS_MASTER;

    delay = 0;
    live = 1;

    for ( ;; ) {
        if (delay) {
            if (ca_sigalrm) {
                delay *= 2;
                ca_sigalrm = 0;
            }

            ca_log_debug(0, "termination cycle: %d", delay);

            itv.it_interval.tv_sec = 0;
            itv.it_interval.tv_usec = 0;
            itv.it_value.tv_sec = delay / 1000;
            itv.it_value.tv_usec = (delay % 1000) * 1000;

            if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
                ca_log_alert(errno, "setitimer() failed");
            }
        }

        ca_log_debug(0, "sigsuspend");

        sigsuspend(&set);

        if (ca_reap) {
            ca_reap = 0;
            ca_log_debug(0, "reap children");
            live = ca_reap_children();
        }

        if (!live && (ca_quit || ca_terminate)) {
            break;
        }

        if (ca_terminate) {
            if (delay == 0) {
                delay = 50;
            }

            if (delay > 1000) {
                ca_signal_child_processes(SIGKILL);

            } else {
                ca_signal_child_processes(SIGTERM);
            }

            continue;
        }

        if (ca_quit) {
            ca_signal_child_processes(SIGQUIT);
            continue;
        }

        if (ca_change_binary) {
            ca_change_binary = 0;
            ca_log_notice(0, "changing binary");

            ca_new_binary = ca_exec_new_binary((char **) ca_argv);
            if (ca_new_binary != CA_INVALID_PID) {
                kill(getpid(), SIGQUIT);
            }
        }
    }

    ca_log_debug(0, "exit");
}


int
main(int argc, char **argv)
{
    int            ret;
    ca_conf_t      conf;
    ca_int_t       i, pid;
    ca_server_t   *server;

    ca_parse_options(argc, argv);

    if (conf_file.len == 0) {
        ca_str_set(&conf_file, CA_CONF_PATH);
    }

    conf_ctx.daemon = CA_CONF_UNSET;
    conf_ctx.check_interval = CA_CONF_UNSET_UINT;
    conf_ctx.connect_timeout = CA_CONF_UNSET_UINT;
    conf_ctx.send_timeout = CA_CONF_UNSET_UINT;
    conf_ctx.recv_timeout = CA_CONF_UNSET_UINT;
    conf_ctx.max_nfree = CA_CONF_UNSET_UINT;
    conf_ctx.log_level = CA_CONF_UNSET;

    ca_str_null(&conf_ctx.pid);
    ca_str_null(&conf_ctx.update_url);
    ca_str_null(&conf_ctx.update_exe);
    ca_str_null(&conf_ctx.identify);
    ca_str_null(&conf_ctx.log_file);

    ca_memzero(&conf, sizeof(ca_conf_t));
    conf.ctx = &conf_ctx;
    conf.commands = ca_conf_commands;

    ret = 0;
    if (ca_conf_parse(&conf, &conf_file) != CA_CONF_OK) {
        ret = -1;
        goto out;
    }

    /*
     * check and initialize config
     */

    ca_conf_init_value(conf_ctx.daemon, 1);
    ca_conf_init_uint_value(conf_ctx.check_interval, 600);
    ca_conf_init_uint_value(conf_ctx.connect_timeout, 60);
    ca_conf_init_uint_value(conf_ctx.send_timeout, 60);
    ca_conf_init_uint_value(conf_ctx.recv_timeout, 60);
    ca_conf_init_uint_value(conf_ctx.max_nfree, 64);

    if (conf_ctx.log_file.len == 0) {
        ca_str_set(&conf_ctx.log_file, CA_LOG_PATH);
        ca_str_set(&conf_ctx.log_level_str, "notice");
        conf_ctx.log_level = CA_LOG_NOTICE;
    }

    if (conf_ctx.update_url.len == 0) {
        ca_str_set(&conf_ctx.update_url, CA_UPDATE_URL);
    }

    if (conf_ctx.update_exe.len == 0) {
        ca_str_set(&conf_ctx.update_exe, CA_UPDATE_EXE);
    }

    if (conf_ctx.identify.len == 0) {
        ca_log_emerg(0, "\"identify\" directive not found");
        ret = -1;
        goto out;
    }

    if (conf_ctx.acq_items == NULL || conf_ctx.acq_items->nelem == 0) {
        ca_log_emerg(0, "\"acq\" item not found");
        ret = -1;
        goto out;
    }

#if 0
    for (i = 0; ca_acq_item_handlers[i].name.len != 0; i++) {
        if (!ca_acq_item_handlers[i].exist) {
            ca_log_emerg(0, "acq item \"%V\" not specified",
                         &ca_acq_item_handlers[i].name);
            ret = -1;
            goto out;
        }
    }
#endif

    if (conf_ctx.servers == NULL || conf_ctx.servers->nelem == 0) {
        ca_log_emerg(0, "\"server\" directive not found");
        ret = -1;
        goto out;
    }

    if (conf_ctx.pid.len == 0) {
        ca_str_set(&conf_ctx.pid, CA_PID_PATH);
    }

    conf_ctx.oldpid.len = conf_ctx.pid.len + sizeof(CA_OLDPID_EXT);
    conf_ctx.oldpid.data = ca_alloc(conf_ctx.oldpid.len);
    if (conf_ctx.oldpid.data == NULL) {
        ret = -1;
        goto out;
    }
    ca_memcpy(ca_cpymem(conf_ctx.oldpid.data, conf_ctx.pid.data,
                        conf_ctx.pid.len),
              CA_OLDPID_EXT, sizeof(CA_OLDPID_EXT));

#if 0
    ca_log_stderr(0, "log: %V %V(%d)", &conf_ctx.log_file,
                  &conf_ctx.log_level_str, conf_ctx.log_level);
    ca_log_stderr(0, "pid: %V", &conf_ctx.pid);
    ca_log_stderr(0, "oldpid: %V", &conf_ctx.oldpid);
    ca_log_stderr(0, "update_url: %V", &conf_ctx.update_url);
    ca_log_stderr(0, "update_exe: %V", &conf_ctx.update_exe);
    ca_log_stderr(0, "identify: %V", &conf_ctx.identify);
    ca_log_stderr(0, "check_interval: %uL", conf_ctx.check_interval);
    ca_log_stderr(0, "max_free_object: %uL", conf_ctx.max_nfree);
    ca_log_stderr(0, "connect_timeout: %uL", conf_ctx.connect_timeout);
    ca_log_stderr(0, "send_timeout: %uL", conf_ctx.send_timeout);
    ca_log_stderr(0, "recv_timeout: %uL", conf_ctx.recv_timeout);

    {
        int           i;
        ca_acq_t     *item;
        ca_server_t  *server;

        for (i = 0; i < conf_ctx.servers->nelem; i++) {
            server = (ca_server_t *) conf_ctx.servers->elem + i;
            ca_log_stderr(0, "server: %s", server->addr_str);
        }

        ca_log_stderr(0, "acq {");
        for (i = 0; i < conf_ctx.acq_items->nelem; i++) {
            item = (ca_acq_t *) conf_ctx.acq_items->elem + i;
            ca_log_stderr(0, "    %V  %L  %L  %L",
                          &item->item,
                          item->freq,
                          item->id,
                          item->type);
        }
        ca_log_stderr(0, "}");
    }
#endif

    ca_argc = argc;
    ca_argv = ca_argv_dup(argc, argv);
    if (ca_argv == NULL) {
        ca_log_stderr(0, "duplicate argv failed");
        ret = -1;
        goto out;
    }

    pid = pid_file_running((char *) conf_ctx.pid.data);

    if (ca_action == CA_START) {
        if (pid == CA_ERROR) {
            ca_log_stderr(errno, "checking pid file \"%V\" failed",
                          &conf_ctx.pid);

            ret = -1;

        } else if (pid > 0) {
            ca_log_stderr(0, "clagent[%d] has been running", pid);
            ret = -1;

        } else {

            if (ca_log_init(conf_ctx.log_level, (char *) conf_ctx.log_file.data)
                != CA_OK)
            {
                ca_log_stderr(0, "init log failed");
                ret = -1;
                goto out;
            }

            ca_signal_init();

            if (conf_ctx.daemon) {
                ca_daemonize(0, 0);
            }

            if (pid_file_create((char *) conf_ctx.pid.data) != CA_OK) {
                ca_log_stderr(errno, "create pid file failed");
                ret = -1;
            } else {

                ret = 0;

                ca_init_processes();

                ca_master_process_cycle();
            }
        }

    } else if (ca_action == CA_STOP) {
        if (pid == CA_ERROR) {
            ca_log_stderr(errno, "checking pid file \"%V\" failed",
                          &conf_ctx.pid);
            ret = -1;
        } else if (pid == CA_OK) {
            ca_log_stderr(0, "no clagent daemon running");
            ret = -1;
        } else {
            ret = 0;
            if (kill(pid, SIGTERM) == -1) {
                ca_log_stderr(errno, "kill(%d, SIGTERM) failed", pid);
                ret = -1;
            }
        }

    } else {
        usage(EXIT_FAILURE);
        ret = -1;
    }

out:

    if (conf_ctx.oldpid.len != 0) {
        ca_free(conf_ctx.oldpid.data);
    }

    if (conf_ctx.acq_items) {
        ca_array_destroy(conf_ctx.acq_items);
    }

    if (conf_ctx.servers) {
        server = conf_ctx.servers->elem;
        for (i = 0; i < conf_ctx.servers->nelem; i++) {
            ca_free(server[i].addr_str);
        }
        ca_array_destroy(conf_ctx.servers);
    }

    ca_conf_free(&conf);

    if (ca_argv != NULL) {
        ca_argv_free(ca_argv);
    }

    ca_title_free();

    ca_log_deinit();

    return ret;
}


static int
ca_signal_init(void)
{
    ca_signal_t       *sig;
    struct sigaction   sa;
    int                status;

    for (sig = ca_signals; sig->signo != 0; sig++) {
        ca_memzero(&sa, sizeof(struct sigaction));
        sa.sa_handler = sig->handler;
        sa.sa_flags = sig->flags;
        sigemptyset(&sa.sa_mask);

        status = sigaction(sig->signo, &sa, NULL);
        if (status < 0) {
            ca_log_emerg(errno, "sigaction(%s) failed", sig->signame);
            return CA_ERROR;
        }
    }

    return CA_OK;
}


static void
ca_signal_handler(int signo)
{
    ca_signal_t   *sig;
    void         (*action)(void);
    char          *action_str;
    int            err;

    err = errno;

    for (sig = ca_signals; sig->signo != 0; sig++) {
        if (sig->signo == signo) {
            break;
        }
    }

    ASSERT(sig->signo != 0);

    action_str = "";
    action = NULL;

    switch (signo) {
    case SIGUSR1:
        break;

    case SIGUSR2:
        if (getppid() > 1 || ca_new_binary > 0) {
            /*
             * Ignore the signal in the new binary if its parent is
             * not the init process, i.e. the old binary's process
             * it still running. Or ignore the signal in the old binary's
             * process if the new binary's process is already running.
             */
            action_str = ", ignoring";
            break;
        }

        ca_change_binary = 1;
        action_str = ", changing binary";
        break;

    case SIGALRM:
        ca_sigalrm = 1;
        break;

    case SIGINT:
    case SIGTERM:
        action_str = ", terminating";
        ca_terminate = 1;
        break;

    case SIGQUIT:
        action_str = ", quiting";
        ca_quit = 1;
        break;

    case SIGHUP:
        action_str = ", reopening log file";
        action = ca_log_reopen;
        break;

        action_str = ", exiting";
        break;

    case SIGCHLD:
        action_str = ", reap child";
        switch (ca_process) {
        case CA_PROCESS_MASTER:
            ca_reap = 1;
            break;

        case CA_PROCESS_ACQ:
            break;
        case CA_PROCESS_UPDATE:
            break;
        }
        break;

    default:
        break;
    }

    ca_log_notice(0, "signal %d (%s) received%s", signo, sig->signame,
                  action_str);

    if (ca_reap) {
        ca_process_get_status();
    }

    if (action != NULL) {
        action();
    }

    errno = err;
}


static void
ca_process_get_status(void)
{
    int     status;
    int     i;
    pid_t   pid;
    int     one = 0;

    for ( ; ; ) {

        pid = waitpid(-1, &status, WNOHANG);
        if (pid == 0) {
            return;
        }

        if (pid == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == ECHILD && one) {
                return;
            }
            return;
        }
        one = 1;

        for (i = 0; i < ca_last_process; ++i) {
            if (ca_processes[i].pid == pid) {
                ca_processes[i].status = status;
                ca_processes[i].exited = 1;
                break;
            }
        }
    }
}


static void
ca_signal_child_processes(int signo)
{
    int  i, err;

    for (i = 0; i < ca_last_process; i++) {
        if (ca_processes[i].detached || ca_processes[i].pid == -1) {
            continue;
        }

        if (ca_processes[i].exited) {
            continue;
        }

        if (ca_processes[i].exiting && (signo == SIGTERM || signo == SIGINT)) {
            continue;
        }

        ca_log_debug(0, "kill(%d, %d)", ca_processes[i].pid, signo);

        if (kill(ca_processes[i].pid, signo) == -1) {
            err = errno;
            ca_log_alert(err, "kill(%d, %d) failed",
                         ca_processes[i].pid, signo);
            if (err == ESRCH) {
                ca_processes[i].exited = 1;
                ca_processes[i].exiting = 0;
                ca_reap = 1;
            }

            continue;
        }

        ca_processes[i].exiting = 1;
    }
}
