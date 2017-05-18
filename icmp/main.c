
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hurd.h>
#include <hurd/trivfs.h>
#include "logging.h"
#include "ports.h"
#include "proto.h"
#include "timer.h"
#include <fcntl.h>

mach_port_t ip_port;

/* TrivFS symbols */
int trivfs_fstype        = FSTYPE_MISC;
int trivfs_fsid          = 0;
int trivfs_allow_open    = O_READ | O_WRITE;
int trivfs_support_read  = 0;
int trivfs_support_write = 0;
int trivfs_support_exec  = 0;

/* Misc necessary trivfs callbacks */
void trivfs_modify_stat(struct trivfs_protid* cred, io_statbuf_t* st) {
    cred = cred; /* Fix warnings */
    st = st; /* Fix warnings */
    /* Do nothing */
}

error_t trivfs_S_file_set_size(struct trivfs_protid* cred, off_t size) {
    size = size; /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_seek(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t reply_type, off_t offs, int whence, off_t* new_offs) {
    reply      = reply;      /* Fix warnings */
    reply_type = reply_type; /* Fix warnings */
    offs       = offs;       /* Fix warnings */
    whence     = whence;     /* Fix warnings */
    new_offs   = new_offs;   /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_select(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* type, int* tag) {
    reply     = reply;     /* Fix warnings */
    replytype = replytype; /* Fix warnings */
    type      = type;      /* Fix warnings */
    tag       = tag;       /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_get_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    reply     = reply;     /* Fix warnings */
    replytype = replytype; /* Fix warnings */
    bits      = bits;      /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_set_all_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    reply     = reply;     /* Fix warnings */
    replytype = replytype; /* Fix warnings */
    bits      = bits;      /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_set_some_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    reply     = reply;     /* Fix warnings */
    replytype = replytype; /* Fix warnings */
    bits      = bits;      /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_clear_some_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    reply     = reply;     /* Fix warnings */
    replytype = replytype; /* Fix warnings */
    bits      = bits;      /* Fix warnings */
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_goaway(struct trivfs_control* cntl, int flags) {
    cntl  = cntl;  /* Fix warnings */
    flags = flags; /* Fix warnings */
    exit(EXIT_SUCCESS);
    return 0;
}

int main() {
    error_t err;
    kern_return_t ret;
    mach_port_t bootstrap;
    struct trivfs_control* fsys;
    char buffer[1 << 11];
    char ping_ip[4];

//    for(int i = 0; i < 4; myip[i++] = 0x00);
    ping_ip[0] = 192;
    ping_ip[1] = 168;
    ping_ip[2] = 42;
    ping_ip[3] = 143;

    task_get_bootstrap_port(mach_task_self(), &bootstrap);
    if(bootstrap == MACH_PORT_NULL) {
        log_string("Must be started as a translator");
        return 1; // INVALID
    }

    err = trivfs_startup(bootstrap, 0, 0, 0, 0, 0, &fsys);
    if(err) {
        log_string("Couldn't setup translator");
        return 1; // IO
    }

    ip_port = file_name_lookup("./0800", O_READ | O_WRITE, 0);
    if(ip_port == MACH_PORT_NULL) {
        fprintf(stderr, "Couldn't open ipv4 port\n");
        return 1;
    }

    typeinfo_t tpinfo;
    tpinfo.id = lvl4_frame;
    tpinfo.size = 12;
    buffer[0]  = ping_ip[0]; // ip to ping 1
    buffer[1]  = ping_ip[1]; // ip to ping 2
    buffer[2]  = ping_ip[2]; // ip to ping 3
    buffer[3]  = ping_ip[3]; // ip to ping 4
    buffer[4]  = 0x08; // Type
    buffer[5]  = 0x00; // Code
    buffer[6]  = 0x73; // checksum 1 = 8C84
    buffer[7]  = 0x7b; // checksum 2
    buffer[8]  = 0x42; // header data 1 identifier 1
    buffer[9]  = 0x42; // header data 2 identifier 2
    buffer[10] = 0x42; // header data 3 sequence number 1
    buffer[11] = 0x42; // header data 4 sequence number 2

    int errsd = send_data(ip_port, &tpinfo, buffer);
    return 0; // SUCCESS
}

