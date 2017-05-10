
#include "tcp.h"
#include "endian.h"
#include <string.h>
#include <assert.h>

#define HLEN4  4
#define HLEN6 16

struct tcp_frame_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_nb;
    uint32_t ack_nb;
    uint8_t offset : 4;
    uint8_t reserved : 3;
    int ns  : 1;
    int cwr : 1;
    int ece : 1;
    int urg : 1;
    int ack : 1;
    int psh : 1;
    int rst : 1;
    int syn : 1;
    int fin : 1;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__ ((__packed__));

/* Works on software endian values */
inline static uint16_t add16(uint16_t a, uint16_t b) {
    uint16_t s = a + b;
    return (s < a ? s + 1 : s);
}

/* Checksum is returned in network byte ordering */
uint16_t compute_cheksum(char* buffer, size_t size, int ipv6, const char* src, const char* dst) {
    uint16_t sum = 0;
    size_t i;

    if(ipv6) {
        /* Add the addresses */
        for(i = 0; i < HLEN6 / 2; ++i) {
            sum = add16(sum, ((uint16_t)src[2*i] << 8) + (uint16_t)src[2*i+1]);
            sum = add16(sum, ((uint16_t)dst[2*i] << 8) + (uint16_t)dst[2*i+1]);
        }
        /* Add the TCP length */
        sum = add16(sum, size >> 16);
        sum = add16(sum, (uint16_t)size);
        /* Add the next header field */
        sum = add16(sum, 6);
    } else {
        /* Add the addresses */
        for(i = 0; i < HLEN4 / 2; ++i) {
            sum = add16(sum, ((uint16_t)src[2*i] << 8) + (uint16_t)src[2*i+1]);
            sum = add16(sum, ((uint16_t)dst[2*i] << 8) + (uint16_t)dst[2*i+1]);
        }
        /* Add the protocol field */
        sum = add16(sum, 6);
        /* Add the TCP length field */
        sum = add16(sum, (uint16_t)size);
    }

    /* Add the header and content of the TCP frame */
    for(i = 0; i < size / 2; ++i) {
        if(i == 9) /* checksum field is assumed zero'd */ continue;
        sum = add16(sum, ((uint16_t)buffer[2*i] << 8) + (uint16_t)buffer[2*i + 1]);
    }
    if(size % 2 == 1) sum = add16(sum, (uint16_t)buffer[size - 1] << 8);

    return sum;
}

int read_frame(char* buffer, size_t size, tcp_frame_t* fr, int ipv6, const char* src, const char* dst) {
    struct tcp_frame_header* hd = (struct tcp_frame_header*)buffer;
    size_t addr_size = (ipv6 ? 16 : 4);
    assert(sizeof(struct tcp_frame_header) == 20);
    if(size <= 13 + addr_size || size <= (size_t)hd->offset * 4) return 0;
    buffer += addr_size;

    fr->src_port  = htos16(hd->src_port);
    fr->dst_port  = htos16(hd->dst_port);
    fr->seq       = htos32(hd->seq_nb);
    fr->ack       = htos32(hd->ack_nb);
    fr->window    = htos16(hd->window);
    fr->urgent    = htos16(hd->urgent);
    fr->data      = buffer + hd->offset * 4;
    fr->flags.urg = hd->urg;
    fr->flags.ack = hd->ack;
    fr->flags.psh = hd->psh;
    fr->flags.rst = hd->rst;
    fr->flags.syn = hd->syn;
    fr->flags.fin = hd->fin;

    return hd->checksum == stoh16(compute_cheksum(buffer, size, ipv6, src, dst));
}

int build_frame(char* buffer, size_t* size, const tcp_frame_t* fr, size_t datalength,
        int ipv6, const char* src, const char* dst) {
    struct tcp_frame_header* hd = (struct tcp_frame_header*)buffer;
    size_t addr_size = (ipv6 ? 16 : 4);
    assert(sizeof(struct tcp_frame_header) == 20);
    if(*size <= 13 + addr_size || *size <= 20 + datalength) return 0;
    memcpy(buffer, src, addr_size);
    buffer += addr_size;

    *size = 20 + datalength;
    memmove(buffer + 20, fr->data, datalength);

    hd->src_port = stoh16(fr->src_port);
    hd->dst_port = stoh16(fr->dst_port);
    hd->seq_nb   = stoh32(fr->seq);
    hd->ack_nb   = stoh32(fr->ack);
    hd->offset   = 5;
    hd->ns       = 0;
    hd->cwr      = 0;
    hd->ece      = 0;
    hd->urg      = fr->flags.urg;
    hd->ack      = fr->flags.ack;
    hd->psh      = fr->flags.psh;
    hd->rst      = fr->flags.rst;
    hd->syn      = fr->flags.syn;
    hd->fin      = fr->flags.fin;
    hd->window   = stoh16(fr->window);
    hd->urgent   = stoh16(fr->urgent);
    hd->checksum = stoh16(compute_cheksum(buffer, *size, ipv6, src, dst));
    return 1;
}

