
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "arp.h"
#include "logging.h"

struct handler {
    uint16_t type;
    uint8_t addr_len;
    char addr[256]; /* Assumes address is under 256-bytes */
    struct arp_params params;
    char* out;
    struct handler* next;
};

struct handler* handlers;

void init_handlers() {
    handlers = NULL;
};

struct handler* lookup_from_ptype(uint16_t ptype) {
    struct handler* hd = handlers;
    while(hd != NULL) {
        if(hd->ptype == ptype) return hd;
        hd = hd->next;
    }

    return NULL;
}

int add_handler(uint16_t type, uint8_t len, char* addr, const char* out) {
    struct handler* hd;

    hd = lookup_from_ptype(params->ptype);
    if(hd) {
        log_variadic("%hu already has an handler\n", params->ptype);
        return 0;
    }

    hd = malloc(sizeof(struct handler));
    if(!hd) {
        log_string("Failed adding handler");
        return 0;
    }

    hd->next = handlers;
    hd->type = type;
    hd->len  = len;
    memcpy(&hd->addr, addr, len);
    hd->out = malloc(strlen(out));
    if(!hd->out) {
        log_string("Failed adding handler");
        free(hd);
        return 0;
    }
    strcpy(hd->out, out);
    handlers = hd;
    return 1;
}

void remove_handler(uint16_t type) {
    struct handler* prev = handlers;
    struct handler* hd = handlers;
    while(hd != NULL) {
        if(hd->type == type) {
            if(hd == handlers) handlers   = hd->next;
            else               prev->next = hd->next;
            free(hd->out);
            free(hd);
            return;
        }
        prev = hd;
        hd = hd->next;
    }
}

void free_handlers() {
    while(handlers != NULL) {
        struct handler* hd = handlers->next;
        free(handlers->out);
        free(handlers);
        handlers = hd;
    }
}

int main(int argc, char *argv[]) {
    init_handlers();
    log_string("Hello world");
    return 0;
}

