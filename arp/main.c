
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hurd/trivfs.h>
#include "arp.h"
#include "logging.h"
#include "proto.h"
#include "ports.h"

struct handler {
    uint16_t type;
    uint8_t addr_len;
    char addr[256]; /* Assumes address is under 256-bytes */
    struct arp_params params;
    mach_port_t out;
    struct handler* next;
};

struct handler* handlers;

void init_handlers() {
    handlers = NULL;
};

struct handler* lookup_from_ptype(uint16_t ptype) {
    struct handler* hd = handlers;
    while(hd != NULL) {
        if(hd->type == ptype) return hd;
        hd = hd->next;
    }

    return NULL;
}

int add_handler(uint16_t type, uint8_t len, char* addr, mach_port_t out) {
    struct handler* hd;

    hd = lookup_from_ptype(type);
    if(hd) {
        log_variadic("%hu already has an handler\n", type);
        return 0;
    }

    hd = malloc(sizeof(struct handler));
    if(!hd) {
        log_string("Failed adding handler");
        return 0;
    }

    hd->next      = handlers;
    hd->type      = type;
    hd->addr_len  = len;
    memcpy(&hd->addr, addr, len);
    hd->out  = out;
    handlers = hd;
    return 1;
}

void remove_handler(uint16_t type) {
    struct handler* prev = handlers;
    struct handler* hd = handlers;
    while(hd != NULL) {
        if(hd->type == type) {
            if(hd == handlers) handlers   = hd->next;
            else               prev->next = hd->next;
            free(hd);
            return;
        }
        prev = hd;
        hd = hd->next;
    }
}

void free_handlers() {
    while(handlers != NULL) {
        struct handler* hd = handlers->next;
        free(handlers);
        handlers = hd;
    }
}

/*
mig_external mig_routine_t socket_server_routine
	(const mach_msg_header_t *InHeadP)
{
	int msgh_id;
 
	msgh_id = InHeadP->msgh_id - 26000;
 
	if ((msgh_id > 15) || (msgh_id < 0))
		return 0;
 
	return socket_server_routines[msgh_id];
}
*/

// Routines

typedef void (* routine_t) (mach_msg_header_t *inp, mach_msg_type_t *outp);

void lvl2_frame_r(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    mach_msg_type_t* tp       = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
    char* data                = (char*)tp + sizeof(mach_msg_type_t);;
    size_t size               = tp->msgt_size * tp->msgt_number;
    uint16_t type             = peek_ptype(data, size);
    mach_port_t ethernet_port = MACH_PORT_NULL; // TODO receive ethernet port
    char* ha_addr             = NULL;
    char buf[1 << 12];
    struct handler* handler;
    typeinfo_t tpinfo;
    void *prcv, *hrcv;
    
    handler = lookup_from_ptype(type);
    if(!handler) {
        log_variadic("Ignored arp message of type %d\n", type);
        return;
    }

    if(read_pdu(data, size, &handler->params, &prcv, &hrcv)) {
        /* ARP answer */
        tpinfo.id = arp_answer;
        tpinfo.size = handler->params.plen + handler->params.hlen;
        tpinfo.number = 1;

        memmove(buf, prcv, handler->params.plen);
        memmove(buf + handler->params.plen, hrcv, handler->params.hlen);
        send_data(handler->out, &tpinfo, buf);
    } else {
        /* ARP query */
        if(ha_addr == NULL) return; // WUT ?
        if(memcmp(prcv, handler->addr, handler->addr_len) != 0) return;
        size = make_reply(&handler->params, prcv, ha_addr, buf, 4096);
        tpinfo.id     = lvl32_frame;
        tpinfo.size   = size;
        tpinfo.number = 1;
        send_data(ethernet_port, &tpinfo, buf);
    }
}

void arp_query_r(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    mach_port_t ethernet_port = MACH_PORT_NULL; // TODO receive ethernet port
    if(ethernet_port == MACH_PORT_NULL) continue;

    char buf[1 << 12];
    typeinfo_t tpinfo;

    mach_msg_type_t* tp     = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
    char* data              = (char*)tp + sizeof(mach_msg_type_t);;
    arp_query_t* query      = (arp_query_t*)data;
    struct handler* handler = lookup_from_ptype(query->type);
    size_t size             = make_request(&handler->params, query->addr, buf, 4096);

    tpinfo.id     = lvl32_frame;
    tpinfo.size   = size;
    tpinfo.number = 1;
    send_data(ethernet_port, &tpinfo, buf);
}

void arp_register_r(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    mach_msg_type_t* tp = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
    char* data          = (char*)tp + sizeof(mach_msg_type_t);;
    arp_registe_t* reg  = (arp_register_t*)data;
    add_handler(reg->type, reg->len, reg->data, reg->port);
}

static int arp_demuxer(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    routine_t routine;
    mach_msg_type_t tp = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
    switch(tp->msgt_name) {
        // TODO receive ethernet_port
        
        case lvl2_frame:
            routine = lvl2_frame_r;
            break;
        
        case arp_query:
            routine = arp_query_r;
            break;
        
        case arp_register:
            routine = arp_register_r;
            break;

        default:
            // Unknown message type in arp
            routine = NULL;
            if(!trivfs_demuxer(inp, outp))
                return 0;
    }
    if(routine)
        (*routine) (inp, outp);
    return 1;
}

// TODO: return error ?
int launch_registerer() {
    error_r err;
    mach_port_t bootstrap;
    struct trivfs_control* fsys;

    task_get_bootstrap_port(mach_task_self(), &bootstrap);
    if(bootstrap == MACH_PORT_NULL) {
        log_string("Must be started as a translator");
        return 0; // INVALID
    }

    err = trivfs_startup(bootstrap, 0, 0, 0, 0, 0, &fsys);
    if(err) {
        log_string("Couldn't setup translator");
        return 0; // IO
    }

    ports_manage_port_operations_multithread(fsys->pi.bucket,
            arp_demuxer, 0, 0, 0);
    return 1; // SUCCESS
}

int main(int argc, char *argv[]) {
    init_handlers();

    /* TODO parse arguments */

    // TODO Useless now ?
    //kern_return_t ret;
    //mach_port_t fs_port;
    //ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &fs_port);
    //if(ret != KERN_SUCCESS) {
    //    log_string("Couldn't allocate fs port");
    //    exit(EXIT_FAILURE);
    //}

    /* TODO attach to filesystem */

    launch_registerer();
    return 0;
}

