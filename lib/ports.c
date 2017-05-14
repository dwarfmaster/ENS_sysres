
#include "ports.h"
#include <string.h>

struct message_full_header {
    mach_msg_header_t head;
    mach_msg_type_t type;
};

mach_port_type_t get_send_right(mach_port_t port) {
    mach_port_type_t tp;
    mach_port_type(mach_task_self(), port, &tp);
    switch(tp) {
        case MACH_PORT_TYPE_SEND_ONCE:
        case MACH_PORT_TYPE_SEND:
            return MACH_MSG_TYPE_PORT_SEND;
        default:
            return MACH_MSG_TYPE_MAKE_SEND;
    }
}

int send_data_low(mach_port_t port, size_t size, char* data, int id) {
    mach_msg_return_t err;
    mach_msg_header_t* hd = (mach_msg_header_t*)data;
    memmove(data + sizeof(mach_msg_header_t), data, size);

    hd->msgh_bits = MACH_MSGH_BITS_REMOTE(get_send_right(port));
    hd->msgh_size = size + sizeof(struct message_full_header);
    /* round size to multiple of 4 */
    hd->msgh_size = ((hd->msgh_size + 3) >> 2) << 2;
    hd->msgh_local_port = MACH_PORT_NULL;
    hd->msgh_remote_port = port;
    hd->msgh_id = id;

    err = mach_msg( hd, MACH_SEND_MSG,
            hd->msgh_size, 0, MACH_PORT_NULL,
            MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if(err != MACH_MSG_SUCCESS) return 0;
    return 1;
}

int send_data(mach_port_t port, const typeinfo_t* info, char* data) {
    mach_msg_return_t err;
    struct message_full_header* hd = (struct message_full_header*)data;
    unsigned int size = info->size;
    memmove(data + sizeof(struct message_full_header), data, size);

    hd->head.msgh_bits = MACH_MSGH_BITS_REMOTE(get_send_right(port));
    hd->head.msgh_size = size + sizeof(struct message_full_header);
    hd->head.msgh_size = ((hd->head.msgh_size + 3) >> 2) << 2;
    hd->head.msgh_local_port = MACH_PORT_NULL;
    hd->head.msgh_remote_port = port;
    hd->head.msgh_id = info->id;

    hd->type.msgt_name = MACH_MSG_TYPE_UNSTRUCTURED;
    hd->type.msgt_size = 8;
    hd->type.msgt_number = info->size;
    hd->type.msgt_inline = TRUE;
    hd->type.msgt_longform = FALSE;
    hd->type.msgt_deallocate = FALSE;

    err = mach_msg( &hd->head, MACH_SEND_MSG,
            hd->head.msgh_size, 0, MACH_PORT_NULL,
            MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if(err != MACH_MSG_SUCCESS) return 0;
    return 1;
}

int receive_data_low(mach_port_t* port, mach_msg_header_t** tp, char* buffer, size_t size) {
    mach_msg_return_t err;
    struct message_full_header* hd = (struct message_full_header*)buffer;
    hd->head.msgh_size = size;
    err = mach_msg( &hd->head, MACH_RCV_MSG,
            0, hd->head.msgh_size, *port,
            MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if(err != MACH_MSG_SUCCESS) return 0;

    if(tp) *tp = &hd->head;
    if(hd->head.msgh_remote_port != MACH_PORT_NULL) {
        *port = hd->head.msgh_remote_port;
    }
    return 1;
}

int receive_data(mach_port_t* port, typeinfo_t* info, char* buffer, size_t size) {
    struct message_full_header* hd;
    if(!receive_data_low(port, (mach_msg_header_t**)&hd, buffer, size)) return 0;

    info->id     = hd->head.msgh_id;
    info->size   = (hd->type.msgt_size / 8) * hd->type.msgt_number;
    memmove(buffer, buffer + sizeof(struct message_full_header), info->size);
    return 1;
}

int send_port_right(mach_port_t port, mach_port_t rcv) {
    typeinfo_t tpinfo;
    mach_port_t buffer[64];

    tpinfo.id     = MACH_MSG_TYPE_MAKE_SEND;
    tpinfo.size   = sizeof(mach_port_t);
    buffer[0] = rcv;
    return send_data(port, &tpinfo, (char*)buffer);
}

