
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

int main(int argc, char *argv[]) {
    init_handlers();
    kern_return_t ret;
    mach_port_t fs_port;
    char buf[4096];
    uint16_t type;
    mach_msg_header_t* hd;
    mach_msg_type_t* tp;
    char* data;
    size_t size;
    void *prcv, *hrcv;
    struct handler* handler;
    typeinfo_t tpinfo;
    arp_register_t* reg;
    arp_query_t* query;
    mach_port_t ethernet_port;
    char* ha_addr;

    /* TODO parse arguments */

    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &fs_port);
    if(ret != KERN_SUCCESS) {
        log_string("Couldn't allocate fs port");
        exit(EXIT_FAILURE);
    }

    /* TODO attach to filesystem */

    ethernet_port = MACH_PORT_NULL;
    ha_addr = NULL;
    for(;;) {
        if(!receive_data_low(&fs_port, &hd, buf, 4096)) continue;
        tp = (mach_msg_type_t*)((char*)hd + sizeof(mach_msg_header_t));
        data = (char*)tp + sizeof(mach_msg_type_t);
        size = tp->msgt_size * tp->msgt_number;

        switch(tp->msgt_name) {
            /* TODO receive ethernet_port */

            case lvl2_frame:
                type = peek_ptype(data, size);
                handler = lookup_from_ptype(type);
                if(!handler) {
                    log_variadic("Ignored arp message of type %d\n", type);
                    continue;
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
                    if(ha_addr == NULL) continue;
                    if(memcmp(prcv, handler->addr, handler->addr_len) != 0) continue;
                    size = make_reply(&handler->params, prcv, ha_addr, buf, 4096);
                    tpinfo.id     = lvl32_frame;
                    tpinfo.size   = size;
                    tpinfo.number = 1;
                    send_data(ethernet_port, &tpinfo, buf);
                }
                break;

            case arp_query:
                if(ethernet_port == MACH_PORT_NULL) continue;
                query   = (arp_query_t*)data;
                handler = lookup_from_ptype(query->type);
                size    = make_request(&handler->params, query->addr, buf, 4096);
                tpinfo.id     = lvl32_frame;
                tpinfo.size   = size;
                tpinfo.number = 1;
                send_data(ethernet_port, &tpinfo, buf);
                break;

            case arp_register:
                reg = (arp_register_t*)data;
                add_handler(reg->type, reg->len, reg->data, reg->port);
                break;

            default:
                log_variadic("Unknown message type in arp : %d\n", tpinfo.id);
                break;
        }
    }

    return 0;
}

