
#include "protocol.h"
#include "tcp.h"
#include "proto.h"
#include "ports.h"
#include <string.h>

#define TCP_MAX_DATA 1300
#define IP_ADDR_LEN 4
/* TODO update remote window */

/* Assumes X and Y have no side effect */
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

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
    if(sock->local_addr)  free(sock->local_addr);
    if(sock->remote_addr) free(sock->remote_addr);
    sock->local_addr = sock->remote_addr = NULL;

    sock->send_seq = sock->sent_size = 0;
    sock->send_window = 0;
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
    sock->remote_window = fr.window;
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
        sock->receive_size = fr.seq + size_data - sock->receive_seq;
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
            send_data(sock->ip_conn, &tpinfo, msg);

            /* TODO add TIMER_RESEND_ACK_SYN */
            ++sock->history[data].used;
            return;
    }
    sock->history[data].used = 0;
}

/******************************************************************************
 ************************ Socket interface ************************************
 ******************************************************************************/

error_t sock_create(tcp_connection_t** sock, tcp_timer_t timer, mach_port_t ip) {
    tcp_connection_t* new = malloc(sizeof(tcp_connection_t));
    if(!new) return ENOMEM;
    reset(new);
    /* Only handle IPv4 for now */
    new->ipv6  = 0;
    new->timer = timer;
    new->ip_conn = ip;
    *sock = new;
    return 0;
}

error_t sock_listen(tcp_connection_t* sock, int qlimit) {
    if(!sock || sock->state != CLOSED) {
        return EOPNOTSUPP;
    }

    /* TODO change listen semantics to handle accept requests */
    sock->state = LISTEN;
    return 0;
}

error_t sock_accept(tcp_connection_t* sock) {
    /* TODO see listen */
    return EOPNOTSUPP;
}

error_t sock_connect(tcp_connection_t* sock, char* laddr, int lport,
        char* raddr, int rport) {
    char msg[2048];
    typeinfo_t tpinfo;
    tcp_frame_t rsp;
    size_t size;
    if(!sock || sock->state != CLOSED) {
        return EOPNOTSUPP;
    }

    sock->local_addr = malloc(IP_ADDR_LEN);
    if(!sock->local_addr) {
        return ENOMEM;
    }
    memcpy(sock->local_addr, laddr, IP_ADDR_LEN);
    sock->local_port = lport;

    sock->remote_addr = malloc(IP_ADDR_LEN);
    if(!sock->remote_addr) {
        reset(sock);
        return ENOMEM;
    }
    memcpy(sock->remote_addr, raddr, IP_ADDR_LEN);
    sock->remote_port = rport;

    clean_frame(&rsp);
    rsp.src_port      = sock->local_port;
    rsp.dst_port      = sock->remote_port;
    rsp.seq           = 0;
    rsp.flags.syn     = 1;
    size              = 2048;
    if(!build_frame(msg, &size, &rsp, 0, sock->ipv6,
                sock->local_addr, sock->remote_addr)) return EAGAIN;
    
    tpinfo.id     = lvl4_frame;
    tpinfo.size   = size;
    send_data(sock->ip_conn, &tpinfo, msg);
    /* TODO add TIMER_RESEND_SYN */
    sock->state = SYN_SENT;

    return 0;
}

error_t sock_bind(tcp_connection_t* sock, char* laddr, int lport) {
    if(!sock || sock->state != CLOSED) {
        return EOPNOTSUPP;
    }

    sock->local_addr = malloc(IP_ADDR_LEN);
    if(!sock->local_addr) {
        return ENOMEM;
    }
    memcpy(sock->local_addr, laddr, IP_ADDR_LEN);
    sock->local_port = lport;

    return 0;
}

