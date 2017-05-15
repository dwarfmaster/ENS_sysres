
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hurd.h>
#include <hurd/trivfs.h>
#include "ip.h"
#include "ports.h"
#include "proto.h"

#define MAX_SIZE (1 << 16) - 20

#define IPv4    4
#define VERSION IPv4
#define IHL     5
#define DCSP    0 // ???
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
mac_port_t ip_port;

static inline uint16_t hash(uint32_t ip) {
    return ip % (1 << 16);
}

uint32_t get_time() {
    time_value_t time;
    host_get_time(mach_host_self(), &time);
    return time.seconds * 1000 + time.microseconds / 1000;
}

int send_request(struct requests* req, struct mac_address ma) {
    struct typeinfo tpinfo;
    tpinfo.id = lvl32_frame;
    tpinfo.size = req->size;
    memcpy(req->buffer, &ma, sizeof(struct mac_address));
    send_data(ip_port, &tpinfo, (char*)req->buffer);
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

// Routines

typedef void (* routine_t) (mach_msg_header_t *inp, mach_msg_header_t *outp);

// MSG sent: mac_address + ip_header + ip_data
void send_to_r(mach_msg_header_t *inp, mach_msg_header_t *outp) {
    outp = outp; /* Fix warnings */
    mach_msg_type_t tp  = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
    char* data          = (char*)tp + sizeof(mach_msg_header_t);
    size_t size         = (tp->msgt_size / 8) * tp->msgt_number;
    if(size > MAX_SIZE) {
        log_variadic("ip packet size is too big : %u\n", size);
        return;
    }

    struct request* req = malloc(sizeof(struct requests));
    struct ip_header* ih;

    uint32_t ip = *((uint32_t*)data); // TODO may have to convert if little endian
    data += sizeof(uint32_t);

    uint32_t time = get_time();

    req->size = sizeof(struct mac_address) + sizeof(struct ip_header) + size;
    req->next = NULL;

    ih = req->buffer + sizeof(struct mac_address);
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
        mv->next = cache[hs];
        cache[hs] = mv;
    }

    // This may fail if the answer of a previous request arrives
    // before we add the new one to the queue
    if(!mv->waiting && time - mv->time > TIMEOUT) {
        // TODO send mach_msg to ip to seek ip mac_address
        send_data(arp_port, &tp_info, buf);

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
    mach_msg_type_t tp  = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));
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
    mach_msg_type_t* tp = (mach_msg_type_t*)((char*)inp + sizeof(mach_msg_header_t));

    log_variadic("ipv4 received : %d\n", inp->msgh_id);

    switch(inp->msgh_id) {
        case lvl4_frame:
            routine = send_to_r;
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

    // TODO use name of final ipv4 translator
    ip_port = file_name_lookup("./IPv4", O_READ | O_WRITE, 0);

    ports_manage_port_operations_one_thread(fsys->pi.bucket,
            ip_demuxer, 0);
    return 0; // SUCCESS
}

