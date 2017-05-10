
#include "timer.h"
#include "ports.h"

#define TIMER_HEAP_SIZE 1024
#define parent(i) ((i)/2)
#define rchild(i) (2*(i) + 1)
#define lchild(i) (2 * (i))
#define get_size(h) ((h)[0].end_time)
#define set_size(h, v) ((h)[0].end_time = (v))
struct timer {
    uint32_t end_time;
    int port;
    char* data;
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
    return time.seconds + time.microseconds * 1000;
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
        if(full(heap)) {
            /* Only timeout : we won't receive timers we can't handle */
            /* TODO */
            goto send_timeout;
        } else {
            mach_msg_timeout_t timeout = MACH_MSG_TIMEOUT_NONE;
            if(!empty(heap)) {
                uint32_t time = get_time();
                if(heap[1].end_time < time) goto send_timeout;
                timeout = heap[1].end_time - time;
            }
            hd->msgh_size = 1024;
            ret = mach_msg(hd, MACH_RCV_TIMEOUT,
                    0, 1024, ports->in,
                    timeout, MACH_PORT_NULL);
            if(ret == MACH_RCV_TIMED_OUT) goto send_timeout;
            if(ret != MACH_MSG_SUCCESS) continue;
        }

        /* Add new timer */
        push(heap, *msg);
        continue;

send_timeout:
        tpinfo.id     = 0;
        tpinfo.size   = sizeof(timer_message_t);
        tpinfo.number = 1;
        out->port = heap[1].port;
        out->data = heap[1].data;
        if(!send_data(ports->out, &tpinfo, buffer)) goto send_timeout;
    }

    return NULL;
}

tcp_timer_t start_timer_thread(mach_port_t port) {
    /* TODO */
}

void add_timer(tcp_timer_t timer, uint32_t duration, int port, uintptr_t* data) {
    /* TODO */
}

