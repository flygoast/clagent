#ifndef __CLAGENT_H_INCLUDED__
#define __CLAGENT_H_INCLUDED__


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <signal.h>
#include <errno.h>
#include "ca_core.h"
#include "ca_queue.h"
#include "ca_log.h"
#include "ca_string.h"
#include "ca_array.h"
#include "ca_buf.h"
#include "ca_util.h"
#include "ca_conf.h"
#include "ca_daemon.h"
#include "ca_update.h"
#include "ca_acquisition.h"
#include "ca_worker.h"


#define PROG_NAME            	"clagent"
#define CA_VERSION           	"0.0.1"
#define CA_VERSION_HEX       	0x00000001
#define CA_CONF_PATH         	"/usr/local/clagent/conf/clagent.conf"
#define CA_PID_PATH          	"/usr/local/clagent/var/clagent.pid"
#define CA_LOG_PATH          	"/usr/local/clagent/log/clagent.log"
#define CA_UPDATE_URL        	"http://foo.bar.com/clagent/version"
#define CA_UPDATE_EXE        	"/usr/local/clagent/bin/updater"
#define CA_OLDPID_EXT        	".oldbin"

#define MASTER_PROCESS_NAME  	PROG_NAME"[master]"
#define UPDATE_PROCESS_NAME  	PROG_NAME"[update]"
#define ACQ_PROCESS_NAME     	PROG_NAME"[acq]"
#define WORKER_PROCESS_NAME     PROG_NAME"[worker]"


#define CA_START                    0
#define CA_STOP                     1


#define CA_MAX_PROCESSES            1024
#define CA_PROCESS_NORESPAWN        -1
#define CA_PROCESS_JUST_SPAWN       -2
#define CA_PROCESS_RESPAWN          -3
#define CA_PROCESS_JUST_RESPAWN     -4
#define CA_PROCESS_DETACHED         -5

#define CA_PROCESS_MASTER           0
#define CA_PROCESS_ACQ              1
#define CA_PROCESS_UPDATE           2
#define CA_PROCESS_NETWORK          3
#define CA_PROCESS_WORKER           4


typedef struct {
    ca_flag_t    daemon;
    ca_str_t     log_file;
    ca_str_t     log_level_str;
    ca_int_t     log_level;
    ca_str_t     pid;
    ca_str_t     oldpid;
    ca_uint_t    check_interval;
    ca_uint_t    max_nfree;
    ca_str_t     update_url;
    ca_str_t     update_exe;
    ca_str_t     identify;
    ca_uint_t    connect_timeout;
    ca_uint_t    send_timeout;
    ca_uint_t    recv_timeout;
    ca_array_t  *acq_items;
    ca_array_t  *servers;
} ca_conf_ctx_t;


typedef struct {
    char   *path;
    char   *name;
    char  **argv;
    char  **envp;
} ca_exec_ctx_t;


extern int           ca_process;
extern sig_atomic_t  ca_quit;
extern sig_atomic_t  ca_terminate;


pid_t ca_execute(ca_exec_ctx_t *ctx);


#endif /* __CLAGENT_H_INCLUDED__ */
