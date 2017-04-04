
#ifndef DEF_ETHERNET_ETHERNET
#define DEF_ETHERNET_ETHERNET

#include "device.h"
#include <stdint.h>

struct eth_device {
    struct device dev;
    /* TODO */
};

struct mac_address {
    uint8_t bytes[6];
};

struct eth_frame {
    struct mac_address src, dst;
    uint32_t tag;
    uint16_t ethertype;
    uint16_t size;
    char* data;
    uint32_t crc;
};

ethernet_error_t read_mac_address(const char* src, struct mac_address* dst);
ethernet_error_t compute_crc(struct eth_frame* frame);
ethernet_error_t check_crc(struct eth_frame* frame);
/* Creates an ethernet II protocol frame, without tagging */
ethernet_error_t make_frame(struct eth_frame* frame, char* buffer, size_t size);
/* The frame is valid only as long as the buffer is.
 * The size of the frame is set in size.
 */
ethernet_error_t decode_frame(char* buffer, size_t* size, struct eth_frame* frame);

#endif//DEF_ETHERNET_ETHERNET

