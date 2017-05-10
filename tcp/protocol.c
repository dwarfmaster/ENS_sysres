
#include "protocol.h"
#include "tcp.h"
#include "proto.h"
#include "ports.h"

/* TODO use function send frame which use timer to check wether data must
 * be sent again */

void clean_frame(tcp_frame_t* fr) {
    fr->seq = fr->ack = 0;
    fr->window = fr->urgent = 0;
    fr->data = NULL;
    fr->flags.urg = fr->flags.ack = fr->flags.psh
        = fr->flags.rst = fr->flags.syn = fr->flags.fin = 0;
}

/* msg must be in a 2048-byte wide buffer */
void message_closed(tcp_connection_t* sock, char* msg, size_t size) {
    /* Nothing to do
       Avoid warnings */
    sock = sock;
    msg = msg;
    size = size;
}

void message_listen(tcp_connection_t* sock, char* msg, size_t size) {
    tcp_frame_t fr;
    tcp_frame_t rsp;
    typeinfo_t tpinfo;
    if(!read_frame(msg, size, &fr, sock->ipv6, sock->remote_addr, sock->local_addr)) return;

    if(fr.flags.ack) return;
    if(fr.flags.syn) {
        clean_frame(&rsp);
        rsp.src_port  = sock->local_port;
        rsp.dst_port  = sock->remote_port;
        rsp.ack       = fr.seq;
        rsp.seq       = fr.seq + 1;
        rsp.flags.ack = 1;
        rsp.flags.syn = 1;
        size          = 2048;
        if(!build_frame(msg, &size, &rsp, 0, sock->ipv6, sock->local_addr, sock->remote_addr)) return;
        
        tpinfo.id     = lvl4_frame;
        tpinfo.size   = size;
        tpinfo.number = 1;
        send_data(sock->ip_conn, &tpinfo, msg);
        sock->state   = SYN_RECEIVED;
    }
}

void message_syn_received(tcp_connection_t* sock, char* msg, size_t size) {
    tcp_frame_t fr;
    if(!read_frame(msg, size, &fr, sock->ipv6, sock->remote_addr, sock->local_addr)) return;

    if(fr.flags.rst) {
        sock->state = LISTEN;
        return;
    }

    if(fr.flags.ack) {
        sock->state = ESTABLISHED;
        return;
    }
}

void message_syn_sent(tcp_connection_t* sock, char* msg, size_t size) {
    tcp_frame_t fr;
    tcp_frame_t rsp;
    typeinfo_t tpinfo;
    if(!read_frame(msg, size, &fr, sock->ipv6, sock->remote_addr, sock->local_addr)) return;

    if(fr.flags.syn && !fr.flags.ack) {
        clean_frame(&rsp);
        rsp.src_port  = sock->local_port;
        rsp.dst_port  = sock->remote_port;
        rsp.ack       = fr.seq;
        rsp.seq       = fr.seq + 1;
        rsp.flags.ack = 1;
        rsp.flags.syn = 1;
        size          = 2048;
        if(!build_frame(msg, &size, &rsp, 0, sock->ipv6, sock->local_addr, sock->remote_addr)) return;
        
        tpinfo.id     = lvl4_frame;
        tpinfo.size   = size;
        tpinfo.number = 1;
        send_data(sock->ip_conn, &tpinfo, msg);
        sock->state   = SYN_RECEIVED;
        return;
    }

    if(fr.flags.syn && fr.flags.ack) {
        clean_frame(&rsp);
        rsp.src_port  = sock->local_port;
        rsp.dst_port  = sock->remote_port;
        rsp.ack       = fr.seq;
        rsp.flags.ack = 1;
        size          = 2048;
        if(!build_frame(msg, &size, &rsp, 0, sock->ipv6, sock->local_addr, sock->remote_addr)) return;
        
        tpinfo.id     = lvl4_frame;
        tpinfo.size   = size;
        tpinfo.number = 1;
        send_data(sock->ip_conn, &tpinfo, msg);
        sock->state   = ESTABLISHED;
        return;
    }
}

