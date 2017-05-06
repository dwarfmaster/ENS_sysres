
#ifndef DEF_ARP_ARP
#define DEF_ARP_ARP

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

struct arp_params {
    void *haddr, *paddr;
    uint16_t htype, ptype;
    uint8_t hlen, plen;
};

/* Returns the size of the request. Returns 0 in case of error.
 * prcv CAN point into buffer */
size_t make_request(const struct arp_params* prms, void* prcv,
        void* buffer, size_t size);
size_t make_reply(struct arp_params* prms, void* prcv, void* hrcv,
        void* buffer, size_t size);

/* Get the ethertype of a request */
uint16_t peek_ptype(void* buffer, size_t size);
/* Parse a request, returns 0 if it is a request and 1 if it is an answer */
int read_pdu(void* buffer, size_t size,
        struct arp_params* prms, void** prcv, void** hrcv);

#endif//DEF_ARP_ARP

