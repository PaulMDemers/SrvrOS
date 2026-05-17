#include <srvros/sys.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int parse_ipv4(const char *text, uint32_t *ip_out) {
    uint32_t parts[4] = {0, 0, 0, 0};
    uint32_t part = 0;
    uint32_t value = 0;
    uint32_t digits = 0;

    if (text == 0 || text[0] == '\0' || ip_out == 0) {
        return -1;
    }
    for (uint32_t i = 0;; i++) {
        char ch = text[i];
        if (ch >= '0' && ch <= '9') {
            value = value * 10 + (uint32_t)(ch - '0');
            digits++;
            if (value > 255 || digits > 3) {
                return -1;
            }
            continue;
        }
        if ((ch == '.' || ch == '\0') && digits != 0) {
            if (part >= 4) {
                return -1;
            }
            parts[part++] = value;
            value = 0;
            digits = 0;
            if (ch == '\0') {
                if (part != 4) {
                    return -1;
                }
                *ip_out = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
                return 0;
            }
            continue;
        }
        return -1;
    }
}

static void print_ip(uint32_t ip) {
    printf("%u.%u.%u.%u",
        (unsigned)((ip >> 24) & 0xff),
        (unsigned)((ip >> 16) & 0xff),
        (unsigned)((ip >> 8) & 0xff),
        (unsigned)(ip & 0xff));
}

int main(int argc, char **argv) {
    const char *target = argc > 1 ? argv[1] : "10.0.2.2";
    uint32_t ip = 0;
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("usage: ping [host-or-ip]");
        return 0;
    }
    if (parse_ipv4(target, &ip) < 0) {
        if (srv_net_dns(target, &ip) < 0) {
            printf("ping: cannot resolve %s\n", target);
            return 1;
        }
    }

    printf("PING %s (", target);
    print_ip(ip);
    printf(")\n");

    uint32_t received = 0;
    for (uint16_t sequence = 1; sequence <= 4; sequence++) {
        uint64_t elapsed = 0;
        if (srv_net_ping(ip, sequence, 100, &elapsed) == 0) {
            printf("reply from ");
            print_ip(ip);
            printf(": seq=%u time=%llut\n", (unsigned)sequence, (unsigned long long)elapsed);
            received++;
        } else {
            printf("timeout seq=%u\n", (unsigned)sequence);
        }
    }

    printf("ping: sent=4 received=%u lost=%u\n", (unsigned)received, (unsigned)(4 - received));
    return received == 0 ? 1 : 0;
}
