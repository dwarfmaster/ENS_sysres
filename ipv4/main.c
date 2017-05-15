
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hurd.h>
#include <hurd/trivfs.h>
#include "ip.h"
#include "logging.h"
#include "ports.h"
#include "proto.h"
#include "timer.h"
#include <fcntl.h>

#define MAX_SIZE (1 << 16) - 20

#define IPv4    4
#define VERSION IPv4
#define IHL     5
#define DSCP    0 // ???
#define ECN     0 // ???
#define TCP     0x06
#define TIMEOUT 900

struct mac_address {
    uint8_t b0, b1, b2, b3, b4, b5;
};

struct requests {
    char buffer[1 << 11];
    size_t size;
    struct requests* next;
};

struct mac_value {
    uint32_t ip;
    struct mac_address ma;
    char waiting;
    uint32_t time;
    struct requests* reqs;
    struct mac_value* next;
};

static struct mac_value* cache[1 << 16];
mach_port_t ip_port, arp_port, timer_port;
char myip[4];

static inline uint16_t hash(uint32_t ip) {
    return ip % (1 << 16);
}

void send_request(struct requests* req, struct mac_address ma) {
    struct typeinfo tpinfo;
    tpinfo.id = lvl32_frame;
    tpinfo.size = req->size;
    memcpy(req->buffer, &ma, sizeof(struct mac_address));
    send_data(arp_port, &tpinfo, (char*)req->buffer);
    if(req->next != NULL) send_request(req->next, ma);
    free(req);
}

struct mac_value* lookup(uint32_t ip) {
    struct mac_value* mv = cache[hash(ip)];
    while(mv != NULL) {
        if(mv->ip == ip) break;
        mv = mv->next;
    }
    return mv;
}

/* TrivFS symbols */
int trivfs_fstype        = FSTYPE_MISC;
int trivfs_fsid          = 0;
int trivfs_allow_open    = O_READ | O_WRITE;
int trivfs_support_read  = 0;
int trivfs_support_write = 0;
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

void make_request(mach_port_t port, char* ip) {
    arp_query_t* query;
    char buffer[256];
    typeinfo_t tpinfo;

    query = (arp_query_t*)buffer;
    query->type = 0x0800; // IPv4
    memcpy(query->addr, ip, 4);
    tpinfo.id   = arp_query;
    tpinfo.size = 6;
    send_data(port, &tpinfo, buffer);
}

// Routines

typedef void (* routine_t) (mach_msg_header_t *inp, mach_msg_header_t *outp);

// MSG sent: ethertype + mac_address + ip_header + ip_data
void send_to_r(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    outp = outp; /* Fix warnings */
    mach_msg_type_t* tp = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
    char* data          = (char*)tp + sizeof(mach_msg_header_t);
    size_t size         = (tp->msgt_size / 8) * tp->msgt_number;
    if(size > MAX_SIZE) {
        log_variadic("ip packet size is too big : %u\n", size);
        return;
    }

    struct requests* req = malloc(sizeof(struct requests));
    struct ip_header* ih;

    uint32_t ip = *((uint32_t*)data); // TODO may have to convert if little endian
    data += sizeof(uint32_t);

    uint32_t time = get_time();

    req->size = 2 + sizeof(struct mac_address) + sizeof(struct ip_header) + size;
    req->next = NULL;

    char* et = req->buffer;
    et[0] = 0x08;
    et[1] = 0x00;

    ih = (struct ip_header*)req->buffer + 2 + sizeof(struct mac_address);
    ih->hd             = build_hd(VERSION, IHL);
    ih->dscp_ecn       = build_dscp_ecn(DSCP, ECN);
    ih->length         = sizeof(struct ip_header) + size;
    ih->identification = time % (1 << 16); // identify packet
    ih->fragments      = build_fragments(0, 0, 0); // We shouldn't have to fragmentize ourselves
    ih->ttl            = 64;
    ih->protocol       = TCP; // TODO generalize
    ih->src            = 0; // TODO find source ip
    ih->dst            = ip;
    compute_checksum(ih);

    memcpy(ih + sizeof(struct ip_header), data, size);

    struct mac_value* mv = lookup(ip);
    if(mv == NULL) {
        mv = malloc(sizeof(struct mac_value));
        mv->ip = ip;
        mv->waiting = 0;
        mv->time = time - TIMEOUT - 1; // To ensure that current mac address is outdated
        mv->next = cache[hash(ip)];
        cache[hash(ip)] = mv;
    }

    if(!mv->waiting && time - mv->time > TIMEOUT) {
        add_timer(timer_port, 0, 0, ip);
        mv->waiting = 1;
    }
    if(mv->waiting) {
        req->next = mv->reqs;
        mv->reqs  = req;
    } else {
        send_request(req, mv->ma);
    }
}

