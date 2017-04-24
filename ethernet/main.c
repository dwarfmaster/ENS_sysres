
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

struct lvl32_data {
    struct mac_address addr;
    char* data;
};

void* demuxer_thread(void* vargs) {
    struct demuxer_args* args = (struct demuxer_args*)vargs;
    struct device dev;
    struct reserved2_data* rs2data;
    mach_port_t set, set2, tmp;
    kern_return_t ret;
    ethernet_error_t err;
    char buffer[4096];
    size_t size;
    typeinfo_t tpinfo;
    struct eth_frame frame;
    struct lvl32_data* lvl32;
    struct mac_address src; /* TODO discover local mac address */

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
        tmp = set;
        if(!receive_data(&set, &tpinfo, buffer, 4096)) continue;

        switch(tpinfo.id) {
            /* Data received from device thread */
            case lvl2_frame:
                err = decode_frame(buffer, tpinfo.size, &frame);
                if(err != ETH_SUCCESS) continue;
                dispatch(frame.ethertype, frame.size, frame.data);
                set = tmp;
                break;

            /* Data received from trivfs indicating lock */
            case reserved1:
                tmp = set;
                set = set2;
                set2 = tmp;
                /* Acknowledge lock */
                tpinfo.id = reserved1;
                tpinfo.size = 0;
                tpinfo.number = 1;
                send_data(args->to_trivfs, &tpinfo, buffer);
                break;

            /* Data received from trivfs indicating end of a lock */
            case reserved2:
                tmp = set;
                set = set2;
                set2 = tmp;
                rs2data = (struct reserved2_data*)buffer;
                mach_port_move_member(mach_task_self(), rs2data->nport, set);
                break;

            /* Data received from one of the level3 ports */
            case lvl3_frame:
                lvl32 = (struct lvl32_data*)buffer;
                frame.src       = src;
                frame.dst       = lvl32->addr;
                frame.ethertype = lookup_type(set);
                frame.size      = tpinfo.size - sizeof(struct mac_address);
                frame.data      = lvl32->data;

                size = 4096;
                err = make_frame(&frame, buffer, &size);
                if(err != ETH_SUCCESS) {
                    log_variadic("Couldn't send frame of type %4X\n", frame.ethertype);
                    continue;
                }
                tpinfo.id     = lvl2_frame;
                tpinfo.size   = size;
                tpinfo.number = 1;
                send_data(dev.out, &tpinfo, buffer);

                set = tmp;
                break;

            default:
                log_variadic("Main thread received invalid type id : %d\n", tpinfo.id);
                set = tmp;
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

