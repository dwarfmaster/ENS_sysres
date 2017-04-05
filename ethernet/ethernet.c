
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

/* Returns an error if the size of the message is too big/too small. */
ethernet_error_t compute_crc(struct eth_frame* frame) {
    /* TODO */
}

ethernet_error_t check_crc(struct eth_frame* frame) {
    /* TODO */
}

/*****************************************************************
 ******************** Frame Handling *****************************
 *****************************************************************/

struct ethII_header {
    uint8_t bytes[12];
    uint16_t ethertype;
} __attribute__((__packed__));

struct ethII_header_tagged {
    uint8_t bytes[12];
    uint16_t tpid;
    uint16_t tci;
    uint16_t ethertype;
} __attribute__((__packed__));

struct eth_header_llc {
    uint8_t dsap, ssap;
    uint8_t control;
    struct eth_snap snap;
} __attribute__((__packed__));


/* Makes an ethernet II frame, without tag, assuming CRC is already
 * computed.
 */
ethernet_error_t make_frame(struct eth_frame* frame, char* buffer, size_t size) {
    struct ethII_header* hd = (struct ethII_header*)buffer;
    int* crc = (int*)(buffer + sizeof(struct ethII_header) + frame->size);
    size_t pack_size = sizeof(struct ethII_header) + 4 + frame->size;
    if(pack_size > size) return ETH_INVALID;

    for(size_t i = 0; i < 12; ++i) hd->bytes[i] = (i >= 6 ? frame->src.bytes[i-6] : frame->dst.bytes[i]);
    hd->ethertype = frame->ethertype;
    memcpy(buffer + sizeof(struct ethII_header), frame->data, frame->size);
    *crc = frame->crc; /* Is this endian dependant ? */

    return ETH_SUCCESS;
}




ethernet_error_t decode_frame(char* buffer, size_t size, struct eth_frame* frame) {
    struct ethII_header* hd = (struct ethII_header*)buffer;
    struct ethII_header_tagged* hd_tag = (struct ethII_header_tagged*)buffer;
    char* data = NULL;
    uint32_t* crc = (uint32_t*)(buffer + size - 4);
    if(size < 64) return ETH_INVALID;


    if(hd->ethertype == 0x8100) {
        frame->tag       = hd_tag->tci;
        frame->ethertype = hd_tag->ethertype;
        data             = buffer + sizeof(struct ethII_header_tagged);
    } else {
        frame->tag       = 0; /* This value is reserved when tag is present, so
                               * it can be used to test the absence of tag
                               */
        frame->ethertype = hd->ethertype;
        data             = buffer + sizeof(struct ethII_header);
    }


    struct eth_header_llc* llc = (struct eth_header_llc*)data;
    if(frame->ethertype >= 1536) { /* Ethernet II frame, ie DIX ethernet */
        frame->size = size - (data - buffer) - 4;
        frame->data = data;
        frame->crc  = *crc;
        return ETH_SUCCESS;
    }
    
    else if(frame->ethertype <= 1500) {
        /* Novel Raw IPX */
        if(llc->dsap == 0xFF && llc->ssap == 0xFF) {
            frame->size      = frame->ethertype;
            frame->crc       = *crc;
            frame->data      = data;
            frame->ethertype = 0x8137; /* IPX ethertype */
        }
        /* Snap extension */
        else if(llc->dsap == 0xAA && llc->ssap == 0xAA) {
            frame->size      = frame->ethertype - 8;
            frame->crc       = *crc;
            frame->data      = data + 8;
            frame->snap      = llc->snap;
            frame->ethertype = 0; /* Invalid ethertype => snap must be used */
        }
        /* Standart LLC-only ethernet */
        else {
            /* TODO : we won't handle it for now, it's too complicated */
            return ETH_INVALID; /* Drop the frame */
        }

        if(frame->size + 4 + (size_t)(data - buffer) != size) return ETH_INVALID;
        else                                                  return ETH_SUCCESS;
    }
    
    else {
        return ETH_INVALID;
    }
}

