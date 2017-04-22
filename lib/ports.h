
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
    unsigned int number; /* Number of elements of that type */
} typeinfo_t;

/* Assumes there is space after data, ie it's in a buffer bigger than
 * info->size * info->number by at least 64 bytes
 * data may be ovewritten during the sending.
 */
int send_data(mach_port_t port, const typeinfo_t* info, char* data);
/* info is only set, not read here */
int receive_data(mach_port_t port, typeinfo_t* info, char* buffer, size_t size);

#endif//DEF_LIB_PORTS

