#ifndef __CA_THREADPOOL_H_INCLUDED__
#define __CA_THREADPOOL_H_INCLUDED__


#include <pthread.h>
#include <assert.h>
#include "ca_heap.h"


#define THREAD_STACK_SIZE   1048576     /* 1M */


typedef struct threadpool {
    pthread_mutex_t     mutex;
    pthread_cond_t      cond;
    pthread_cond_t      exit_cond;
    pthread_cond_t      task_over_cond;
    ca_heap_t           task_queue;
    int                 thread_stack_size;
    int                 exit;
    int                 threads_idle;
    int                 threads_num;
    int                 threads_max;
} ca_threadpool_t;


/* ------------------- Macro wrappers ---------------------*/
#define ca_pthread_create(m, n, p, q) \
    assert(pthread_create(m, n, p, q) == 0)
#define ca_pthread_attr_init(m) \
    assert(pthread_attr_init(m) == 0)
#define ca_pthread_attr_setstacksize(m, n) \
    assert(pthread_attr_setstacksize(m, n) == 0)
#define ca_pthread_attr_setdetachstate(m, n) \
    assert(pthread_attr_setdetachstate(m, n) == 0)
#define ca_pthread_attr_destroy(m) \
    assert(pthread_attr_destroy(m) == 0)
#define ca_pthread_mutex_init(m, n) \
    assert(pthread_mutex_init(m, n) == 0)
#define ca_pthread_mutex_destroy(m) \
    assert(pthread_mutex_destroy(m) == 0)
#define ca_pthread_cond_init(m, n) \
    assert(pthread_cond_init(m, n) == 0)
#define ca_pthread_cond_destroy(m) \
    assert(pthread_cond_destroy(m) == 0)
#define ca_pthread_mutex_lock(m) \
    assert(pthread_mutex_lock(m) == 0)
#define ca_pthread_mutex_trylock(m) \
    assert(pthread_mutex_trylock(m) == 0)
#define ca_pthread_mutex_unlock(m) \
    assert(pthread_mutex_unlock(m) == 0)
#define ca_pthread_cond_wait(m, n) \
    assert(pthread_cond_wait(m, n) == 0)
#define ca_pthread_cond_signal(m) \
    assert(pthread_cond_signal(m) == 0)
#define ca_pthread_cond_broadcast(m) \
    assert(pthread_cond_broadcast(m) == 0)
#define ca_pthread_cond_timedwait(m, n, p) \
    pthread_cond_timedwait(m, n, p)


extern ca_threadpool_t *ca_threadpool_create(int init, int max,
    int stack_size);
extern int ca_threadpool_add_task(ca_threadpool_t *tp,
    void (*func)(void*), void *arg, int priority);
extern void ca_threadpool_clear_task_queue(ca_threadpool_t *pool);
extern int ca_threadpool_task_over(ca_threadpool_t *pool, int block,
    int timeout);
extern int ca_threadpool_destroy(ca_threadpool_t *pool, int block,
    int timeout);
extern void ca_threadpool_exit(ca_threadpool_t *pool);


#endif /* __CA_THREADPOOL_H_INCLUDED__ */
