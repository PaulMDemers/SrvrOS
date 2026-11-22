#ifndef SRVROS_POSIX_ARPA_NAMESER_H
#define SRVROS_POSIX_ARPA_NAMESER_H

#include <stdint.h>

/* DNS Internet class (RFC 1035, section 3.2.4). */
#define C_IN 1

#define T_NS 2
#define T_SOA 6
#define T_MX 15
#define T_TXT 16
#define T_SRV 33

/* The fixed 12-byte DNS message header, with all fields in network order. */
typedef struct __attribute__((__packed__)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} HEADER;

/* Advance a mutable byte cursor. The caller checks that 2 or 4 bytes remain. */
#define GETSHORT(value, cursor) do { \
    __typeof__(&(cursor)) __srvros_cursor = &(cursor); \
    const unsigned char *__srvros_bytes = *__srvros_cursor; \
    (value) = (uint16_t)((uint16_t)__srvros_bytes[0] << 8 | __srvros_bytes[1]); \
    *__srvros_cursor += 2; \
} while (0)

#define GETLONG(value, cursor) do { \
    __typeof__(&(cursor)) __srvros_cursor = &(cursor); \
    const unsigned char *__srvros_bytes = *__srvros_cursor; \
    (value) = (uint32_t)__srvros_bytes[0] << 24 | (uint32_t)__srvros_bytes[1] << 16 | \
        (uint32_t)__srvros_bytes[2] << 8 | __srvros_bytes[3]; \
    *__srvros_cursor += 4; \
} while (0)

#endif
