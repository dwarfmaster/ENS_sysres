
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ip.h"

int main(int argc, char *argv[]) {
    char* buffer[2048];
    uint32_t ip;
    while(1) {
        scanf("%s", buffer);
        ip = read_ip_addr(buffer);
        write_ip_addr(ip, buffer);
        printf("%s\n", buffer);
    }
    return 0;
}

