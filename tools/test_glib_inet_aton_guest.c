/* BSD numeric IPv4 parsing contract; may be compiled with symbol renaming. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct sample { const char *text; int valid; uint32_t value; };
static const struct sample cases[] = {
    {"0", 1, 0}, {"4294967295", 1, 0xffffffffu}, {"4294967296", 0, 0},
    {"0xffffffff", 1, 0xffffffffu}, {"0XFFFFFFFF", 1, 0xffffffffu},
    {"037777777777", 1, 0xffffffffu}, {"040000000000", 0, 0},
    {"255.16777215", 1, 0xffffffffu}, {"256.0", 0, 0}, {"1.16777216", 0, 0},
    {"255.255.65535", 1, 0xffffffffu}, {"1.256.1", 0, 0}, {"1.2.65536", 0, 0},
    {"255.255.255.255", 1, 0xffffffffu}, {"1.2.256.4", 0, 0}, {"1.2.3.256", 0, 0},
    {"0xc0.0250.1.0Xff", 1, 0xc0a801ffu}, {"127.1", 1, 0x7f000001u},
    {"10.0x10002", 1, 0x0a010002u}, {"010.010.010.010", 1, 0x08080808u},
    {"1.2.3.4 ", 1, 0x01020304u}, {"1.2.3.4\t\n\r\v\f", 1, 0x01020304u},
    {"", 0, 0}, {".1", 0, 0}, {"1.", 0, 0}, {"1..2", 0, 0},
    {"1.2.3.4.5", 0, 0}, {"0x", 0, 0}, {"1.0x", 0, 0},
    {"08", 0, 0}, {"0xg", 0, 0}, {"1.2.3.4x", 0, 0},
    {"1.2.3.4 x", 0, 0}, {" 1.2.3.4", 0, 0}, {"+1", 0, 0}, {"-1", 0, 0}
};

int main(void) {
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        struct in_addr result;
        memset(&result, 0xa5, sizeof(result));
        struct in_addr before = result;
        errno = EDOM;
        int status = inet_aton(cases[i].text, &result);
        if (!!status != cases[i].valid || errno != EDOM ||
            (status && ntohl(result.s_addr) != cases[i].value) ||
            (!status && memcmp(&result, &before, sizeof(result)) != 0)) {
            printf("glib-inet-aton: case %zu failed status=%d errno=%d\n", i, status, errno);
            return 1;
        }
        errno = EDOM;
        status = inet_aton(cases[i].text, NULL);
        if (!!status != cases[i].valid || errno != EDOM) {
            printf("glib-inet-aton: null-output case %zu failed status=%d errno=%d\n", i, status, errno);
            return 1;
        }
    }
    struct in_addr addr;
    errno = EDOM;
    if (inet_pton(AF_INET, "010.010.010.010", &addr) != 1 ||
        ntohl(addr.s_addr) != UINT32_C(0x0a0a0a0a) || errno != EDOM) {
        printf("glib-inet-aton: inet_pton decimal behavior changed\n");
        return 1;
    }
    puts("glib-inet-aton: ok");
    return 0;
}
