
#include "protocol.h"
#include "tcp.h"
#include "proto.h"
#include "ports.h"
#include <string.h>

void tcp_add_timer(tcp_connection_t* sock, uint32_t duration,
        enum tcp_timer_action action,
        uint32_t seq, uint32_t size,
        int used) {
    size_t i;
    for(i = 0; i < TCP_SENT_HISTORY_SIZE; ++i) {
        if(!sock->history[i].used) {
            sock->history[i].action = action;
            sock->history[i].seq    = seq;
            sock->history[i].size   = size;
            sock->history[i].used   = used;
            add_timer(sock->timer, duration, sock->local_port, i);
            return;
        }
    }
}

void reset(tcp_connection_t* sock) {
    size_t i;

    sock->local_port = sock->remote_port = 0;
    free(sock->local_addr);
    free(sock->remote_addr);
    sock->local_addr = sock->remote_addr = NULL;

    sock->send_seq = sock->sent_size = 0;
    sock->receive_seq = sock->receive_size = 0;
    sock->must_ack = 0;

    for(i = 0; i < TCP_SENT_HISTORY_SIZE; ++i) {
        sock->history[i].used = 0;
    }
}

void clean_frame(tcp_frame_t* fr) {
    fr->seq = fr->ack = 0;
    fr->window = fr->urgent = 0;
    fr->data = NULL;
    fr->flags.urg = fr->flags.ack = fr->flags.psh
        = fr->flags.rst = fr->flags.syn = fr->flags.fin = 0;
}

/******************************************************************************
 ************************ Message reception ***********************************
 ******************************************************************************/

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
        rsp.src_port      = sock->local_port;
        rsp.dst_port      = sock->remote_port;
        rsp.ack           = fr.seq;
        rsp.seq           = fr.seq + 1;
        rsp.flags.ack     = 1;
        rsp.flags.syn     = 1;
        size              = 2048;
        sock->receive_seq = fr.seq;
        if(!build_frame(msg, &size, &rsp, 0, sock->ipv6, sock->local_addr, sock->remote_addr)) return;
        
        tpinfo.id     = lvl4_frame;
        tpinfo.size   = size;
        tpinfo.number = 1;
        send_data(sock->ip_conn, &tpinfo, msg);
        tcp_add_timer(sock, 200000, TIMER_RESEND_ACK_SYN, 0, 0, 1);
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
        rsp.src_port      = sock->local_port;
        rsp.dst_port      = sock->remote_port;
        rsp.ack           = fr.seq;
        rsp.seq           = fr.seq + 1;
        rsp.flags.ack     = 1;
        rsp.flags.syn     = 1;
        size              = 2048;
        sock->receive_seq = fr.seq;
        if(!build_frame(msg, &size, &rsp, 0, sock->ipv6, sock->local_addr, sock->remote_addr)) return;
        
        tpinfo.id     = lvl4_frame;
        tpinfo.size   = size;
        tpinfo.number = 1;
        send_data(sock->ip_conn, &tpinfo, msg);
        sock->state   = SYN_RECEIVED;
        tcp_add_timer(sock, 200000, TIMER_RESEND_ACK_SYN, 0, 0, 1);
        return;
    }

    if(fr.flags.syn && fr.flags.ack) {
        clean_frame(&rsp);
        rsp.src_port      = sock->local_port;
        rsp.dst_port      = sock->remote_port;
        rsp.ack           = fr.seq;
        rsp.flags.ack     = 1;
        size              = 2048;
        sock->receive_seq = fr.seq;
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
        sock->state   = TIME_WAIT;
        tcp_add_timer(sock, 240000, TIMER_CLOSE, 0, 0, 1);
    }
}

