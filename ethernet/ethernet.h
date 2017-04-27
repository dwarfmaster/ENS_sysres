
#ifndef DEF_ETHERNET_ETHERNET
#define DEF_ETHERNET_ETHERNET

#include <stdint.h>
#include <stdio.h>
#include <stdint.h>
#include "types.h"

struct mac_address {
    uint8_t bytes[6];
};

struct eth_snap {
    uint8_t vendor[3];
    uint8_t id[2];
};

/* Fields with a comment are required by make_frame */
struct eth_frame {
    struct mac_address src, dst; /* */
    uint16_t ethertype; /* */
    uint16_t size; /* */
    uint32_t crc;
    uint16_t tag;
    struct eth_snap snap;
    char* data; /* */
};

ethernet_error_t read_mac_address(const char* src, struct mac_address* dst);
ethernet_error_t compute_crc(const char* buffer, size_t size, uint32_t* crc);
/* Returns ETH_AGAIN in case of failure and ETH_SUCCESS if crc is valid */
ethernet_error_t check_crc(const char* buffer, size_t size, uint32_t crc);
/* Creates an ethernet II protocol frame, without tagging
 * buffer and frame->data CAN overlap
 */
ethernet_error_t make_frame(struct eth_frame* frame, char* buffer, size_t* size);
/* The frame is valid only as long as the buffer is.
 * size must be the size of the whole frame (determined by level 1
 * machinery)
 */
ethernet_error_t decode_frame(char* buffer, size_t size, struct eth_frame* frame);

#endif//DEF_ETHERNET_ETHERNET

