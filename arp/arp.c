
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
    if(size < sizeof(struct arp_request_header) + 2 + 3*prms->hlen + 2*prms->plen) return 0;

    *(uint16_t*)buffer = 0x0806;
    buffer += 2;
    memcpy(buffer, prms->broadcast, prms->hlen);
    buffer += prms->hlen;

    struct arp_request_header* hd = (struct arp_request_header*)buffer;
    char* data = buffer + sizeof(struct arp_request_header);

    hd->htype = stoh16(prms->htype);
    hd->ptype = stoh16(prms->ptype);
    hd->hlen  = prms->hlen;
    hd->plen  = prms->plen;
    hd->oper  = stoh16(1);

    memcpy(data, prms->haddr, prms->hlen); data += prms->hlen;
    memcpy(data, prms->paddr, prms->plen); data += prms->plen;
    memset(data, 0,           prms->hlen); data += prms->hlen; /* In requests, the target hardware address is ignored */
    memcpy(data, prcv,        prms->plen);

    return sizeof(struct arp_request_header) + 2 + 3*prms->hlen + 2*prms->plen;
}

size_t make_reply(struct arp_params* prms, void* prcv, void* hrcv, void* buffer, size_t size) {
    if(size < sizeof(struct arp_request_header) + 2 + 3*prms->hlen + 2*prms->plen) return 0;

    *(uint16_t*)buffer = 0x0806;
    buffer += 2;
    memcpy(buffer, hrcv, prms->hlen);
    buffer += prms->hlen;

    struct arp_request_header* hd = (struct arp_request_header*)buffer;
    char* data = buffer + sizeof(struct arp_request_header);

    hd->htype = stoh16(prms->htype);
    hd->ptype = stoh16(prms->ptype);
    hd->hlen  = prms->hlen;
    hd->plen  = prms->plen;
    hd->oper  = stoh16(2);

    memcpy(data, prms->haddr, prms->hlen); data += prms->hlen;
    memcpy(data, prms->paddr, prms->plen); data += prms->plen;
    memcpy(data, hrcv,        prms->hlen); data += prms->hlen;
    memcpy(data, prcv,        prms->plen);

    return sizeof(struct arp_request_header) + 2 + 3*prms->hlen + 2*prms->plen;
}

uint16_t peek_ptype(void* buffer, size_t size) {
    if(size <= 8) return 0;
    struct arp_request_header* hd = (struct arp_request_header*)buffer;
    return htos16(hd->ptype);
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
    assert(prms->htype == htos16(hd->htype));
    assert(prms->ptype == htos16(hd->ptype));

    *hrcv = data;
    if(htos16(hd->oper) == 2) {
        *prcv = data + 2 * prms->hlen + prms->plen;
        if(memcmp(*prcv, prms->paddr, prms->plen) != 0) return -1;
    }
    *prcv = data + prms->hlen;
    return htos16(hd->oper) == 2;
}