void refresh_ip_r(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    outp = outp; /* Fix warnings */
    mach_msg_type_t* tp = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
    char* data          = (char*)tp + sizeof(mach_msg_header_t);
    size_t size         = (tp->msgt_size / 8) * tp->msgt_number;
    if(size != sizeof(uint32_t) + sizeof(struct mac_address)) {
        log_variadic("IP received arp answer of invalid length : %u\n", size);
        return;
    }

    uint32_t ip           = *((uint32_t*)data);
    struct mac_address ma = *((struct mac_address*)data + sizeof(uint32_t));

    struct mac_value* mv = lookup(ip);
    if(mv == NULL || mv->waiting == 0) {
        char buf[16];
        write_ip_addr(ip, buf);
        log_variadic("IP ignored arp answer for ip : %s\n", buf);
        return;
    }

    mv->time = get_time();
    mv->ma = ma;
    send_request(mv->reqs, ma);
    mv->reqs = NULL;
    mv->waiting = 0;
}

static int ip_demuxer(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    routine_t routine   = NULL;
    timer_message_t* msg;
    uint32_t ip;

    log_variadic("ipv4 received : %d\n", inp->msgh_id);

    switch(inp->msgh_id) {
        case lvl4_frame:
            routine = send_to_r;
            break;

        case timer_msg:
            msg = (timer_message_t*)inp
                + sizeof(mach_msg_header_t)
                + sizeof(mach_msg_type_t);
            ip = *(uint32_t*)msg->data;
            make_request(arp_port, (char*)(&ip));
            if(lookup(ip)->waiting == 1) add_timer(timer_port, 60*1000, 0, ip);
            break;

        case arp_answer:
            routine = refresh_ip_r;
            break;

        default:
            if(!trivfs_demuxer(inp, outp))
                return 0;
    }
    if(routine)
        (*routine) (inp, outp);
    return 0;
}

int main() {
    error_t err;
    kern_return_t ret;
    mach_port_t bootstrap;
    struct trivfs_control* fsys;
    char buffer[1 << 11];

    for(int i = 0; i < 4; myip[i++] = 0x00);

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

    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &ip_port);
    if(ret != KERN_SUCCESS) {
        printf("Couldn't allocate input port for IPv4\n");
        return 1;
    }

    timer_port = start_timer_thread(ip_port);

    // TODO use better arp address
    arp_port = file_name_lookup("./0806", O_READ | O_WRITE, 0);
    if(arp_port == MACH_PORT_NULL) {
        printf("Couldn't open arp port\n");
        return 1;
    }

    arp_register_t* reg = (arp_register_t*)buffer;
    reg->port_type.msgt_name          = MACH_MSG_TYPE_MAKE_SEND;
    reg->port_type.msgt_size          = sizeof(mach_port_t) * 8;
    reg->port_type.msgt_number        = 1;
    reg->port_type.msgt_inline        = TRUE;
    reg->port_type.msgt_longform      = FALSE;
    reg->port_type.msgt_deallocate    = FALSE;
    reg->port_type.msgt_unused        = 0;
    reg->port                         = ip_port;

    reg->content_type.msgt_name       = MACH_MSG_TYPE_UNSTRUCTURED;
    reg->content_type.msgt_size       = 8;
    reg->content_type.msgt_number     = 8;
    reg->content_type.msgt_inline     = TRUE;
    reg->content_type.msgt_longform   = FALSE;
    reg->content_type.msgt_deallocate = FALSE;
    reg->content_type.msgt_unused     = 0;
    reg->type                         = 0x0800; // IPv4
    reg->len                          = 4;
    memcpy(reg->data, myip, 4);

    send_data_low(arp_port, sizeof(arp_register_t) + 8, buffer, arp_register);

    ports_manage_port_operations_one_thread(fsys->pi.bucket,
            ip_demuxer, 0);
    return 0; // SUCCESS
}

