
#ifndef DEF_LIB_PROTO
#define DEF_LIB_PROTO

#include <hurd.h>
#include <mach.h>
#include <stdlib.h>
#include <stdint.h>

enum Id {
    lvl1_frame = 2999, /* Chosen to be the value of device read */
    lvl1_new = 42,  /* Big enough not to collide with trivfs msgt_names */
    lvl2_frame,
    lvl3_frame,
    lvl32_frame, /* Data is a level-2 address and the data itself */
    lvl4_frame,  /* Assume data preceded by a level-3 address */
    upper_frame,
    reserved1,
    reserved2,
    reserved3,
    arp_query,
    arp_register,
    arp_answer,
    timer_msg
};

typedef struct lvl1_new {
    mach_msg_type_t port_type;
    mach_port_t port;

    mach_msg_type_t addr_type;
    size_t addr_len;
    char addr[];
} __attribute__ ((__packed__)) lvl1_new_t;

typedef struct arp_register {
    mach_msg_type_t port_type;
    mach_port_t port;

    mach_msg_type_t content_type;
    uint16_t type;
    uint8_t  len;
    char data[];
} __attribute__ ((__packed__)) arp_register_t;

/* An ARP answer is the requested logical address followed without
 * gap by the physical address. */

/* An ARP query is a type and a logical address to be queried */
typedef struct arp_query {
    uint16_t type;
    char addr[];
} __attribute__ ((__packed__)) arp_query_t;

#endif//DEF_LIB_PROTO

