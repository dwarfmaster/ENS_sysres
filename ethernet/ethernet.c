
#include "ethernet.h"
#include "logging.h"
#include "endian.h"
#include <string.h>

int char_value(char c) {
    if(c >= '0' && c <= '9')      return c - '0';
    else if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    else if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    else                          return -1;
}

int pair_value(const char* src, size_t id) {
    int i1 = char_value(src[id]);
    int i2 = char_value(src[id + 1]);
    if(i1 < 0 || i2 < 0) return -1;
    else                 return i1 * 16 + i2;
}

ethernet_error_t read_mac_address(const char* src, struct mac_address* dst) {
    /* TODO : support other formats */
    int i;
    if(strlen(src) != 17 || src[2] != ':'
            || src[5]  != ':' || src[8]  != ':'
            || src[11] != ':' || src[14] != ':') {
        return ETH_INVALID;
    }

    for(int j = 0; j < 6; ++j) {
        i = pair_value(src, 3*j);
        if(i < 0) return ETH_INVALID;
        dst->bytes[j] = (uint8_t)i;
    }
    return ETH_SUCCESS;
}

/*****************************************************************
 ************************* CRC32 *********************************
 *****************************************************************/

static const uint32_t crc_polybytes[256] = {
    0x00000000,0x04C11DB7,0x09823B6E,0x0D4326D9,0x130476DC,0x17C56B6B,0x1A864DB2,0x1E475005,
    0x2608EDB8,0x22C9F00F,0x2F8AD6D6,0x2B4BCB61,0x350C9B64,0x31CD86D3,0x3C8EA00A,0x384FBDBD,
    0x4C11DB70,0x48D0C6C7,0x4593E01E,0x4152FDA9,0x5F15ADAC,0x5BD4B01B,0x569796C2,0x52568B75,
    0x6A1936C8,0x6ED82B7F,0x639B0DA6,0x675A1011,0x791D4014,0x7DDC5DA3,0x709F7B7A,0x745E66CD,
    0x9823B6E0,0x9CE2AB57,0x91A18D8E,0x95609039,0x8B27C03C,0x8FE6DD8B,0x82A5FB52,0x8664E6E5,
    0xBE2B5B58,0xBAEA46EF,0xB7A96036,0xB3687D81,0xAD2F2D84,0xA9EE3033,0xA4AD16EA,0xA06C0B5D,
    0xD4326D90,0xD0F37027,0xDDB056FE,0xD9714B49,0xC7361B4C,0xC3F706FB,0xCEB42022,0xCA753D95,
    0xF23A8028,0xF6FB9D9F,0xFBB8BB46,0xFF79A6F1,0xE13EF6F4,0xE5FFEB43,0xE8BCCD9A,0xEC7DD02D,
    0x34867077,0x30476DC0,0x3D044B19,0x39C556AE,0x278206AB,0x23431B1C,0x2E003DC5,0x2AC12072,
    0x128E9DCF,0x164F8078,0x1B0CA6A1,0x1FCDBB16,0x018AEB13,0x054BF6A4,0x0808D07D,0x0CC9CDCA,
    0x7897AB07,0x7C56B6B0,0x71159069,0x75D48DDE,0x6B93DDDB,0x6F52C06C,0x6211E6B5,0x66D0FB02,
    0x5E9F46BF,0x5A5E5B08,0x571D7DD1,0x53DC6066,0x4D9B3063,0x495A2DD4,0x44190B0D,0x40D816BA,
    0xACA5C697,0xA864DB20,0xA527FDF9,0xA1E6E04E,0xBFA1B04B,0xBB60ADFC,0xB6238B25,0xB2E29692,
    0x8AAD2B2F,0x8E6C3698,0x832F1041,0x87EE0DF6,0x99A95DF3,0x9D684044,0x902B669D,0x94EA7B2A,
    0xE0B41DE7,0xE4750050,0xE9362689,0xEDF73B3E,0xF3B06B3B,0xF771768C,0xFA325055,0xFEF34DE2,
    0xC6BCF05F,0xC27DEDE8,0xCF3ECB31,0xCBFFD686,0xD5B88683,0xD1799B34,0xDC3ABDED,0xD8FBA05A,
    0x690CE0EE,0x6DCDFD59,0x608EDB80,0x644FC637,0x7A089632,0x7EC98B85,0x738AAD5C,0x774BB0EB,
    0x4F040D56,0x4BC510E1,0x46863638,0x42472B8F,0x5C007B8A,0x58C1663D,0x558240E4,0x51435D53,
    0x251D3B9E,0x21DC2629,0x2C9F00F0,0x285E1D47,0x36194D42,0x32D850F5,0x3F9B762C,0x3B5A6B9B,
    0x0315D626,0x07D4CB91,0x0A97ED48,0x0E56F0FF,0x1011A0FA,0x14D0BD4D,0x19939B94,0x1D528623,
    0xF12F560E,0xF5EE4BB9,0xF8AD6D60,0xFC6C70D7,0xE22B20D2,0xE6EA3D65,0xEBA91BBC,0xEF68060B,
    0xD727BBB6,0xD3E6A601,0xDEA580D8,0xDA649D6F,0xC423CD6A,0xC0E2D0DD,0xCDA1F604,0xC960EBB3,
    0xBD3E8D7E,0xB9FF90C9,0xB4BCB610,0xB07DABA7,0xAE3AFBA2,0xAAFBE615,0xA7B8C0CC,0xA379DD7B,
    0x9B3660C6,0x9FF77D71,0x92B45BA8,0x9675461F,0x8832161A,0x8CF30BAD,0x81B02D74,0x857130C3,
    0x5D8A9099,0x594B8D2E,0x5408ABF7,0x50C9B640,0x4E8EE645,0x4A4FFBF2,0x470CDD2B,0x43CDC09C,
    0x7B827D21,0x7F436096,0x7200464F,0x76C15BF8,0x68860BFD,0x6C47164A,0x61043093,0x65C52D24,
    0x119B4BE9,0x155A565E,0x18197087,0x1CD86D30,0x029F3D35,0x065E2082,0x0B1D065B,0x0FDC1BEC,
    0x3793A651,0x3352BBE6,0x3E119D3F,0x3AD08088,0x2497D08D,0x2056CD3A,0x2D15EBE3,0x29D4F654,
    0xC5A92679,0xC1683BCE,0xCC2B1D17,0xC8EA00A0,0xD6AD50A5,0xD26C4D12,0xDF2F6BCB,0xDBEE767C,
    0xE3A1CBC1,0xE760D676,0xEA23F0AF,0xEEE2ED18,0xF0A5BD1D,0xF464A0AA,0xF9278673,0xFDE69BC4,
    0x89B8FD09,0x8D79E0BE,0x803AC667,0x84FBDBD0,0x9ABC8BD5,0x9E7D9662,0x933EB0BB,0x97FFAD0C,
    0xAFB010B1,0xAB710D06,0xA6322BDF,0xA2F33668,0xBCB4666D,0xB8757BDA,0xB5365D03,0xB1F740B4
};
static const uint8_t reflect8[256] = {
    0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
    0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
    0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
    0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
    0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
    0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
    0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
    0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
    0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
    0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
    0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
    0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
    0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
    0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
    0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
    0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF
};

