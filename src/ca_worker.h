#ifndef __CA_WORKER_H_INCLUDED__
#define __CA_WORKER_H_INCLUDED__


int ca_submit_task(void *data, int priority);
int ca_submit_result(void *data, int priority);

void *ca_get_result();

void ca_worker_process_cycle(void *dummy);


#endif /* __CA_WORKER_H_INCLUDED__ */
