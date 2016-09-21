#ifndef __CA_LOAD_AVERAGE_H_INCLUDED__
#define __CA_LOAD_AVERAGE_H_INCLUDED__


u_char *ca_get_loadavg_1(time_t now, time_t freq);
u_char *ca_get_loadavg_5(time_t now, time_t freq);
u_char *ca_get_loadavg_15(time_t now, time_t freq);


#endif /* __CA_LOAD_AVERAGE_H_INCLUDED__ */
