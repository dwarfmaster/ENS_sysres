
#ifndef DEF_LIB_ENDIAN
#define DEF_LIB_ENDIAN

#include <stdint.h>

/* TODO make them inline */
uint16_t bswap16(uint16_t x);
uint32_t bswap32(uint32_t x);
uint16_t stoh16(uint16_t x);
uint32_t stoh32(uint32_t x);
uint16_t htos16(uint16_t x);
uint32_t htos32(uint32_t x);

#endif//DEF_LIB_ENDIAN

