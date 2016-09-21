#ifndef __CA_NET_FLOW_H_INCLUDED__
#define __CA_NET_FLOW_H_INCLUDED__


u_char *ca_get_intranet_flow_in(time_t now, time_t freq);
u_char *ca_get_intranet_flow_out(time_t now, time_t freq);
u_char *ca_get_extranet_flow_in(time_t now, time_t freq);
u_char *ca_get_extranet_flow_out(time_t now, time_t freq);
u_char *ca_get_intranet_pkgs_in(time_t now, time_t freq);
u_char *ca_get_intranet_pkgs_out(time_t now, time_t freq);
u_char *ca_get_extranet_pkgs_in(time_t now, time_t freq);
u_char *ca_get_extranet_pkgs_out(time_t now, time_t freq);
u_char *ca_get_total_flow_in(time_t now, time_t freq);
u_char *ca_get_total_flow_out(time_t now, time_t freq);
u_char *ca_get_total_pkgs_in(time_t now, time_t freq);
u_char *ca_get_total_pkgs_out(time_t now, time_t freq);


#endif /* __CA_NET_FLOW_H_INCLUDED__ */
