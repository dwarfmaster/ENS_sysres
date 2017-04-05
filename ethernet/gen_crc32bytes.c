#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

static const uint32_t crc_poly = 0x04C11DB7; /* CRC32 polynomila without MSB */
static const uint32_t TOPBIT   = 0x80000000;

int main() {
    uint8_t byte = 0;
    printf("static const uint32_t crc_polybytes[256] = {\n");
    while(1) {
        uint32_t remainder = byte << 24;
        if(byte % 10 == 0) printf("    ");
        for(size_t bit = 8; bit > 0; --bit) {
            /* If upper bit is one */
            if(remainder & TOPBIT) {
                remainder = (remainder << 1) ^ crc_poly;
            } else {
                remainder = (remainder << 1);
            }
        }
        printf("0x%08X", remainder);
        if(byte != 255) printf(",");
        if(byte % 10 == 9) printf("\n");
        if(byte == 255) break;
        ++byte;
    }
    printf("\n};\n");
    return 0;
}
