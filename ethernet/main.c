
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hurd.h>
#include "ethertypes.h"
#include "ethernet.h"
#include "device.h"
#include "logging.h"
#include "ports.h"
#include "proto.h"

struct demuxer_args {
    mach_port_t from_trivfs;
    mach_port_t to_trivfs;
    const char* dev;
};

void* demuxer_thread(void* vargs) {
    struct demuxer_args* args = (struct demuxer_args*)vargs;
    struct device dev;
    mach_port_t set, set2, tmp;
    kern_return_t ret;
    ethernet_error_t err;
    char buffer[4096];
    size_t size;
    typeinfo_t tpinfo;

    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &set);
    if(ret != KERN_SUCCESS) {
        log_string("Allocation of main port set failed");
        exit(EXIT_FAILURE);
    }
    ret = mach_port_move_member(mach_task_self(), args->from_trivfs, set);
    if(ret != KERN_SUCCESS) {
        log_string("Couldn't setup set");
        exit(EXIT_FAILURE);
    }

    err = open_file_device(&dev, args->dev);
    if(err != ETH_SUCCESS) {
        log_variadic("Couldn't open device %s\n", args->dev);
        exit(EXIT_FAILURE);
    }
    ret = mach_port_move_member(mach_task_self(), dev.in, set);
    if(ret != KERN_SUCCESS) {
        log_string("Couldn't setup set");
        exit(EXIT_FAILURE);
    }

    set2 = args->from_trivfs;
    while(1) {
        if(!receive_data(set, &tpinfo, buffer, 4096)) continue;

        switch(tpinfo.id) {
            /* Data received from device thread */
            case lvl2_frame:
                /* TODO */
                break;

            /* Data received from trivfs indicating lock */
            case reserved1:
                tmp = set;
                set = set2;
                set2 = tmp;
                /* TODO */
                break;

            /* Data received from trivfs indicating end of a lock */
            case reserved2:
                tmp = set;
                set = set2;
                set2 = tmp;
                break;

            /* Data received from one of the level3 ports */
            case lvl3_frame:
                /* TODO */
                break;

            default:
                log_variadic("Main thread received invalid type id : %d\n", tpinfo.id);
                break;
        }
    }
}

/* Create another thread for muxing and handling the device,
 * and launch trivfs on main thread
 */
int main(int argc, char *argv[]) {
    launch_registerer();
    return 0;
}

