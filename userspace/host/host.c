#include <srvros/sys.h>

#include <stdint.h>
#include <stdio.h>

static void print_ip(uint32_t ip) {
    printf("%u.%u.%u.%u",
        (unsigned)((ip >> 24) & 0xff),
        (unsigned)((ip >> 16) & 0xff),
        (unsigned)((ip >> 8) & 0xff),
        (unsigned)(ip & 0xff));
}

int main(int argc, char **argv) {
    const char *name = argc > 1 ? argv[1] : "example.com";
    uint32_t ip = 0;
    if (srv_net_dns(name, &ip) < 0) {
        printf("host: %s not found\n", name);
        return 1;
    }

    printf("%s has address ", name);
    print_ip(ip);
    printf("\n");
    return 0;
}
