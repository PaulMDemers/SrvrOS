#include <resolv.h>
#include <srvros/dns_wire.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* DNS names use the presentation escapes of RFC 1035 master files. */
static bool dns_presentation_special(unsigned char byte) {
    switch (byte) {
    case '"': case '.': case ';': case '\\':
    case '(': case ')': case '@': case '$':
        return true;
    default:
        return false;
    }
}

int dn_expand(const unsigned char *message, const unsigned char *end,
    const unsigned char *source, char *name, int name_size) {
    uintptr_t base = (uintptr_t)message;
    uintptr_t stop = (uintptr_t)end;
    uintptr_t start = (uintptr_t)source;
    if (!message || !name || name_size <= 0 || start < base || start >= stop) return -1;

    size_t offset = start - base;
    size_t limit = stop - base;
    size_t consumed = 0;
    size_t wire_size = 1;  /* Reserve space for the terminating root label. */
    char expanded[SRV_DNS_NAME_MAX * 4];
    size_t used = 0;
    while (offset < limit) {
        size_t label_at = offset;
        unsigned length = message[offset++];
        if (length == 0) {
            if (used >= (size_t)name_size) return -1;
            expanded[used] = '\0';
            memcpy(name, expanded, used + 1);
            return (int)(consumed ? consumed : offset - (start - base));
        }
        if ((length & 0xc0) == 0xc0) {
            if (offset == limit) return -1;
            size_t target = ((size_t)(length & 0x3f) << 8) | message[offset++];
            if (target < sizeof(HEADER) || target >= label_at) return -1;
            if (consumed == 0) consumed = offset - (start - base);
            /* A prior name must finish before this pointer. Each jump
             * shrinks the upper bound, including chains through suffixes. */
            limit = label_at;
            offset = target;
            continue;
        }
        if (length > 63 || length > limit - offset ||
            length + 1 > SRV_DNS_NAME_MAX - wire_size) return -1;
        wire_size += length + 1;
        if (used != 0) expanded[used++] = '.';
        for (unsigned i = 0; i < length; i++) {
            unsigned char byte = message[offset++];
            if (byte <= 0x20 || byte >= 0x7f) {
                expanded[used++] = '\\';
                expanded[used++] = (char)('0' + byte / 100);
                expanded[used++] = (char)('0' + byte / 10 % 10);
                expanded[used++] = (char)('0' + byte % 10);
            } else {
                if (dns_presentation_special(byte)) expanded[used++] = '\\';
                expanded[used++] = (char)byte;
            }
        }
    }
    return -1;
}