uint32_t reflect32(uint32_t v) {
    uint32_t ret = 0;
    for(size_t i = 0; i < 31; ++i) {
        if(v & 1) ret |= (1 << (31 - i));
        v = (v >> 1);
    }
    return ret;
}

/* Returns an error if the size of the message is too big/too small. */
/* Algorithm implementation taken from :
 *   https://barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code
 * crc is stored in hardware (network) byte order
 */
ethernet_error_t compute_crc(const char* buffer, size_t size, uint32_t* crc) {
    if(size < 60 || size > 1518) return ETH_INVALID;
    *crc = 0xFFFFFFFF;
    uint8_t data;
    for(size_t byte = 0; byte < size; ++byte) {
        data = reflect8[(uint8_t)buffer[byte]] ^ (*crc >> 24);
        *crc = crc_polybytes[data] ^ (*crc << 8);
    }
    *crc = reflect32(*crc) ^ 0xFFFFFFFF;
    *crc = stoh32(*crc);
    return ETH_SUCCESS;
}

/* CRC is expected in software byte order */
ethernet_error_t check_crc(const char* buffer, size_t size, uint32_t crc) {
    uint32_t computed;
    ethernet_error_t err = htos32(compute_crc(buffer, size, &computed));
    if(err != ETH_SUCCESS) return err;
    if(computed == crc)    return ETH_SUCCESS;
    else                   return ETH_INVALID;
}

