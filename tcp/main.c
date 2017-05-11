
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "timer.h"
#include "ports.h"

int main(int argc, char *argv[]) {
    int prt, time;
    mach_port_t port;
    tcp_timer_t timer;
    char buffer[4096];
    typeinfo_t tpinfo;
    timer_message_t* msg = (timer_message_t*)buffer;

    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    timer = start_timer_thread(port);

    while(1) {
        scanf("%d", &prt);
        scanf("%d", &time);
        if(time == 0) break;
        add_timer(timer, time, prt, (uintptr_t)(argv[prt % argc]));
    }

    while(1) {
        if(!receive_data(&port, &tpinfo, buffer, 1024)) continue;
        printf("Received : %d -> \"%s\"\n", msg->port, (char*)msg->data);
        if(msg->port == 42) break;
    }

    return 0;
}

