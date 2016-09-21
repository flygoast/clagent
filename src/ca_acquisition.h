#ifndef __CA_ACQUISITION_H_INCLUDED__
#define __CA_ACQUISITION_H_INCLUDED__


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "acq/ca_cpu.h"
#include "acq/ca_disk_io.h"
#include "acq/ca_disk_urate.h"
#include "acq/ca_load_average.h"
#include "acq/ca_memory.h"
#include "acq/ca_net_flow.h"


typedef u_char *(*ca_acq_item_handler_pt)(time_t now, time_t freq);

typedef struct {
    ca_str_t                name;
    ca_acq_item_handler_pt  item_handler;
    unsigned                exist:1;           
} ca_acq_item_handler_t;

extern ca_acq_item_handler_t  ca_acq_item_handlers[];

typedef struct {
    ca_str_t                item;
    ca_uint_t               freq;
    ca_int_t                id;
    ca_uint_t               id_len;
    ca_int_t                type;
    ca_uint_t               accessed;
    ca_acq_item_handler_pt  handler;
} ca_acq_t;


typedef struct {
    ca_str_t             host_str;
    ca_str_t             port_str;
    u_char              *addr_str;
    struct sockaddr_in   sin;
} ca_server_t;



void ca_acq_process_cycle(void *dummy);


#endif /* __CA_ACQUISITION_H_INCLUDED__ */

