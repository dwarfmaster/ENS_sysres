
#ifndef DEF_LIB_PROTO
#define DEF_LIB_PROTO

enum Id {
    lvl1_frame = 2, /* Chosen to be the value of select message id */
    lvl2_frame,
    lvl3_frame,
    lvl4_frame,
    upper_frame,
    reserved1,
    reserved2,
    reserved3,
    arp_query,
    arp_answer
};

#endif//DEF_LIB_PROTO

