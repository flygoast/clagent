#ifndef __CA_MEMORY_H_INCLUDED__
#define __CA_MEMORY_H_INCLUDED__


u_char *ca_get_mem_total(time_t now, time_t freq);
u_char *ca_get_mem_used(time_t now, time_t freq);
u_char *ca_get_mem_free(time_t now, time_t freq);
u_char *ca_get_swap_total(time_t now, time_t freq);
u_char *ca_get_swap_used(time_t now, time_t freq);
u_char *ca_get_swap_free(time_t now, time_t freq);
u_char *ca_get_mem_cache(time_t now, time_t freq);
u_char *ca_get_mem_buffer(time_t now, time_t freq);
u_char *ca_get_mem_urate(time_t now, time_t freq);
u_char *ca_get_swap_urate(time_t now, time_t freq);


#endif /* __CA_MEMORY_H_INCLUDED__ */
