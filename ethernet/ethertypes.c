
#include "ethertypes.h"
#include "logging.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct type_file {
    int fd;
    struct type_file* next;
    uint16_t tp;
};

static struct type_file* opened[256];
static char* type_dir;
static char* type_buffer;

static inline uint8_t hash(uint16_t fd) {
    return fd % 256;
}

/* Returns NULL is not found */
static inline struct type_file* lookup(uint16_t tp) {
    uint8_t hs = hash(tp);
    struct type_file* fds = opened[hs];

    while(fds != NULL) {
        if(fds->tp == tp) return fds;
        fds = fds->next;
    }
    return NULL;
}

ethernet_error_t types_init(const char* dir) {
    for(int i = 0; i < 256; ++i) opened[256] = NULL;
    size_t len = strlen(dir);
    type_dir = malloc(len);
    if(!type_dir) return ETH_AGAIN;
    type_dir[0] = 0;
    if(!strcat(type_dir, dir)) {
        free(type_dir);
        return ETH_AGAIN;
    }

    len += 5;
    type_buffer = malloc(len);
    if(!type_buffer) {
        free(type_dir);
        return ETH_AGAIN;
    }
    return ETH_SUCCESS;
}

ethernet_error_t types_register(uint16_t tp) {
    if(lookup(tp) == NULL) return ETH_INVALID;
    uint8_t hs = hash(tp);
    struct type_file* tf = malloc(sizeof(struct type_file));
    if(!tf) return ETH_AGAIN;

    sprintf(type_buffer, "%s/%04X", type_dir, tp);
    tf->fd = open(type_buffer, O_WRONLY);
    if(tf->fd < 0) {
        free(tf);
        return ETH_IO;
    }

    tf->tp = tp;
    tf->next = opened[hs];
    opened[hs] = tf;
    return ETH_SUCCESS;
}

void dispatch(uint16_t tp, uint16_t size, uint8_t* data) {
    struct type_file* tf = lookup(tp);
    if(!tf) {
        log_variadic("Unhandled ethertype %04X\n", tp);
        return;
    }
    write(tf->fd, &size, sizeof(uint16_t));
    write(tf->fd, data,  size);
}


