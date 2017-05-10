
#ifndef DEF_TCP_PROTOCOL
#define DEF_TCP_PROTOCOL

#define TCP_BUFFER_SIZE 4096

#include <stdint.h>
#include <pthread.h>
#include <hurd.h>

typedef enum tcp_state {
    CLOSED,
    LISTEN,
    SYN_RECEIVED,
    SYN_SENT,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSING,
    TIME_WAIT,
    CLOSE_WAIT,
    LAST_ACK,
    NUMBER_STATE
} tcp_state_t;

typedef struct tcp_connection {
    uint16_t local_port, remote_port;
    char *local_addr, *remote_addr;
    tcp_state_t state;
    int ipv6;
    pthread_t thread;
    mach_port_t ip_conn;

    char send_buffer[TCP_BUFFER_SIZE];
    char receive_buffer[TCP_BUFFER_SIZE];
} tcp_connection_t;

void message(tcp_connection_t* sock, char* msg, size_t size);
/* TODO socket interface */

#endif//DEF_TCP_PROTOCOL

