
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ethernet.h"
#include "logging.h"

int main(int argc, char *argv[]) {
    char buffer[256];
    size_t n;
    uint32_t crc;
    while(1) {
        n = 256;
        scanf("%s", buffer);
        if(compute_crc(buffer, strlen(buffer), &crc) != ETH_SUCCESS) break;
        log_variadic("%08X\n", crc);
    }
    log_string("Invalid");
    return 0;
}

