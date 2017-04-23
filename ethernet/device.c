
#include "device.h"
#include "ethernet.h"
#include "ports.h"
#include "proto.h"
#include "logging.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <pthread.h>
#include <hurd.h>
#include <hurd/io.h>

/****************************************************************
 *********************** File Device ****************************
 ****************************************************************/
struct file_device_main_data {
    file_t fd;
    mach_port_t in;
    mach_port_t sel; /* Sel is used for select queries on fd */
    mach_port_t set; /* Contains sel and in */
    mach_port_t out;
};

void* file_device_main(void* data) {
    char buffer[4096];
    char* buf;
    size_t size, rd, wr;
    struct file_device_main_data* params = data;
    typeinfo_t tpinfo;

    while(1) {
        __io_select_request(params->fd, params->sel, SELECT_READ);
        if(!receive_data(params->set, &tpinfo, buffer, 4096)) continue;

        switch(tpinfo.id) {
            case lvl1_frame: /* type id from select */
                buf = buffer;
                io_read(params->fd, &buf, &size, -1, 4096);
                tpinfo.number = 1;
                tpinfo.size   = size;
                tpinfo.id     = 42; /* TODO define code for ethernet frame */
                send_data(params->out, &tpinfo, buf);
                break;
            case lvl2_frame:
                size = tpinfo.size * tpinfo.number;
                rd = 0;
                while(size > 0) {
                    io_write(params->fd, buffer + rd, size, -1, &wr);
                    rd   += wr;
                    size -= wr;
                }
                break;
            default:
                log_variadic("Device thread received invalid type id : %d\n", tpinfo.id);
                break;
        }
    }
}

ethernet_error_t open_file_device(struct device* dev, const char* path) {
    struct file_device_main_data* data;
    kern_return_t ret;
    int pret;
    ethernet_error_t err;
    data      = NULL;

    data = malloc(sizeof(struct file_device_main_data));
    if(!data) {
        err = ETH_AGAIN;
        goto err;
    }

    data->fd  = MACH_PORT_NULL;
    data->in  = MACH_PORT_NULL;
    data->sel = MACH_PORT_NULL;
    data->set = MACH_PORT_NULL;
    data->out = MACH_PORT_NULL;
    err = ETH_IO;

    data->fd = file_name_lookup(path, O_READ | O_WRITE, 0);
    if(data->fd == MACH_PORT_NULL) goto err;

    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &data->in);
    if(ret != KERN_SUCCESS) goto err;
    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &data->sel);
    if(ret != KERN_SUCCESS) goto err;

    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &data->set);
    if(ret != KERN_SUCCESS) goto err;
    err = mach_port_move_member(mach_task_self(), data->sel, data->set);
    if(ret != KERN_SUCCESS) goto err;
    err = mach_port_move_member(mach_task_self(), data->fd, data->set);
    if(ret != KERN_SUCCESS) goto err;

    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &data->out);
    if(ret != KERN_SUCCESS) goto err;

    dev->out = data->in;
    dev->in  = data->out;
    /* TODO find MAC address */

    pret = pthread_create(&dev->thread, NULL, file_device_main, (void*)data);
    if(pret) {
        err = ETH_AGAIN;
        goto err;
    }

    return ETH_SUCCESS;

err:
    if(data) {
        if(data->fd  != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), data->fd);
        if(data->in  != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), data->in);
        if(data->out != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), data->out);
        if(data->set != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), data->sel);
        if(data->set != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), data->set);
        free(data);
    }
    return err;
}

/****************************************************************
 *********************** Mach Device ****************************
 ****************************************************************/
ethernet_error_t open_mach_device(struct device* dev, const char* name) {
    /* TODO */
}

/****************************************************************
 *********************** Dummy Device ***************************
 ****************************************************************/
ethernet_error_t dummy_write(void* dev, void* data, size_t* size) {
    printf("Sending message of size %u\n", *size);
    return ETH_SUCCESS;
}

static char buffer[2048];
ethernet_error_t dummy_read(void* dev, void* data, size_t* size) {
    struct eth_frame fr;

    printf("Source: ");
    scanf("%s", buffer);
    read_mac_address(buffer, &fr.src);
    printf("Destination: ");
    scanf("%s", buffer);
    read_mac_address(buffer, &fr.dst);

    printf("Ethertype: ");
    scanf("%hX", &fr.ethertype);
    printf("Data: ");
    scanf("%s", buffer);

    fr.size = strlen(buffer);
    *size   = fr.size;
    fr.data = buffer;
    return make_frame(&fr, data, *size);
}

ethernet_error_t dummy_close(void* dev) {
    return ETH_SUCCESS;
}

ethernet_error_t open_dummy_device(struct device* dev) {
    /*
    dev->dev   = NULL;
    dev->write = dummy_write;
    dev->read  = dummy_read;
    dev->close = dummy_close;
    */
    return ETH_SUCCESS;
}

