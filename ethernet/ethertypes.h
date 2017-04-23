
#ifndef DEF_ETHERNET_ETHERTYPES
#define DEF_ETHERNET_ETHERTYPES

#include "types.h"
#include <stdint.h>

ethernet_error_t types_init();
/* Returns ETH_INVALID if the ethertypes is already registered */
ethernet_error_t types_register(uint16_t tp);
void dispatch(uint16_t tp, uint16_t size, uint8_t* data);
ethernet_error_t launch_registerer();

#endif//DEF_ETHERNET_ETHERTYPES