void message_closing(tcp_connection_t* sock, char* msg, size_t size) {
    tcp_frame_t fr;
    if(!read_frame(msg, size, &fr, sock->ipv6, sock->remote_addr, sock->local_addr)) return;

    if(fr.flags.ack) {
        sock->state = TIME_WAIT;
        tcp_add_timer(sock, 240000, TIMER_CLOSE, 0, 0, 1);
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

void message_established(tcp_connection_t* sock, char* msg, size_t size) {
    tcp_frame_t fr;
    if(!read_frame(msg, size, &fr, sock->ipv6, sock->remote_addr, sock->local_addr)) return;

    size_t size_data = size - (fr.data - msg);
    sock->remote_window = fr.window;

    /* Handle acknowledgement */
    if(fr.flags.ack && fr.ack > sock->send_seq) {
        sock->sent_size = sock->send_seq + sock->sent_size - fr.ack;
        sock->send_seq  = fr.ack;
    }

    /* Receive data */
    /* Can we receive this data ? */
    if(fr.seq + size_data - sock->receive_seq >= TCP_BUFFER_SIZE) return;

    /* Is this new data */
    if(fr.seq + size_data > sock->receive_seq + sock->receive_size) {
        size_t offset = fr.seq % TCP_BUFFER_SIZE;
        size_t size   = size_data;
        if(offset + size < TCP_BUFFER_SIZE) {
            memcpy(sock->receive_buffer + offset, fr.data, size);
        } else {
            size = TCP_BUFFER_SIZE - offset;
            memcpy(sock->receive_buffer + offset, fr.data, size);
            offset = size;
            size = size_data - size;
            memcpy(sock->receive_buffer, fr.data + offset, size);
        }
        sock->must_ack = 1;
        tcp_add_timer(sock, 5000, TIMER_ACK, 0, 0, 1);
    }
}

void message(tcp_connection_t* sock, char* msg, size_t size) {
    switch(sock->state) {
        case CLOSED:       return message_closed       (sock, msg, size);
        case LISTEN:       return message_listen       (sock, msg, size);
        case SYN_RECEIVED: return message_syn_received (sock, msg, size);
        case SYN_SENT:     return message_syn_sent     (sock, msg, size);
        case ESTABLISHED:  return message_established  (sock, msg, size);
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

/******************************************************************************
 ************************ Timer end *******************************************
 ******************************************************************************/

void end_timer(tcp_connection_t* sock, uintptr_t data) {
    size_t size;
    tcp_frame_t rsp;
    char msg[2048];
    typeinfo_t tpinfo;

    if(!sock->history[data].used) return;
    
    switch(sock->history[data].action) {
        case TIMER_CLOSE:
            if(sock->state != TIME_WAIT) break;
            reset(sock);
            break;

        case TIMER_ACK:
            if(sock->state != ESTABLISHED || !sock->must_ack) break;
            sock->must_ack = 0;

            clean_frame(&rsp);
            rsp.src_port      = sock->local_port;
            rsp.dst_port      = sock->remote_port;
            rsp.ack           = sock->receive_seq;
            rsp.flags.ack     = 1;
            size              = 2048;
            if(!build_frame(msg, &size, &rsp, 0, sock->ipv6, sock->local_addr, sock->remote_addr)) return;
            
            tpinfo.id     = lvl4_frame;
            tpinfo.size   = size;
            tpinfo.number = 1;
            send_data(sock->ip_conn, &tpinfo, msg);
            break;

        case TIMER_RESEND_DATA:
            /* TODO */
            break;

        case TIMER_RESEND_ACK_SYN:
            if(sock->state != SYN_RECEIVED) break;
            if(sock->history[data].used > 10) {
                reset(sock);
                break;
            }

            clean_frame(&rsp);
            rsp.src_port      = sock->local_port;
            rsp.dst_port      = sock->remote_port;
            rsp.ack           = sock->receive_seq;
            rsp.seq           = sock->receive_seq + 1;
            rsp.flags.ack     = 1;
            rsp.flags.syn     = 1;
            size              = 2048;
            if(!build_frame(msg, &size, &rsp, 0, sock->ipv6, sock->local_addr, sock->remote_addr)) return;
            
            tpinfo.id     = lvl4_frame;
            tpinfo.size   = size;
            tpinfo.number = 1;
            send_data(sock->ip_conn, &tpinfo, msg);

            /* TODO add TIMER_RESEND_ACK_SYN */
            ++sock->history[data].used;
            return;
    }
    sock->history[data].used = 0;
}

