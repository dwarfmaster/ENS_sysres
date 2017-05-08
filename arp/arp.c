
#include "arp.h"
#include "endian.h"
#include <string.h>
#include <assert.h>

struct arp_request_header {
    uint16_t htype, ptype;
    uint8_t hlen, plen;
    uint16_t oper;
} __attribute__ ((__packed__));

size_t make_request(const struct arp_params* prms, void* prcv, void* buffer, size_t size) {
    if(size < sizeof(struct arp_request_header) + 3*prms->hlen + 2*prms->plen) return 0;
    struct arp_request_header* hd = (struct arp_request_header*)buffer;
    char* data = buffer + prms->hlen + sizeof(struct arp_request_header);
    memcpy(buffer, prms->broadcast, prms->hlen);

    hd->htype = stoh16(prms->htype);
    hd->ptype = stoh16(prms->ptype);
    hd->hlen  = prms->hlen;
    hd->plen  = prms->plen;
    hd->oper  = stoh16(1);

    memcpy(data, prms->haddr, prms->hlen); data += prms->hlen;
    memcpy(data, prms->paddr, prms->plen); data += prms->plen;
    data += prms->hlen; /* In requests, the target hardware address is ignored */
    memcpy(data, prcv,        prms->plen);

    return sizeof(struct arp_request_header) + 3*prms->hlen + 2*prms->plen;
}

size_t make_reply(struct arp_params* prms, void* prcv, void* hrcv, void* buffer, size_t size) {
    if(size < sizeof(struct arp_request_header) + 3*prms->hlen + 2*prms->plen) return 0;
    struct arp_request_header* hd = (struct arp_request_header*)buffer;
    char* data = buffer + prms->hlen + sizeof(struct arp_request_header);
    memcpy(buffer, hrcv, prms->hlen);

    hd->htype = stoh16(prms->htype);
    hd->ptype = stoh16(prms->ptype);
    hd->hlen  = prms->hlen;
    hd->plen  = prms->plen;
    hd->oper  = stoh16(0);

    memcpy(data, prms->haddr, prms->hlen); data += prms->hlen;
    memcpy(data, prms->paddr, prms->plen); data += prms->plen;
    memcpy(data, hrcv,        prms->hlen); data += prms->hlen;
    memcpy(data, prcv,        prms->plen);

    return sizeof(struct arp_request_header) + 3*prms->hlen + 2*prms->plen;
}

uint16_t peek_ptype(void* buffer, size_t size) {
    if(size <= 8) return 0;
    struct arp_request_header* hd = (struct arp_request_header*)buffer;
    return hd->ptype;
}

int read_pdu(void* buffer, size_t size, struct arp_params* prms, void** prcv, void** hrcv) {
    if(size < sizeof(struct arp_request_header) + 2*prms->hlen + 2*prms->plen) {
        *prcv = NULL;
        *hrcv = NULL;
        return -1;
    }

    struct arp_request_header* hd = (struct arp_request_header*)buffer;
    char* data = buffer + sizeof(struct arp_request_header);

    assert(prms->hlen  == hd->hlen);
    assert(prms->plen  == hd->plen);
    assert(prms->htype == hd->htype);
    assert(prms->ptype == hd->ptype);

    *hrcv = data;
    *prcv = data + prms->hlen;
    return htos16(hd->oper);
}