/*****************************************************************
 ******************** Frame Handling *****************************
 *****************************************************************/

struct ethII_header {
    uint8_t bytes[12];
    uint16_t ethertype;
} __attribute__((__packed__));

struct ethII_header_tagged {
    uint8_t bytes[12];
    uint16_t tpid;
    uint16_t tci;
    uint16_t ethertype;
} __attribute__((__packed__));

struct eth_header_llc {
    uint8_t dsap, ssap;
    uint8_t control;
    struct eth_snap snap;
} __attribute__((__packed__));


/* Makes an ethernet II frame, without tag.
 * CRC does not need to be already computed.
 */
ethernet_error_t make_frame(struct eth_frame* frame, char* buffer, size_t* size) {
    struct ethII_header* hd = (struct ethII_header*)buffer;
    uint32_t* crc = (uint32_t*)(buffer + sizeof(struct ethII_header) + frame->size);
    size_t pack_size = sizeof(struct ethII_header) + 4 + frame->size;
    if(pack_size > *size) return ETH_INVALID;

    for(size_t i = 0; i < 12; ++i) hd->bytes[i] = (i >= 6 ? frame->src.bytes[i-6] : frame->dst.bytes[i]);
    hd->ethertype = stoh16(frame->ethertype);
    memmove(buffer + sizeof(struct ethII_header), frame->data, frame->size);
    *size = pack_size;
    return compute_crc(buffer, frame->size + sizeof(struct ethII_header), crc);
}




ethernet_error_t decode_frame(char* buffer, size_t size, struct eth_frame* frame) {
    struct ethII_header* hd = (struct ethII_header*)buffer;
    struct ethII_header_tagged* hd_tag = (struct ethII_header_tagged*)buffer;
    char* data = NULL;
    if(size < 14) return ETH_INVALID;

    /* Copy the source and destination buffers */
    memcpy(frame, buffer, 12);

    if(htos16(hd->ethertype) == 0x8100) {
        frame->tag       = htos16(hd_tag->tci);
        frame->ethertype = htos16(hd_tag->ethertype);
        data             = buffer + sizeof(struct ethII_header_tagged);
    } else {
        frame->tag       = 0; /* This value is reserved when tag is present, so
                               * it can be used to test the absence of tag
                               */
        frame->ethertype = htos16(hd->ethertype);
        data             = buffer + sizeof(struct ethII_header);
    }


    struct eth_header_llc* llc = (struct eth_header_llc*)data;
    if(frame->ethertype >= 1536) { /* Ethernet II frame, ie DIX ethernet */
        frame->size = size - (data - buffer);
        frame->data = data;
        return ETH_SUCCESS;
    }
    
    else if(frame->ethertype <= 1500) {
        /* Novel Raw IPX */
        if(llc->dsap == 0xFF && llc->ssap == 0xFF) {
            frame->size      = htos16(frame->ethertype);
            frame->data      = data;
            frame->ethertype = 0x8137; /* IPX ethertype */
        }
        /* TODO : we won't handle anything for now, it's too complicated, and doesn't seem
         * used in pratice.
         */
        /* Snap extension */
        else if(llc->dsap == 0xAA && llc->ssap == 0xAA) {
            log_string("Unhandled LLC-SNAP ethernet frame");
            return ETH_INVALID;
        }
        /* Standart LLC-only ethernet */
        else {
            log_string("Unhandled LLC ethernet frame");
            return ETH_INVALID; /* Drop the frame */
        }

        if(frame->size + (size_t)(data - buffer) != size) {
            return ETH_INVALID;
        } else {
            return ETH_SUCCESS;
        }
    }
    
    else {
        return ETH_INVALID;
    }
}

