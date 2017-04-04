
#include "ethernet.h"
#include <string.h>

int char_value(char c) {
    if(c >= '0' && c <= '9')      return c - '0';
    else if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    else if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    else                          return -1;
}

int pair_value(const char* src, size_t id) {
    int i1 = char_value(src[id]);
    int i2 = char_value(src[id + 1]);
    if(i1 < 0 || i2 < 0) return -1;
    else                 return i1 * 16 + i2;
}

#define READ_PAIR(j) \
    i = pair_value(src, 3*j); \
    if(i < 0) return ETH_INVALID; \
    dst->bytes[j] = (uint8_t)i
ethernet_error_t read_mac_address(const char* src, struct mac_address* dst) {
    /* TODO : support other formats */
    int i;
    if(strlen(src) != 17 || src[2] != ':'
            || src[5]  != ':' || src[8]  != ':'
            || src[11] != ':' || src[14] != ':') {
        return ETH_INVALID;
    }

    READ_PAIR(0);
    READ_PAIR(1);
    READ_PAIR(2);
    READ_PAIR(3);
    READ_PAIR(4);
    READ_PAIR(5);
    return ETH_SUCCESS;
}

ethernet_error_t compute_crc(struct eth_frame* frame) {
    /* TODO */
}

ethernet_error_t check_crc(struct eth_frame* frame) {
    /* TODO */
}

ethernet_error_t make_frame(struct eth_frame* frame, char* buffer, size_t size) {
    /* TODO */
}

ethernet_error_t decode_frame(char* buffer, size_t* size, struct eth_frame* frame) {
    /* TODO */
}

