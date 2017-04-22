
#include "ports.h"
#include <string.h>

struct message_full_header {
    mach_msg_header_t head;
    mach_msg_type_t type;
};

int send_data(mach_port_t port, const typeinfo_t* info, char* data) {
    mach_msg_return_t err;
    struct message_full_header* hd = (struct message_full_header*)data;
    unsigned int size = info->size * info->number;
    memmove(data + sizeof(struct message_full_header), data, size);

    hd->head.msgh_bits = MACH_MSGH_BITS_REMOTE(
            MACH_MSG_TYPE_MAKE_SEND);
    hd->head.msgh_size = size + sizeof(struct message_full_header);
    hd->head.msgh_local_port = MACH_PORT_NULL;
    hd->head.msgh_remote_port = port;

    hd->type.msgt_name = info->id;
    hd->type.msgt_size = info->size * 8;
    hd->type.msgt_number = info->number;
    hd->type.msgt_inline = TRUE;
    hd->type.msgt_longform = FALSE;
    hd->type.msgt_deallocate = FALSE;

    err = mach_msg( &hd->head, MACH_SEND_MSG,
            hd->head.msgh_size, 0, MACH_PORT_NULL,
            MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if(err != MACH_MSG_SUCCESS) return 0;
    return 1;
}

/* TODO use timeout */
int receive_data(mach_port_t port, typeinfo_t* info, char* buffer, size_t size) {
    mach_msg_return_t err;
    struct message_full_header* hd = (struct message_full_header*)buffer;
    hd->head.msgh_size = size;
    err = mach_msg( &hd->head, MACH_RCV_MSG,
            0, hd->head.msgh_size, port,
            MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if(err != MACH_MSG_SUCCESS) return 0;

    info->id     = hd->type.msgt_name;
    info->size   = hd->type.msgt_size / 8;
    info->number = hd->type.msgt_number;
    memmove(buffer, buffer + sizeof(struct message_full_header), info->size * info->number);
    return 1;
}