void message_fin_wait_1(tcp_connection_t* sock, char* msg, size_t size) {
    tcp_frame_t fr;
    tcp_frame_t rsp;
    typeinfo_t tpinfo;
    if(!read_frame(msg, size, &fr, sock->ipv6, sock->remote_addr, sock->local_addr)) return;

    if(fr.flags.ack && !fr.flags.fin) {
        sock->state = FIN_WAIT_2;
        return;
    }

    if(fr.flags.fin) {
        clean_frame(&rsp);
        rsp.src_port  = sock->local_port;
        rsp.dst_port  = sock->remote_port;
        rsp.ack       = fr.seq;
        rsp.flags.ack = 1;
        size          = 2048;
        if(!build_frame(msg, &size, &rsp, 0, sock->ipv6, sock->local_addr, sock->remote_addr)) return;
        
        tpinfo.id     = lvl4_frame;
        tpinfo.size   = size;
        tpinfo.number = 1;
        send_data(sock->ip_conn, &tpinfo, msg);

        /* TODO start timer for time wait */
        if(fr.flags.ack) sock->state = TIME_WAIT;
        else             sock->state = CLOSING;
    }
}

void message_fin_wait_2(tcp_connection_t* sock, char* msg, size_t size) {
    tcp_frame_t fr;
    tcp_frame_t rsp;
    typeinfo_t tpinfo;
    if(!read_frame(msg, size, &fr, sock->ipv6, sock->remote_addr, sock->local_addr)) return;

    if(fr.flags.fin) {
        clean_frame(&rsp);
        rsp.src_port  = sock->local_port;
        rsp.dst_port  = sock->remote_port;
        rsp.ack       = fr.seq;
        rsp.flags.ack = 1;
        size          = 2048;
        if(!build_frame(msg, &size, &rsp, 0, sock->ipv6, sock->local_addr, sock->remote_addr)) return;
        
        tpinfo.id     = lvl4_frame;
        tpinfo.size   = size;
        tpinfo.number = 1;
        send_data(sock->ip_conn, &tpinfo, msg);
        /* TODO Start timer for time wait */
        sock->state   = TIME_WAIT;
    }
}

void message_closing(tcp_connection_t* sock, char* msg, size_t size) {
    tcp_frame_t fr;
    if(!read_frame(msg, size, &fr, sock->ipv6, sock->remote_addr, sock->local_addr)) return;

    if(fr.flags.ack) {
        sock->state = TIME_WAIT;
        return;
    }
}

void message_time_wait(tcp_connection_t* sock, char* msg, size_t size) {
    /* Nothing to do, wait for timer to end */
    sock = sock;
    msg  = msg;
    size = size;
}

void message_close_wait(tcp_connection_t* sock, char* msg, size_t size) {
    /* Nothing to do, wait for close order from user */
    sock = sock;
    msg  = msg;
    size = size;
}

void message_last_ack(tcp_connection_t* sock, char* msg, size_t size) {
    tcp_frame_t fr;
    if(!read_frame(msg, size, &fr, sock->ipv6, sock->remote_addr, sock->local_addr)) return;

    if(fr.flags.ack) {
        sock->state = CLOSED;
        return;
    }
}

void message(tcp_connection_t* sock, char* msg, size_t size) {
    switch(sock->state) {
        case CLOSED:       return message_closed       (sock, msg, size);
        case LISTEN:       return message_listen       (sock, msg, size);
        case SYN_RECEIVED: return message_syn_received (sock, msg, size);
        case SYN_SENT:     return message_syn_sent     (sock, msg, size);
        /* TODO ESTABLISHED */
        case FIN_WAIT_1:   return message_fin_wait_1   (sock, msg, size);
        case FIN_WAIT_2:   return message_fin_wait_2   (sock, msg, size);
        case CLOSING:      return message_closing      (sock, msg, size);
        case TIME_WAIT:    return message_time_wait    (sock, msg, size);
        case CLOSE_WAIT:   return message_close_wait   (sock, msg, size);
        case LAST_ACK:     return message_last_ack     (sock, msg, size);
        /* Invalid state value, assume CLOSED */
        default:           sock->state = CLOSED;
    }
}

