
#ifndef DEF_TCP_TIMER
#define DEF_TCP_TIMER

#include <stdint.h>
#include <hurd.h>

typedef mach_port_t tcp_timer_t;

typedef struct timer_message {
    int port;
    uintptr_t data;
} timer_message_t;

/* port is the port to which end of timer message are sent.
 * These messages are simplt a timer_message_t */
tcp_timer_t start_timer_thread(mach_port_t port);
void add_timer(tcp_timer_t timer, uint32_t duration, int port, uintptr_t data);

#endif//DEF_TCP_TIMER

