
#ifndef DEF_TCP_TCP
#define DEF_TCP_TCP

#include <stdint.h>
#include <stdlib.h>

typedef struct tcp_frame {
    uint16_t src_port, dst_port;
    uint32_t seq;
    uint32_t ack;
    uint16_t window;
    uint16_t urgent;
    /* For now we do not indicate the NS, CWR and ECE flags as they are not absolutly
     * necessary.
     */
    struct {
        int urg;
        int ack;
        int psh;
        int rst;
        int syn;
        int fin;
    } flags;
    char* data;
} tcp_frame_t;

/* Returns 0 if decoding succedded and 1 otherwise */
int read_frame(char* buffer, size_t size, tcp_frame_t* fr, int ipv6, const char* src, const char* dst);
/* Build frame with the values sets in fr.
 * fr->data can be inside of buffer.
 * Set the size of the resulting frame in size.
 */
int build_frame(char* buffer, size_t* size, const tcp_frame_t* fr, size_t datalength,
        int ipv6, const char* src, const char* dst);

#endif

