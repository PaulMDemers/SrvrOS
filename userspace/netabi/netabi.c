#include <srvros/sys.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define NETABI_CANARY 0x7a6b5c4d3e2f1908ull
#define NETABI_ROUTER_IP ((((uint32_t)10) << 24) | (((uint32_t)0) << 16) | (((uint32_t)2) << 8) | 2)

struct small_status_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t initialized;
};

struct small_net_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t kind;
};

struct small_arp_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint32_t ip;
};

static int fail(const char *step) {
    printf("netabi: fail %s\n", step);
    return 1;
}

static void prepare_header(uint64_t *abi_version, uint64_t *struct_size, uint64_t size) {
    *abi_version = SRV_NET_ABI_VERSION;
    *struct_size = size;
}

static int check_status_truncation(void) {
    struct {
        struct small_status_info info;
        uint64_t canary;
    } probe;
    memset(&probe, 0, sizeof(probe));
    probe.canary = NETABI_CANARY;
    prepare_header(&probe.info.abi_version, &probe.info.struct_size, sizeof(probe.info));

    if (srv_syscall1(SYS_NET_STATUS_INFO, (long)&probe.info) != 0) {
        return fail("status-syscall");
    }
    if (probe.canary != NETABI_CANARY) {
        return fail("status-canary");
    }
    if (probe.info.abi_version != SRV_NET_ABI_VERSION ||
        probe.info.struct_size < sizeof(probe.info) ||
        probe.info.initialized == 0) {
        return fail("status-header");
    }
    return 0;
}

static int check_net_list_truncation(void) {
    long listener = srv_net_listen(8124);
    if (listener < 0) {
        return fail("listen");
    }

    struct {
        struct small_net_info info;
        uint64_t canary;
    } probe;
    memset(&probe, 0, sizeof(probe));
    probe.canary = NETABI_CANARY;
    prepare_header(&probe.info.abi_version, &probe.info.struct_size, sizeof(probe.info));

    long next = srv_syscall2(SYS_NET_LIST, 0, (long)&probe.info);
    close((int)listener);
    if (next <= 0) {
        return fail("list-syscall");
    }
    if (probe.canary != NETABI_CANARY) {
        return fail("list-canary");
    }
    if (probe.info.abi_version != SRV_NET_ABI_VERSION ||
        probe.info.struct_size < sizeof(probe.info) ||
        probe.info.kind == 0) {
        return fail("list-header");
    }
    return 0;
}

static int check_arp_list_truncation(void) {
    uint64_t elapsed = 0;
    (void)srv_net_ping(NETABI_ROUTER_IP, 91, 100, &elapsed);

    struct {
        struct small_arp_info info;
        uint64_t canary;
    } probe;
    memset(&probe, 0, sizeof(probe));
    probe.canary = NETABI_CANARY;
    prepare_header(&probe.info.abi_version, &probe.info.struct_size, sizeof(probe.info));

    long next = srv_syscall2(SYS_NET_ARP_LIST, 0, (long)&probe.info);
    if (next <= 0) {
        return fail("arp-syscall");
    }
    if (probe.canary != NETABI_CANARY) {
        return fail("arp-canary");
    }
    if (probe.info.abi_version != SRV_NET_ABI_VERSION ||
        probe.info.struct_size < sizeof(probe.info) ||
        probe.info.ip == 0) {
        return fail("arp-header");
    }
    return 0;
}

int main(void) {
    if (srv_net_dhcp() < 0) {
        return fail("dhcp");
    }
    if (check_status_truncation() != 0 ||
        check_net_list_truncation() != 0 ||
        check_arp_list_truncation() != 0) {
        return 1;
    }
    printf("netabi: ok\n");
    return 0;
}
