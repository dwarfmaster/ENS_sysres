
#include "device.h"
#include "ethernet.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/****************************************************************
 *********************** File Device ****************************
 ****************************************************************/
ethernet_error_t file_write(void* pfd, void* data, size_t* size) {
    int fd = (int)pfd;
    ssize_t written;

    written = write(fd, data, *size);
    if(written == -1) {
        *size = 0;
        switch(errno) {
            case EAGAIN:
            case EINTR:
                return ETH_AGAIN;
            case EIO:
            case EFBIG:
            case ENOSPC:
            case EPIPE:
                return ETH_IO;
            case EBADF:
            case EFAULT:
            case EINVAL:
            default:
                return ETH_INVALID;
        }
    }

    *size = written;
    return ETH_SUCCESS;
}

ethernet_error_t file_read(void* pfd, void* data, size_t* size) {
    int fd = (int)pfd;
    ssize_t rd;

    rd = read(fd, data, *size);
    if(rd == -1) {
        *size = 0;
        switch(errno) {
            case EAGAIN:
            case EINTR:
                return ETH_AGAIN;
            case EIO:
                return ETH_IO;
            case EBADF:
            case EISDIR:
            case EFAULT:
            case EINVAL:
            default:
                return ETH_INVALID;
        }
    }

    *size = rd;
    return ETH_SUCCESS;
}

ethernet_error_t file_close(void* pfd) {
    int fd = (int)pfd;
    close(fd);
    return ETH_SUCCESS;
}

ethernet_error_t open_file_device(struct device* dev, const char* path) {
    int fd;

    fd = open(path, O_RDWR | O_APPEND);
    if(fd == -1) {
        switch(errno) {
            case EWOULDBLOCK:
                return ETH_AGAIN;
            case ELOOP:
            case EMFILE:
            case ENFILE:
            case ENODEV:
            case ENOMEM:
            case ENOSPC:
            case ENXIO:
            case EOVERFLOW:
                return ETH_IO;
            case EROFS:
            case ETXTBSY:
            case EPERM:
            case ENOTDIR:
            case ENOENT:
            case ENAMETOOLONG:
            case EACCES:
            case EEXIST:
            case EFAULT:
            case EISDIR:
            default:
                return ETH_INVALID;
        }
    }

    dev->dev   = (void*)fd;
    dev->write = file_write;
    dev->read  = file_read;
    dev->close = file_close;
    return ETH_SUCCESS;
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
    dev->dev   = NULL;
    dev->write = dummy_write;
    dev->read  = dummy_read;
    dev->close = dummy_close;
    return ETH_SUCCESS;
}

