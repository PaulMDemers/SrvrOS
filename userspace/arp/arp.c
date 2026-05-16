#include <srvros/sys.h>

#include <stdint.h>
#include <stdio.h>

static void print_ip(uint32_t ip) {
    if (ip == 0) {
        printf("0.0.0.0");
        return;
    }
    printf("%u.%u.%u.%u",
        (unsigned)((ip >> 24) & 0xff),
        (unsigned)((ip >> 16) & 0xff),
        (unsigned)((ip >> 8) & 0xff),
        (unsigned)(ip & 0xff));
}

static void print_mac(const uint8_t mac[6]) {
    printf("%02x:%02x:%02x:%02x:%02x:%02x",
        (unsigned)mac[0],
        (unsigned)mac[1],
        (unsigned)mac[2],
        (unsigned)mac[3],
        (unsigned)mac[4],
        (unsigned)mac[5]);
}

int main(void) {
    uint64_t index = 0;
    uint64_t count = 0;
    struct srv_arp_info info;
    printf("Address            HWaddress          Iface\n");
    for (;;) {
        long next = srv_net_arp_list(index, &info);
        if (next < 0) {
            printf("arp: list failed\n");
            return 1;
        }
        if (next == 0) {
            break;
        }
        print_ip(info.ip);
        printf("        ");
        print_mac(info.mac);
        printf(" e1000 age=%llu\n", (unsigned long long)(srv_ticks() - info.updated_tick));
        index = (uint64_t)next;
        count++;
    }
    if (count == 0) {
        printf("arp: cache empty\n");
    }
    return 0;
}
