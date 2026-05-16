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

int main(void) {
    struct srv_net_status_info info;
    if (srv_net_status_info(&info) < 0) {
        printf("route: net status failed\n");
        return 1;
    }

    printf("Destination        Gateway            Iface Flags\n");
    printf("default            ");
    print_ip(info.router_ip);
    printf("        e1000 ");
    printf("%s\n", info.router_ip != 0 ? "UG" : "-");
    printf("10.0.2.0/24        0.0.0.0            e1000 U\n");
    printf("dns                ");
    print_ip(info.dns_ip);
    printf("        e1000 %s\n", info.dns_ip != 0 ? "U" : "-");
    return 0;
}
