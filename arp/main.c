
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
char* mac_address;
size_t mac_addr_len;
mach_port_t ethernet_port;

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

/* TrivFS symbols */
int trivfs_fstype        = FSTYPE_MISC;
int trivfs_fsid          = 0;
int trivfs_allow_open    = 0;
int trivfs_support_read  = 1;
int trivfs_support_write = 1;
int trivfs_support_exec  = 0;

/* Misc necessary trivfs callbacks */
void trivfs_modify_stat(struct trivfs_protid* cred, io_statbuf_t* st) {
    cred = cred; /* Fix warnings */
    st = st; /* Fix warnings */
    /* Do nothing */
}

error_t trivfs_S_file_set_size(struct trivfs_protid* cred, off_t size) {
    size = size; /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_seek(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t reply_type, off_t offs, int whence, off_t* new_offs) {
    reply      = reply;      /* Fix warnings */
    reply_type = reply_type; /* Fix warnings */
    offs       = offs;       /* Fix warnings */
    whence     = whence;     /* Fix warnings */
    new_offs   = new_offs;   /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_select(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* type, int* tag) {
    reply     = reply;     /* Fix warnings */
    replytype = replytype; /* Fix warnings */
    type      = type;      /* Fix warnings */
    tag       = tag;       /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_get_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    reply     = reply;     /* Fix warnings */
    replytype = replytype; /* Fix warnings */
    bits      = bits;      /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_set_all_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    reply     = reply;     /* Fix warnings */
    replytype = replytype; /* Fix warnings */
    bits      = bits;      /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_set_some_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    reply     = reply;     /* Fix warnings */
    replytype = replytype; /* Fix warnings */
    bits      = bits;      /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_clear_some_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    reply     = reply;     /* Fix warnings */
    replytype = replytype; /* Fix warnings */
    bits      = bits;      /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_goaway(struct trivfs_control* cntl, int flags) {
    cntl  = cntl;  /* Fix warnings */
    flags = flags; /* Fix warnings */
    exit(EXIT_SUCCESS);
    return 0;
}

// Routines

typedef void (* routine_t) (mach_msg_header_t *inp, mach_msg_header_t *outp);

void lvl2_frame_r(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    outp = outp; /* Fix warnings */
    mach_msg_type_t* tp = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
    char* data          = (char*)tp + sizeof(mach_msg_type_t);;
    size_t size         = tp->msgt_size * tp->msgt_number;
    uint16_t type       = peek_ptype(data, size);
    char* ha_addr       = NULL;
    char buf[1 << 12];
    struct handler* handler;
    typeinfo_t tpinfo;
    char *prcv, *hrcv;
    
    handler = lookup_from_ptype(type);
    if(!handler) {
        log_variadic("Ignored arp message of type %d\n", type);
        return;
    }

    if(read_pdu(data, size, &handler->params, &prcv, &hrcv)) {
        /* ARP answer */
        tpinfo.id     = arp_answer;
        tpinfo.size   = handler->params.plen + handler->params.hlen;
        tpinfo.number = 1;
        printf("%d.%d.%d.%d -> %02X:%02X:%02X:%02X:%02X:%02X",
                prcv[0], prcv[1], prcv[2], prcv[3],
                hrcv[0], hrcv[1], hrcv[2],
                hrcv[3], hrcv[4], hrcv[5]);

        memmove(buf, prcv, handler->params.plen);
        memmove(buf + handler->params.plen, hrcv, handler->params.hlen);
        send_data(handler->out, &tpinfo, buf);
    } else {
        /* ARP query */
        if(ha_addr == NULL) return; // WUT ?
        if(memcmp(prcv, handler->addr, handler->addr_len) != 0) return;
        size          = make_reply(&handler->params, prcv, ha_addr, buf, 4096);
        tpinfo.id     = lvl32_frame;
        tpinfo.size   = size;
        tpinfo.number = 1;
        send_data(ethernet_port, &tpinfo, buf);
    }
}

void arp_query_r(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    outp                      = outp;
    mach_port_t ethernet_port = MACH_PORT_NULL; // TODO receive ethernet port
    if(ethernet_port == MACH_PORT_NULL) return;

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
    outp                = outp; /* prevent warnings */
    mach_msg_type_t* tp = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
    char* data          = (char*)tp + sizeof(mach_msg_type_t);;
    arp_register_t* reg = (arp_register_t*)data;
    add_handler(reg->type, reg->len, reg->data, reg->port);
}

static int arp_demuxer(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    routine_t routine   = NULL;
    mach_msg_type_t* tp = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
    lvl1_new_t* lvl1    = (lvl1_new_t*)tp;

    log_variadic("msgt_name : %d\n", tp->msgt_name);

    switch(tp->msgt_name) {
        case lvl1_new:
            ethernet_port = lvl1->port;
            mac_addr_len  = lvl1->addr_len;
            memcpy(mac_address, lvl1->addr, mac_addr_len);
            break;
        
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
            if(!trivfs_demuxer(inp, outp))
                return 0;
    }
    if(routine)
        (*routine) (inp, outp);
    return 1;
}

int main(int argc, char *argv[]) {
    error_t err;
    mach_port_t bootstrap;
    struct trivfs_control* fsys;

    init_handlers();

    /* TODO parse arguments */

    task_get_bootstrap_port(mach_task_self(), &bootstrap);
    if(bootstrap == MACH_PORT_NULL) {
        log_string("Must be started as a translator");
        return 1; // INVALID
    }

    err = trivfs_startup(bootstrap, 0, 0, 0, 0, 0, &fsys);
    if(err) {
        log_string("Couldn't setup translator");
        return 1; // IO
    }

    ports_manage_port_operations_multithread(fsys->pi.bucket,
            arp_demuxer, 0, 0, 0);
    return 0; // SUCCESS
}

