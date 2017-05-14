
#ifndef DEF_LIB_PORTS
#define DEF_LIB_PORTS

#include <mach.h>
#include <stdlib.h>

/* Info about the type in the message. id can be arbitrarily
 * set : it isn't used during the transaction, only by the
 * receiver.
 * size and number are used to determine the size of the data
 * to send when sending.
 */
typedef struct typeinfo {
    unsigned int id;
    unsigned int size;   /* Size in bytes of the type */
} typeinfo_t;

/* Assumes there is space after data, ie it's in a buffer bigger than
 * info->size * info->number by at least 64 bytes
 * data may be ovewritten during the sending.
 */
int send_data(mach_port_t port, const typeinfo_t* info, char* data);
/* Send data of size size. The type header must already be included */
int send_data_low(mach_port_t port, size_t size, char* data, int id);
/* info is only set, not read here
 * If port is a port set, it is set to the real port from which the data came
 */
int receive_data(mach_port_t* port, typeinfo_t* info, char* buffer, size_t size);
/* hd is only set, it will point to the inside of buffer */
int receive_data_low(mach_port_t* port, mach_msg_header_t** tp, char* buffer, size_t size);
/* Send a send port right to port made from the receive right rcv */
int send_port_right(mach_port_t port, mach_port_t rcv);

#endif//DEF_LIB_PORTS

