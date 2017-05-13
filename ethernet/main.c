
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
    struct device dev;
};

struct lvl32_data {
    struct mac_address addr;
    char* data;
};

void* demuxer_thread(void* vargs) {
    struct demuxer_args* args = (struct demuxer_args*)vargs;
    struct reserved2_data* rs2data;
    mach_port_t set, tmp;
    kern_return_t ret;
    ethernet_error_t err;
    char buffer[4096];
    size_t size;
    typeinfo_t tpinfo;
    struct eth_frame frame;
    struct lvl32_data* lvl32;
    int locked;

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

    ret = mach_port_move_member(mach_task_self(), args->dev.in, set);
    if(ret != KERN_SUCCESS) {
        log_string("Couldn't setup set");
        exit(EXIT_FAILURE);
    }

    locked = 0;
    while(1) {
        tmp = set;
        if(!receive_data(&set, &tpinfo, buffer, 4096)) continue;

        switch(tpinfo.id) {
            /* Data received from device thread */
            case lvl3_frame:
                if(locked) continue;
                err = decode_frame(buffer, tpinfo.size * tpinfo.number, &frame);
                if(err != ETH_SUCCESS) continue;
                dispatch(frame.ethertype, tpinfo.size, buffer);
                set = tmp;
                break;

            /* Data received from trivfs indicating lock */
            case reserved1:
                /* Acknowledge lock */
                tpinfo.id = reserved1;
                tpinfo.size = 0;
                tpinfo.number = 1;
                send_data(args->to_trivfs, &tpinfo, buffer);
                set = tmp;
                locked = 1;
                break;

            /* Data received from trivfs indicating end of a lock */
            case reserved2:
                rs2data = (struct reserved2_data*)buffer;
                mach_port_move_member(mach_task_self(), rs2data->nport, set);
                set = tmp;
                locked = 0;
                break;

            /* Data received from one of the level3 ports */
            case lvl32_frame:
                if(locked) continue;
                lvl32 = (struct lvl32_data*)buffer;
                frame.src       = args->dev.mac;
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
                send_data(args->dev.out, &tpinfo, buffer);

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
    /* TODO handle arguments */
    kern_return_t ret;
    ethernet_error_t err;
    int pret;
    pthread_t thread;
    struct demuxer_args args;

    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &args.to_trivfs);
    if(ret != KERN_SUCCESS) {
        log_string("Couldn't allocate to_trivfs port");
        exit(EXIT_FAILURE);
    }

    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &args.from_trivfs);
    if(ret != KERN_SUCCESS) {
        log_string("Couldn't allocate from_trivfs port");
        exit(EXIT_FAILURE);
    }

    err = open_file_device(&args.dev, "/dev/eth0");
    if(err != ETH_SUCCESS) {
        log_variadic("Couldn't open device /dev/eth0\n");
        exit(EXIT_FAILURE);
    }

    err = types_init("/home/hurd/ENS_sysres/arp/handlers", args.from_trivfs, args.to_trivfs, args.dev.mac);
    if(err != ETH_SUCCESS) {
        log_string("Couldn't init ethertypes map");
        exit(EXIT_FAILURE);
    }

    pret = pthread_create(&thread, NULL, demuxer_thread, (void*)&args);
    if(pret) {
        log_string("Couldn't create demuxer thread");
        exit(EXIT_FAILURE);
    }

    launch_registerer();
    return 0;
}

