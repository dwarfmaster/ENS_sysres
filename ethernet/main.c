
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ethernet.h"

int main(int argc, char *argv[]) {
    char buffer[256];
    size_t n;
    uint32_t crc;
    while(1) {
        n = 256;
        getline(&buffer, &n, stdin);
        if(compute_crc(buffer, strlen(buffer), &crc) != ETH_SUCCESS) break;
        printf("%08X\n", crc);
    }
    printf("Invalid\n");
    return 0;
}

