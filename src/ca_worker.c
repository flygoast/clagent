#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>
#include "clagent.h"
#include "ca_threadpool.h"
#include "ca_so.h"


ca_threadpool_t    *task_pool;
ca_heap_t          *result_queue;


typedef struct {
    void      *arg;
    int        priority;
} ca_result_t;


typedef struct {
    void  *(*module_init)(void *conf);
    void  *(*module_fetcher)(void *ctx);
    void   (*module_process_task)(void *ctx);
    void  *(*module_submiter)(void *ctx);
    void   (*module_deinit)(void *ctx);
} module_func_t;


module_func_t  mf;


ca_symbol_t  module_syms[] = {
    {"module_init",             (void **)&mf.module_init, 0},
    {"module_fetcher_cycle",    (void **)&mf.module_fetcher, 1},
    {"module_process_task",     (void **)&mf.module_process_task, 1},
    {"submiter_cycle",          (void **)&mf.module_submiter, 1},
    {"module_deinit",           (void **)&mf.module_deinit, 0},
    {NULL, NULL, 0}
};


static int ca_priority_less(void *ent1, void *ent2);


void
ca_worker_process_cycle(void *dummy)
{
    sigset_t            set;
    ca_conf_ctx_t      *conf;
    pthread_t           fetcher;
    pthread_t           submiter;
    pthread_attr_t      attr;

    void               *module_handle;
    void               *module_context;

    conf = dummy;
    ca_process = CA_PROCESS_WORKER;

    sigemptyset(&set);
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        ca_log_err(errno, "sigprocmask() failed");
    }

    if (ca_load_so(&module_handle, module_syms, "./module.so") < 0) {
        ca_log_crit(0, "load module so failed");
        exit(255);
    }

    module_context = NULL;
    if (mf.module_init) {
        module_context = mf.module_init(conf);
    }

    task_pool = ca_threadpool_create(1, 10, THREAD_STACK_SIZE);
    if (task_pool == NULL) {
        ca_log_crit(0, "create threadpool failed");
        exit(255);
    }

    result_queue = ca_heap_create();
    if (result_queue == NULL) {
        ca_log_crit(0, "create result queue failed");
        exit(255);
    }
    ca_heap_set_less(result_queue, ca_priority_less);

    ca_pthread_attr_init(&attr);
    ca_pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ca_pthread_create(&fetcher, &attr, mf.module_fetcher, module_context);
    ca_pthread_create(&submiter, &attr, mf.module_submiter, module_context);
    ca_pthread_attr_destroy(&attr);

    while (1) {
        if (ca_quit || ca_terminate) {
            break;
        }

        sleep(1);
    }

    if (mf.module_deinit) {
        mf.module_deinit(module_context);
    }

    ca_unload_so(&module_handle);

    ca_log_debug(0, "exit");

    exit(0);
}


int
ca_submit_task(void *data, int pri)
{
    return ca_threadpool_add_task(task_pool, mf.module_process_task, data, pri);
}


int
ca_submit_result(void *data, int priority)
{
    ca_result_t  *result = (ca_result_t *) ca_alloc(sizeof(ca_result_t));
    if (result == NULL) {
        return -1;
    }

    return ca_heap_insert(result_queue, result);
}


void *
ca_get_result()
{
    return ca_heap_remove(result_queue, 0);
}


static int
ca_priority_less(void *ent1, void *ent2)
{
    ca_result_t  *t1 = (ca_result_t *)ent1;
    ca_result_t  *t2 = (ca_result_t *)ent2;

    return (t1->priority < t2->priority) ? 1 : 0;
}
