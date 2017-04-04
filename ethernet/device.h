
#ifndef DEF_ETHERNET_DEVICE
#define DEF_ETHERNET_DEVICE

#include <mach.h>
#include <hurd.h>
#include "types.h"

struct device {
    void* dev;
    ethernet_error_t (*write)(void*, void*, size_t*); // dev data size
    ethernet_error_t (*read)(void*, void*, size_t*);
    ethernet_error_t (*close)(void*);
};

ethernet_error_t open_file_device(struct device* dev, const char* path);
ethernet_error_t open_mach_device(struct device* dev, const char* name);

#endif//DEF_ETHERNET_DEVICE

