
#include "ethertypes.h"
#include "logging.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <hurd.h>
#include <hurd/io.h>
#include <hurd/trivfs.h>

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

/* TivFS symbols */
int trivfs_fstype = FSTYPE_MISC;
int trivfs_fsid = 0;
int trivfs_allow_open = O_WRITE;
int trivfs_support_read  = 1;
int trivfs_support_write = 1;
int trivfs_support_exec  = 0;

/* The file has nothing to read */
error_t trivfs_S_io_read (struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t reply_type, vm_address_t* data,
        mach_msg_type_number_t* data_len, off_t offs,
        mach_msg_type_number_t aount) {
    if(!cred)                                 return EOPNOTSUPP;
    else if (!(cred->po->openmodes & O_READ)) return EBADF;
    *data_len = 0;
    return 0;
}
error_t trivfs_S_io_readable(struct trivfs_protid* cred,
        mach_port_t reply, mach_msg_type_name_t replytype,
        mach_msg_type_number_t* amount) {
    if(!cred)                                 return EOPNOTSUPP;
    else if (!(cred->po->openmodes & O_READ)) return EINVAL;
    *amount = 0;
    return 0;
}

/* Register a new ethertype.
 * Ignore offset
 */
static char buffer[5];
static int pos = 0;
error_t trivfs_S_io_write (struct trivfs_protid* cred,
        mach_port_t reply, mach_msg_type_name_t replytype,
        vm_address_t data, mach_msg_type_number_t datalen,
        off_t offs, mach_msg_type_number_t* amount) {
    if(!cred)                                  return EOPNOTSUPP;
    else if (!(cred->po->openmodes & O_WRITE)) return EBADF;

    char* wr = (char*)data;
    size_t i = 0, rd;
    unsigned int tp;
    ethernet_error_t err;
    while(i < datalen) {
        buffer[(i + pos) % 5] = wr[i];
        ++i;

        if((i + pos) % 5 == 0) {
            if(buffer[4] != '\n') continue;
            buffer[4] = 0;
            rd = sscanf(buffer, "%4X", &tp);
            if(rd == 1) {
                err = types_register((uint16_t)tp);
                if(err != ETH_SUCCESS) {
                    log_variadic("Failed to register %4X\n", tp);
                }
            }
        }
    }
    pos = (pos + datalen) % 5;
    return 0;
}

/* Misc necessary trivfs callbacks */
void trivfs_modify_stat(struct trivfs_protid* cred, io_statbuf_t* st) {
    /* Do nothing */
}

error_t trivfs_S_file_set_size(struct trivfs_protid* cred, off_t size) {
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_seek(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t reply_type, off_t offs, int whence, off_t* new_offs) {
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_select(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* type, int* tag) {
    if(!cred) return EOPNOTSUPP;
    if(((*type & SELECT_READ) && !(cred->po->openmodes & O_READ))
            || ((*type & SELECT_WRITE) && !(cred->po->openmodes & O_WRITE))) {
        return EBADF;
    }

    *type &= ~SELECT_URG;
    return 0;
}

error_t trivfs_S_io_get_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    if(!cred) return EOPNOTSUPP;
    *bits = cred->po->openmodes;
    return 0;
}

error_t trivfs_S_io_set_all_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_set_some_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_S_io_clear_some_openmodes(struct trivfs_protid* cred, mach_port_t reply,
        mach_msg_type_name_t replytype, int* bits) {
    if(!cred) return EOPNOTSUPP;
    else      return 0;
}

error_t trivfs_goaway(struct trivfs_control* cntl, int flags) {
    exit(EXIT_SUCCESS);
    return 0;
}

ethernet_error_t launch_registerer() {
    error_t err;
    mach_port_t bootstrap;
    struct trivfs_control* fsys;

    task_get_bootstrap_port(mach_task_self(), &bootstrap);
    if(bootstrap == MACH_PORT_NULL) {
        log_string("Must be started as a translator");
        return ETH_INVALID;
    }

    err = trivfs_startup(bootstrap, 0, 0, 0, 0, 0, &fsys);
    if(err) {
        log_string("Couldn't setup translator");
        return ETH_IO;
    }

    ports_manage_port_operations_one_thread(fsys->pi.bucket,
            trivfs_demuxer, 0);
    return 0;
}


