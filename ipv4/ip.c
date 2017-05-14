
#include "ip.h"
#include <stdio.h>

uint8_t build_hd(uint8_t version, uint8_t ihl) {
    return ((version & 0xF) << 4) | (ihl & 0xF);
}

void split_hd(uint8_t hd, uint8_t* version, uint8_t* ihl) {
    *version = ((hd >> 4) & 0xF);
    *ihl     = hd & 0xF;
}

uint8_t build_dscp_ecn(uint8_t dscp, uint8_t ecn) {
    return ((dscp & 0x3F) << 2) | (ecn & 0x2);
}

void split_dscp_ecn(uint8_t dscp_ecn, uint8_t* dscp, uint8_t* ecn) {
    *dscp = ((dscp_ecn >> 2) & 0x3F);
    *ecn  = dscp_ecn & 0x2;
}

uint16_t build_fragments(bool df, bool mf, uint16_t offset) {
    return ((uint16_t)(!!df & 0x1) << 14)
         | ((uint16_t)(!!mf & 0x1) << 13)
         | (offset & 0x1fff);
}

void split_fragments(uint16_t fragments, bool* df, bool* mf, uint16_t* offset) {
    *df     = ((fragments >> 14) & 0x1);
    *mf     = ((fragments >> 13) & 0x1);
    *offset = fragments & 0x1fff;
}

uint32_t read_ip_addr(const char* text) {
    uint8_t b0, b1, b2, b3;
    if(sscanf(text, "%hhu.%hhu.%hhu.%hhu", &b0, &b1, &b2, &b3) != 4) return 0;
    else return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | (uint32_t)b3;
}

void write_ip_addr(uint32_t addr, char* buffer) {
    snprintf(buffer, 16, "%u.%u.%u.%u", (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF);
}

uint16_t _compute_checksum(void* vhd) {
    uint32_t ck = 0;
    uint16_t* hd = (uint16_t*)vhd;
    for(size_t i = 0; i < 10; ++i) ck += (uint32_t)hd[i];
    while(ck > 0xffff) ck = (ck & 0xffff) + ((ck >> 16) & 0xff);
    return ~ck;
}

void compute_checksum(struct ip_header* hd) {
    hd->checksum = 0;
    hd->checksum = _compute_checksum(hd);
}

bool check_checksum(struct ip_header* hd) {
    return (_compute_checksum(hd) == 0);
}

