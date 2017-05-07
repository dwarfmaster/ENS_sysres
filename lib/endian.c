
#include "endian.h"

uint16_t bswap16(uint16_t x) {
    return (x << 8) | (x >> 8);
}

uint32_t bswap32(uint32_t x) {
    return __builtin_bswap32(x);
}

/* Hardware (network) is bug endian and software
 * is little-endian.
 */
uint16_t stoh16(uint16_t x) {
    return bswap16(x);
}

uint32_t stoh32(uint32_t x) {
    return bswap32(x);
}

uint16_t htos16(uint16_t x) {
    return bswap16(x);
}

uint32_t htos32(uint32_t x) {
    return bswap32(x);
}

