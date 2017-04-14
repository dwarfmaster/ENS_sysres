
#ifndef DEF_ETHERNET_DEVICE
#define DEF_ETHERNET_DEVICE

#include <mach.h>
#include <hurd.h>
#include <pthread.h>
#include "ethernet.h"
#include "types.h"

struct device {
    pthread_t thread;
    /* Send data to this port to send it from the device */
    mach_port_t out;
    /* Read from this port to receive frames */
    mach_port_t in;
    /* The address of the device */
    struct mac_address mac;
};

ethernet_error_t open_file_device(struct device* dev, const char* path);
ethernet_error_t open_mach_device(struct device* dev, const char* name);
ethernet_error_t open_dummy_device(struct device* dev);

#endif//DEF_ETHERNET_DEVICE