error_t sock_shutdown(tcp_connection_t* sock) {
    char msg[2048];
    typeinfo_t tpinfo;
    tcp_frame_t rsp;
    size_t size;
    if(!sock) return EOPNOTSUPP;

    switch(sock->state) {
        case LISTEN:
        case SYN_SENT:
            reset(sock);
            break;

        case CLOSE_WAIT:
            sock->state = LAST_ACK;
        case SYN_RECEIVED:
            clean_frame(&rsp);
            rsp.src_port      = sock->local_port;
            rsp.dst_port      = sock->remote_port;
            rsp.flags.fin     = 1;
            size              = 2048;
            if(!build_frame(msg, &size, &rsp, 0, sock->ipv6,
                        sock->local_addr, sock->remote_addr)) return EAGAIN;
            
            tpinfo.id     = lvl4_frame;
            tpinfo.size   = size;
            send_data(sock->ip_conn, &tpinfo, msg);
            /* TODO add resend fin timer */
            if(sock->state != LAST_ACK) sock->state = FIN_WAIT_1;
            break;

        default:
            return EOPNOTSUPP;
    }
    return 0;
}

error_t sock_send(tcp_connection_t* sock, char* data, size_t datalen) {
    char buffer[2048];
    tcp_frame_t fr;
    typeinfo_t tpinfo;
    size_t datalength, size;

    if(!sock || sock->state != ESTABLISHED) {
        return EOPNOTSUPP;
    }

    if(sock->send_window + datalen > TCP_BUFFER_SIZE) {
        return EAGAIN;
    }

    size_t offset = (sock->send_seq + sock->send_window) % TCP_BUFFER_SIZE;
    if(offset + datalen < TCP_BUFFER_SIZE) {
        memcpy(sock->send_buffer + offset, data, datalen);
    } else {
        memcpy(sock->send_buffer + offset, data, TCP_BUFFER_SIZE - offset);
        offset  = TCP_BUFFER_SIZE - offset;
        datalen = datalen - offset;
        memcpy(sock->send_buffer, data + offset, datalen);
    }
    sock->send_window += datalen;

    /* Remote window of 0 means paused connection */
    if(sock->remote_window == 0) return 0;

    /* Add the data and send as much as possible */
    /* TODO bufferize and wait before sending it all */
    while(sock->send_window - sock->sent_size > 0) {
        clean_frame(&fr);
        fr.src_port  = sock->local_port;
        fr.dst_port  = sock->remote_port;
        fr.seq       = sock->send_seq + sock->sent_size;
        fr.window    = TCP_BUFFER_SIZE - sock->receive_size;

        fr.flags.ack = 1;
        fr.ack       = sock->receive_seq + sock->receive_size;

        offset       = sock->send_seq % TCP_BUFFER_SIZE;
        fr.data      = sock->send_buffer + offset;
        datalength   = MIN(sock->send_window - sock->sent_size, TCP_MAX_DATA);
        if(offset + datalength > TCP_BUFFER_SIZE) datalength = TCP_BUFFER_SIZE - offset;

        size = 2048;
        if(!build_frame(buffer, &size, &fr, datalength, sock->ipv6,
                    sock->local_addr, sock->remote_addr)) break;

        tpinfo.id     = lvl4_frame;
        tpinfo.size   = size;
        if(!send_data(sock->ip_conn, &tpinfo, buffer)) break;

        sock->must_ack   = 0;
        sock->sent_size += datalength;
    }

    return 0;
}

error_t sock_receive(tcp_connection_t* sock, char* data, size_t* data_size) {
    if(!sock || sock->state != ESTABLISHED) {
        return EOPNOTSUPP;
    }

    size_t used_size = MIN(*data_size, sock->receive_size);
    uint32_t offset = sock->receive_seq % TCP_BUFFER_SIZE;
    *data_size = used_size;

    if(used_size == 0) return 0;

    if(offset + used_size <= TCP_BUFFER_SIZE) {
        memcpy(data, sock->receive_buffer + offset, used_size);
    } else {
        memcpy(data, sock->receive_buffer + offset, TCP_BUFFER_SIZE - offset);
        offset = TCP_BUFFER_SIZE - offset;
        memcpy(data + offset, sock->receive_buffer, used_size - offset);
    }
    sock->receive_seq  += used_size;
    sock->receive_size -= used_size;

    return 0;
}

