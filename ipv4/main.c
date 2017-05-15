
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
    uint8_t b0, b1, b2, b3;
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
mac_port_t arp_port;

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
    send_data(arp_port, &tpinfo, (char*)req->buffer);
    if(req->next != NULL) send_request(req->next, ma);
}

// MSG sent: mac_address + ip_header + ip_data
void get_mac_address_r(mach_msg_header_t *inp, mach_msg_header_t *outp) {
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

    uint16_t hs = hash(ip);
    struct mac_value* mv = cache[hs];

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

    uint32_t time = get_time();
    while(mv != NULL) {
        if(mv->ip == ip) break;
        mv = mv->next;
    }
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
        mv->waiting = 1;
        // TODO send mach_msg to ip to seek ip mac_address
    }
    if(mv->waiting) {
        req->next = mv->reqs;
        mv->reqs  = req;
    } else {
        send_request(req, mv->ma);
    }
}

int main(int argc, char *argv[]) {
    char* buffer[2048];
    uint32_t ip;
    while(1) {
        scanf("%s", buffer);
        ip = read_ip_addr(buffer);
        write_ip_addr(ip, buffer);
        printf("%s\n", buffer);
    }
    return 0;
}

