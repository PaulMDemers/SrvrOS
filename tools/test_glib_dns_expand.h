#ifndef TEST_GLIB_DNS_EXPAND_H
#define TEST_GLIB_DNS_EXPAND_H

#include <stddef.h>

/* CHECK(label, condition) is provided by the including guest fixture. */
static int check_dns_expand(void) {
    CHECK("header-size", sizeof(HEADER) == 12 && _Alignof(HEADER) == 1);
    CHECK("header-id", offsetof(HEADER, id) == 0);
    CHECK("header-flags", offsetof(HEADER, flags) == 2);
    CHECK("header-qdcount", offsetof(HEADER, qdcount) == 4);
    CHECK("header-ancount", offsetof(HEADER, ancount) == 6);
    CHECK("header-nscount", offsetof(HEADER, nscount) == 8);
    CHECK("header-arcount", offsetof(HEADER, arcount) == 10);
    CHECK("dns-constants", C_IN == 1 && T_NS == 2 && T_SOA == 6 &&
        T_MX == 15 && T_TXT == 16 && T_SRV == 33);

    const unsigned char numbers[] = {0x80, 0xff, 0x81, 0x23, 0x45, 0x67};
    const unsigned char *cursor = numbers;
    unsigned short short_value = 0;
    unsigned long long_value = 0;
    GETSHORT(short_value, cursor);
    CHECK("getshort-value", short_value == 0x80ff && cursor == numbers + 2);
    GETLONG(long_value, cursor);
    CHECK("getlong-value", long_value == 0x81234567UL && cursor == numbers + 6);

    const unsigned char packet[] = {
        [12] =
        3, 'W', 'w', 'W', 7, 'E', 'x', 'a', 'm', 'p', 'l', 'e',
        3, 'C', 'O', 'M', 0,
        0xc0, 16,                /* suffix points to Example.COM */
        0xc0, 29,                /* a backward chain to that pointer */
        3, 'a', 'p', 'i', 0xc0, 16,
        0                        /* root */
    };
    const unsigned char *end = packet + sizeof(packet);
    char output[512];
    CHECK("literal-consumed", dn_expand(packet, end, packet + 12, output, sizeof(output)) == 17);
    CHECK("literal-case", strcmp(output, "WwW.Example.COM") == 0);
    CHECK("suffix-consumed", dn_expand(packet, end, packet + 29, output, sizeof(output)) == 2);
    CHECK("suffix-name", strcmp(output, "Example.COM") == 0);
    CHECK("chain-consumed", dn_expand(packet, end, packet + 31, output, sizeof(output)) == 2);
    CHECK("chain-name", strcmp(output, "Example.COM") == 0);
    CHECK("literal-pointer-consumed", dn_expand(packet, end, packet + 33, output, sizeof(output)) == 6);
    CHECK("literal-pointer-name", strcmp(output, "api.Example.COM") == 0);
    CHECK("root-consumed", dn_expand(packet, end, packet + 39, output, sizeof(output)) == 1);
    CHECK("root-empty", output[0] == '\0');

    const unsigned char escaped[] = {3, 'a', '.', 'b', 3, 'x', '\\', 'y', 1, 1, 0};
    CHECK("escape-consumed", dn_expand(escaped, escaped + sizeof(escaped), escaped,
        output, sizeof(output)) == (int)sizeof(escaped));
    CHECK("escape-text", strcmp(output, "a\\.b.x\\\\y.\\001") == 0);
    char exact[16];
    CHECK("exact-fit", dn_expand(packet, end, packet + 12, exact, sizeof(exact)) == 17);
    CHECK("exact-fit-text", strcmp(exact, "WwW.Example.COM") == 0);
    char short_buffer[15], saved[15];
    memset(short_buffer, 0x5a, sizeof(short_buffer));
    memcpy(saved, short_buffer, sizeof(saved));
    CHECK("short-rejected", dn_expand(packet, end, packet + 12, short_buffer, sizeof(short_buffer)) == -1);
    CHECK("short-unchanged", memcmp(short_buffer, saved, sizeof(saved)) == 0);

    const unsigned char truncated_label[] = {3, 'a', 'b'};
    const unsigned char truncated_pointer[] = {0xc0};
    const unsigned char forward[] = {[12] = 0xc0, 14, 0};
    const unsigned char outside[] = {[12] = 0xc0, 0xff};
    const unsigned char self[] = {[12] = 0xc0, 12};
    const unsigned char header_pointer[] = {[12] = 0xc0, 4};
    const unsigned char loop[] = {[12] = 1, 'a', 0xc0, 12};
    const unsigned char reserved[] = {0x40, 0};
    struct bad_name { const unsigned char *data; size_t size; size_t start; };
    const struct bad_name bad[] = {
        {truncated_label, sizeof(truncated_label), 0},
        {truncated_pointer, sizeof(truncated_pointer), 0},
        {forward, sizeof(forward), 12}, {outside, sizeof(outside), 12},
        {self, sizeof(self), 12}, {header_pointer, sizeof(header_pointer), 12},
        {loop, sizeof(loop), 14},
        {reserved, sizeof(reserved), 0}
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        memset(output, 0x5a, sizeof(output));
        char before[sizeof(output)];
        memcpy(before, output, sizeof(output));
        CHECK("malformed-rejected", dn_expand(bad[i].data, bad[i].data + bad[i].size,
            bad[i].data + bad[i].start, output, sizeof(output)) == -1);
        CHECK("malformed-unchanged", memcmp(output, before, sizeof(output)) == 0);
    }

    unsigned char maximum[257];
    size_t pos = 0;
    for (unsigned label = 0; label < 4; ++label) {
        unsigned n = label == 3 ? 61 : 63;
        maximum[pos++] = (unsigned char)n;
        memset(maximum + pos, 'a' + label, n);
        pos += n;
    }
    maximum[pos++] = 0;
    CHECK("max-wire-size", pos == 255);
    CHECK("max-name", dn_expand(maximum, maximum + pos, maximum, output, sizeof(output)) == 255);
    CHECK("max-name-length", strlen(output) == 253);
    /* Increase the final label by two bytes: the encoded name exceeds 255. */
    maximum[192] = 63;
    maximum[254] = 'd'; maximum[255] = 'd'; maximum[256] = 0;
    memset(output, 0x5a, sizeof(output));
    char max_before[sizeof(output)];
    memcpy(max_before, output, sizeof(output));
    CHECK("overlong-rejected", dn_expand(maximum, maximum + 257, maximum, output, sizeof(output)) == -1);
    CHECK("overlong-unchanged", memcmp(output, max_before, sizeof(output)) == 0);
    return 0;
}

#endif
