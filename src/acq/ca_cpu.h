#ifndef __CA_CPU_H_INCLUDED__
#define __CA_CPU_H_INCLUDED__


u_char *ca_get_cpu_system(time_t now, time_t freq);
u_char *ca_get_cpu_user(time_t now, time_t freq);
u_char *ca_get_cpu_io(time_t now, time_t freq);
u_char *ca_get_cpu_idle(time_t now, time_t freq);
u_char *ca_get_procs_running(time_t now, time_t freq);
u_char *ca_get_procs_blocked(time_t now, time_t freq);


#endif /* __CA_CPU_H_INCLUDED__ */
