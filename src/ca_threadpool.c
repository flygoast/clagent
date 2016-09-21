#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include "ca_core.h"
#include "ca_heap.h"
#include "ca_threadpool.h"


#define POOL_MAX_IDLE       120 /* 2 minutes */


typedef struct task_st {
    void     (*func)(void *);
    void      *arg;
    int        priority;
} ca_task_t;


static int
ca_priority_less(void *ent1, void *ent2)
{
    ca_task_t  *t1 = (ca_task_t *)ent1;
    ca_task_t  *t2 = (ca_task_t *)ent2;

    return (t1->priority < t2->priority) ? 1 : 0;
}


static void *
ca_thread_loop(void *arg)
{
    ca_threadpool_t  *pool = (ca_threadpool_t*) arg;
    ca_task_t        *t = NULL;
    struct timespec   ts;
    struct timeval    tv;
    int               ret;
    int               tosignal;

    while (!pool->exit) {
        ca_pthread_mutex_lock(&pool->mutex);
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + POOL_MAX_IDLE;
        ts.tv_nsec = tv.tv_usec * 1000;

        while (pool->task_queue.len == 0) {
            ret = ca_pthread_cond_timedwait(&pool->cond, &pool->mutex, &ts);
            if (ret == 0) {
                if (pool->exit) {
                    goto EXIT;
                }
                break;
            } else if (ret == ETIMEDOUT) {
                goto EXIT;
            }
        }

        --pool->threads_idle;
        t = ca_heap_remove(&pool->task_queue, 0);
        tosignal = (pool->task_queue.len == 0) ? 1 : 0;
        ca_pthread_mutex_unlock(&pool->mutex);

        if (tosignal) {
            ca_pthread_cond_broadcast(&pool->task_over_cond);
        }

        if (t) {
            t->func(t->arg);
            ca_free(t);
        }

        ca_pthread_mutex_lock(&pool->mutex);
        ++pool->threads_idle;
        ca_pthread_mutex_unlock(&pool->mutex);
    }

    ca_pthread_mutex_lock(&pool->mutex);

EXIT:
    --pool->threads_idle;
    tosignal = --pool->threads_num ? 0 : 1;
    ca_pthread_mutex_unlock(&pool->mutex);
    if (tosignal) {
        ca_pthread_cond_broadcast(&pool->exit_cond);
    }

    return NULL;
}


static void
ca_threadpool_thread_create(ca_threadpool_t *pool)
{
    pthread_t       tid;
    pthread_attr_t  attr;

    ca_pthread_attr_init(&attr);
    ca_pthread_attr_setstacksize(&attr, pool->thread_stack_size);
    ca_pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ca_pthread_create(&tid, &attr, ca_thread_loop, pool);
    ca_pthread_attr_destroy(&attr);
}


static void
ca_threadpool_free_task_queue(ca_threadpool_t *pool) {
    ca_task_t  *t;
    while (pool->task_queue.len != 0) {
        t = ca_heap_remove(&pool->task_queue, 0);
        if (t) {
            free(t);
        }
    }
}


ca_threadpool_t *
ca_threadpool_create(int init, int max, int stack_size)
{
    ca_threadpool_t  *pool;
    int               i;

    assert(init > 0 && max >= init && stack_size >= 0);

    /* Allocate memory and zero all them. */
    pool = (ca_threadpool_t *)ca_calloc(1, sizeof(*pool));
    if (!pool) {
        return NULL;
    }

    ca_pthread_mutex_init(&pool->mutex, NULL);
    ca_pthread_cond_init(&pool->cond, NULL);
    ca_pthread_cond_init(&pool->exit_cond, NULL);
    ca_pthread_cond_init(&pool->task_over_cond, NULL);

    ca_heap_init(&pool->task_queue);
    ca_heap_set_less(&pool->task_queue, ca_priority_less);

    pool->thread_stack_size = (stack_size == 0) ? THREAD_STACK_SIZE :
                                                  stack_size;

    for (i = 0; i < init; ++i) {
        ca_threadpool_thread_create(pool);
    }

    pool->threads_idle = init;
    pool->threads_num = init;
    pool->threads_max = max;

    return pool;
}


