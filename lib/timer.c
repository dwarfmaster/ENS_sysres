
#include "timer.h"
#include "ports.h"
#include <pthread.h>
#include <stdio.h>

#define TIMER_HEAP_SIZE 1024
#define parent(i) ((i)/2)
#define rchild(i) (2*(i) + 1)
#define lchild(i) (2 * (i))
#define get_size(h) ((h)[0].end_time)
#define set_size(h, v) ((h)[0].end_time = (v))
struct timer {
    uint32_t end_time;
    int port;
    uintptr_t data;
};
typedef struct timer heap_element;

/******************************************************************************
 ************************ Heap implementation *********************************
 *****************************************************************************/
void bubble_up(heap_element* heap, size_t elem) {
    if(elem <= 1 || elem > get_size(heap)) return;

    if(heap[elem].end_time < heap[parent(elem)].end_time) {
        heap_element tmp = heap[elem];
        heap[elem] = heap[parent(elem)];
        heap[parent(elem)] = tmp;
        bubble_up(heap, parent(elem));
    }
}

void bubble_down(heap_element* heap, size_t elem) {
    if(elem < 1 || elem > get_size(heap)) return;

    uint32_t mn = heap[elem].end_time;
    size_t mni  = elem;
    if(rchild(elem) <= get_size(heap) && heap[rchild(elem)].end_time < mn) {
        mn  = heap[rchild(elem)].end_time;
        mni = rchild(elem);
    }
    if(lchild(elem) <= get_size(heap) && heap[lchild(elem)].end_time < mn) {
        mn  = heap[lchild(elem)].end_time;
        mni = lchild(elem);
    }

    if(mni == elem) return;
    heap_element tmp = heap[elem];
    heap[elem] = heap[mni];
    heap[mni]  = tmp;
    bubble_down(heap, mni);
}

int empty(heap_element* heap) {
    return get_size(heap) == 0;
}

int full(heap_element* heap) {
    return get_size(heap) + 1 == TIMER_HEAP_SIZE;
}

void push(heap_element* heap, heap_element elem) {
    size_t id = get_size(heap) + 1;
    set_size(heap, id);
    heap[id] = elem;
    bubble_up(heap, id);
}

void pop(heap_element* heap) {
    if(get_size(heap) <= 1) {
        set_size(heap, 0);
        return;
    }

    heap[1] = heap[get_size(heap)];
    set_size(heap, get_size(heap) - 1);
    bubble_down(heap, 1);
}

/******************************************************************************
 ************************ Timer thread ****************************************
 *****************************************************************************/
uint32_t get_time() {
    time_value_t time;
    host_get_time(mach_host_self(), &time);
    return time.seconds * 1000 + time.microseconds / 1000;
}

struct timer_thread_data {
    mach_port_t in, out;
};

void* timer_thread_main(void* data) {
    struct timer_thread_data* ports = (struct timer_thread_data*)data;
    heap_element heap[TIMER_HEAP_SIZE];
    set_size(heap, 0);
    char buffer[1024];
    mach_msg_header_t* hd = (mach_msg_header_t*)buffer;
    heap_element* msg = (heap_element*)
        (buffer + sizeof(mach_msg_header_t) + sizeof(mach_msg_type_t));
    mach_msg_return_t ret;
    typeinfo_t tpinfo;
    timer_message_t* out = (timer_message_t*)buffer;

    for(;;) {
        mach_msg_timeout_t timeout = MACH_MSG_TIMEOUT_NONE;
        mach_msg_option_t  option  = MACH_RCV_MSG;
        if(!empty(heap)) {
            uint32_t time = get_time();
            /* If it should already have been sent, do it now */
            if(heap[1].end_time < time) goto send_timeout;
            timeout = heap[1].end_time - time;
            option |= MACH_RCV_TIMEOUT;
        }

        mach_port_t in = ports->in;
        /* If the heap is full, do not accept any more incoming requests */
        if(full(heap)) in = MACH_PORT_NULL;

        hd->msgh_size = 1024;
        ret = mach_msg(hd, option,
                0, 1024, in,
                timeout, MACH_PORT_NULL);
        if(ret == MACH_MSG_SUCCESS) {
            /* Add new timer */
            push(heap, *msg);
            continue;
        }
        else if(ret != MACH_RCV_TIMED_OUT) continue;

send_timeout:
        do {
            tpinfo.id     = 0;
            tpinfo.size   = sizeof(timer_message_t);
            out->port = heap[1].port;
            out->data = heap[1].data;
            /* Try sending until it succeeds
             * TODO : make a failure count to prevent infinite loop
             */
        } while(!send_data(ports->out, &tpinfo, buffer));
        pop(heap);
    }

    return NULL;
}

tcp_timer_t start_timer_thread(mach_port_t port) {
    mach_port_t in;
    struct timer_thread_data* thdata;
    pthread_t thread;
    kern_return_t ret;
    int pret;

    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &in);
    if(ret != KERN_SUCCESS) return MACH_PORT_NULL;

    thdata = malloc(sizeof(struct timer_thread_data));
    if(!thdata) {
        mach_port_deallocate(mach_task_self(), in);
        return MACH_PORT_NULL;
    }
    thdata->in = in;
    thdata->out = port;

    pret = pthread_create(&thread, NULL, timer_thread_main, (void*)thdata);
    if(pret) {
        mach_port_deallocate(mach_task_self(), in);
        free(thdata);
        return MACH_PORT_NULL;
    }

    return in;
}

void add_timer(tcp_timer_t timer, uint32_t duration, int port, uintptr_t data) {
    typeinfo_t tpinfo;
    char buffer[256];
    struct timer* msg = (struct timer*)buffer;

    msg->end_time = get_time() + duration;
    msg->port     = port;
    msg->data     = data;
    tpinfo.id     = 0;
    tpinfo.size   = sizeof(struct timer);
    send_data(timer, &tpinfo, buffer);
}

