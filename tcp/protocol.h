
#ifndef DEF_TCP_PROTOCOL
#define DEF_TCP_PROTOCOL

#define TCP_BUFFER_SIZE 4096

#include <stdint.h>
#include <pthread.h>
#include <hurd.h>
#include "timer.h"

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

#define TCP_SENT_HISTORY_SIZE 30
enum tcp_timer_action {
    TIMER_CLOSE,          /* TIME_WAIT    -> CLOSED       */
    TIMER_ACK,            /* ESTABLISHED  -> ESTABLISHED  */
    TIMER_RESEND_DATA,    /* ESTABLISHED  -> ESTABLISHED  */
    TIMER_RESEND_ACK_SYN, /* SYN_RECIEVED -> SYN_RECEIVED */
};
struct tcp_sent {
    enum tcp_timer_action action;
    uint32_t seq;
    uint32_t size;
    int used;
};

typedef struct tcp_connection {
    uint16_t local_port, remote_port;
    char *local_addr, *remote_addr;
    tcp_state_t state;
    int ipv6;
    pthread_t thread;
    mach_port_t ip_conn;
    tcp_timer_t timer;

    struct tcp_sent history[TCP_SENT_HISTORY_SIZE];
    uint32_t send_seq;
    uint32_t sent_size;
    /* Add to_send_size */
    uint32_t remote_window;
    char send_buffer[TCP_BUFFER_SIZE];

    uint32_t receive_seq;
    uint32_t receive_size;
    int must_ack;
    char receive_buffer[TCP_BUFFER_SIZE];
} tcp_connection_t;

void message(tcp_connection_t* sock, char* msg, size_t size);
void end_timer(tcp_connection_t* sock, uintptr_t data);
/* TODO socket interface */

#endif//DEF_TCP_PROTOCOL

