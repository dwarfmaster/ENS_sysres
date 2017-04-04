
#include <stdlib.h>
#include <stdio.h>
#include "ethernet.h"

int main(int argc, char *argv[]) {
    struct mac_address addr;
    char buffer[256];
    while(1) {
        scanf("%s", buffer);
        if(read_mac_address(buffer, &addr) != ETH_SUCCESS) break;
        printf("%d - %d - %d - %d - %d - %d\n", addr.bytes[0], addr.bytes[1],
                addr.bytes[2], addr.bytes[3], addr.bytes[4], addr.bytes[5]);
    }
    printf("Invalid\n");
    return 0;
}

