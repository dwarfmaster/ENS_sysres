
#ifndef DEF_IPV4_IP
#define DEF_IPV4_IP

#include <stdint.h>
#include <stdbool.h>

struct ip_header {
    uint8_t  hd;       /* Version and IHL */
    uint8_t  dscp_ecn; /* DSCP and ECN */
    uint16_t length;
    uint16_t identification;
    uint16_t fragments;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src, dst;
};

/* Setters and getters */
uint8_t build_hd(uint8_t version, uint8_t ihl);
void split_hd(uint8_t hd, uint8_t* version, uint8_t* ihl);
uint8_t build_dscp_ecn(uint8_t dscp, uint8_t ecn);
void split_dscp_ecn(uint8_t dscp_ecn, uint8_t* dscp, uint8_t* ecn);
uint16_t build_fragments(bool df, bool mf, uint16_t offset);
void split_fragments(uint16_t fragments, bool* df, bool* mf, uint16_t* offset);

/* Read IP of form xxx.xxx.xxx.xxx */
uint32_t read_ip_addr(const char* text);
/* Write IP. Assumes buffer is of length at least 12 */
void write_ip_addr(uint32_t addr, char* buffer);

/* Checksum */
void compute_checksum(struct ip_header* hd);
bool check_checksum(struct ip_header* hd);

#endif//DEF_IPV4_IP

