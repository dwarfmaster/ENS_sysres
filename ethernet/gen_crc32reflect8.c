#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

static const uint32_t crc_poly = 0x04C11DB7; /* CRC32 polynomila without MSB */
static const uint32_t TOPBIT   = 0x80000000;

int main() {
    uint8_t byte = 0;
    printf("static const uint8_t reflect8[256] = {\n");
    while(1) {
        if(byte % 10 == 0) printf("    ");
        uint8_t res = 0;
        for(size_t i = 0; i < 8; ++i) {
            if(byte & (1 << i)) res |= (1 << (7 - i));
        }
        printf("0x%02X", res);
        if(byte != 255) printf(",");
        if(byte % 10 == 9) printf("\n");
        if(byte == 255) break;
        ++byte;
    }
    printf("\n};\n");
    return 0;
}