int
ca_threadpool_add_task(ca_threadpool_t *pool,
    void (*func)(void*), void *arg, int priority)
{
    int         tosignal = 0;
    ca_task_t  *tq = (ca_task_t *) ca_calloc(1, sizeof(*tq));

    if (!tq) {
        return -1;
    }

    tq->func = func;
    tq->arg = arg;
    tq->priority = priority;

    ca_pthread_mutex_lock(&pool->mutex);
    if (pool->threads_idle == 0 && pool->threads_num < pool->threads_max) {
        ca_threadpool_thread_create(pool);
        ++pool->threads_idle;
        ++pool->threads_num;
    }

    tosignal = (pool->task_queue.len == 0)  ? 1 : 0;

    if (ca_heap_insert(&pool->task_queue, tq) != 0) {
        ca_free(tq);
        ca_pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    ca_pthread_mutex_unlock(&pool->mutex);
    if (tosignal) {
        ca_pthread_cond_broadcast(&pool->cond);
    }

    return 0;
}


void
ca_threadpool_clear_task_queue(ca_threadpool_t *pool) {
    ca_pthread_mutex_lock(&pool->mutex);
    ca_threadpool_free_task_queue(pool);
    ca_pthread_mutex_unlock(&pool->mutex);
}


void
ca_threadpool_exit(ca_threadpool_t *pool)
{
    ca_pthread_mutex_lock(&pool->mutex);
    pool->exit = 1;
    ca_pthread_mutex_unlock(&pool->mutex);
    ca_pthread_cond_broadcast(&pool->cond);
}


int
ca_threadpool_task_over(ca_threadpool_t *pool, int block, int timeout)
{
    int ret;

    ca_pthread_mutex_lock(&pool->mutex);
    if (pool->task_queue.len != 0) {
        if (!block) {
            ca_pthread_mutex_unlock(&pool->mutex);
            return -1;
        } else {
            struct timespec  ts;
            struct timeval   tv;

            gettimeofday(&tv, NULL);
            ts.tv_sec = tv.tv_sec + timeout;
            ts.tv_nsec = tv.tv_usec * 1000;

            while (pool->task_queue.len != 0) {
                if (timeout == 0) {
                    ca_pthread_cond_wait(&pool->task_over_cond,
                                         &pool->mutex);
                } else {
                    ret = ca_pthread_cond_timedwait(&pool->task_over_cond,
                                                    &pool->mutex, &ts);
                    if (ret == 0) {
                        ca_pthread_mutex_unlock(&pool->mutex);
                        return 0;
                    } else if (ret == ETIMEDOUT) {
                        ca_pthread_mutex_unlock(&pool->mutex);
                        return -1;
                    }
                }
            }
        }
    }

    ca_pthread_mutex_unlock(&pool->mutex);
    return 0;
}


int
ca_threadpool_destroy(ca_threadpool_t *pool, int block, int timeout)
{
    int  ret;

    assert(pool);

    ca_pthread_mutex_lock(&pool->mutex);
    if (!pool->exit) {
        /* you should call `threadpool_exit' first */
        ca_pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    if (pool->threads_num != 0) {
        if (!block) {
            ca_pthread_mutex_unlock(&pool->mutex);
            return -1;
        } else {
            struct timespec  ts;
            struct timeval   tv;
            gettimeofday(&tv, NULL);
            ts.tv_sec = tv.tv_sec + timeout;
            ts.tv_nsec = tv.tv_usec * 1000;

            while (pool->threads_num != 0) {
                if (timeout == 0) {
                    ca_pthread_cond_wait(&pool->exit_cond, &pool->mutex);
                    goto CONT;
                } else {
                    ret = ca_pthread_cond_timedwait(&pool->exit_cond,
                        &pool->mutex, &ts);
                    if (ret == 0) {
                        goto CONT;
                    } else if (ret == ETIMEDOUT) {
                        ca_pthread_mutex_unlock(&pool->mutex);
                        return -1;
                    }
                }
            }
        }
    }

CONT:

    ca_pthread_mutex_unlock(&pool->mutex);
    ca_heap_destroy(&pool->task_queue);
    ca_pthread_mutex_destroy(&pool->mutex);
    ca_pthread_cond_destroy(&pool->cond);
    ca_pthread_cond_destroy(&pool->exit_cond);
    ca_pthread_cond_destroy(&pool->task_over_cond);
    ca_free(pool);

    return 0;
}
