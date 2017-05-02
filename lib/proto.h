
#ifndef DEF_LIB_PROTO
#define DEF_LIB_PROTO

enum Id {
    lvl1_frame = 9, /* Chosen to be the value of device read */
    lvl2_frame,
    lvl3_frame,
    lvl32_frame, /* Data is a level-2 address and the data itself */
    lvl4_frame,
    upper_frame,
    reserved1,
    reserved2,
    reserved3,
    arp_query,
    arp_answer
};

#endif//DEF_LIB_PROTO

