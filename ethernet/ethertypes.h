
#ifndef DEF_ETHERNET_ETHERTYPES
#define DEF_ETHERNET_ETHERTYPES

#include "types.h"
#include <stdint.h>
#include <hurd.h>

struct reserved2_data {
    mach_port_t nport;
    uint16_t tp;
};

ethernet_error_t types_init(const char* dir, mach_port_t to_main, mach_port_t from_main);
/* Returns ETH_INVALID if the ethertypes is already registered */
ethernet_error_t types_register(uint16_t tp);
/* Assumes data is larger than size by at least 64 bytes */
void dispatch(uint16_t tp, uint16_t size, char* data);
ethernet_error_t launch_registerer();

#endif//DEF_ETHERNET_ETHERTYPES

