#include <srvros/sys.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <poll.h>
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

static int check_udp_datagrams(void) {
    struct srv_net_status_info status;
    if (srv_net_status_info(&status) < 0) {
        return fail("udp-status");
    }
    int server = socket(AF_INET, SOCK_DGRAM, 0);
    int client = socket(AF_INET, SOCK_DGRAM, 0);
    int result = 1;
    if (server < 0 || client < 0) {
        goto done;
    }
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(7106)};
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        bind(server, (struct sockaddr *)&address, sizeof(address)) >= 0) {
        goto done;
    }
    address.sin_addr.s_addr = htonl(status.local_ip);
    for (unsigned kind = 0; kind < 3; kind++) {
        size_t size = kind == 2 ? 0 : 6;
        if (sendto(client, "abcdef", size, 0, (struct sockaddr *)&address,
                sizeof(address)) != (ssize_t)size) {
            goto done;
        }
        char data[3] = {0};
        struct iovec iov = {data, sizeof(data)};
        unsigned char peer[3] = {0xa5, 0xa5, 0xa5};
        struct msghdr msg = {.msg_iov = &iov, .msg_iovlen = kind == 1 ? 0 : 1,
            .msg_name = peer + 1, .msg_namelen = 1};
        ssize_t n = recvmsg(server, &msg, 0);
        if (n != (kind == 0 ? 3 : 0) ||
            !!(msg.msg_flags & MSG_TRUNC) != (kind != 2) ||
            (kind == 0 && memcmp(data, "abc", 3) != 0) ||
            msg.msg_namelen != sizeof(struct sockaddr_in) ||
            peer[0] != 0xa5 || peer[2] != 0xa5) {
            goto done;
        }
        struct pollfd pfd = {.fd = server, .events = POLLIN};
        if (poll(&pfd, 1, 0) != 0) {
            goto done;
        }
    }
    for (unsigned char i = 0; i < 6; i++) {
        if (sendto(client, &i, 1, 0, (struct sockaddr *)&address, sizeof(address)) != 1) {
            goto done;
        }
    }
    for (unsigned char i = 2; i < 6; i++) {
        unsigned char byte;
        if (recvfrom(server, &byte, 1, 0, NULL, NULL) != 1 || byte != i) {
            goto done;
        }
    }
    result = 0;
    printf("netabi: udp truncation/empty/zero/address/queue ok\n");
done:
    if (server >= 0) {
        close(server);
    }
    if (client >= 0) {
        close(client);
    }
    return result ? fail("udp-datagrams") : 0;
}

static int check_udp_peers(void) {
    struct srv_net_status_info status;
    if (srv_net_status_info(&status) < 0) {
        return fail("udp-peer-status");
    }
    int server = socket(AF_INET, SOCK_DGRAM, 0);
    int peer = socket(AF_INET, SOCK_DGRAM, 0);
    int foreign = socket(AF_INET, SOCK_DGRAM, 0);
    int result = 1;
    struct sockaddr_in destination = {.sin_family = AF_INET, .sin_port = htons(7107),
        .sin_addr.s_addr = htonl(status.local_ip)};
    struct sockaddr_in source = destination;
    source.sin_port = htons(7108);
    if (server < 0 || peer < 0 || foreign < 0 ||
        bind(server, (struct sockaddr *)&destination, sizeof(destination)) < 0 ||
        bind(peer, (struct sockaddr *)&source, sizeof(source)) < 0) {
        goto done;
    }
    char byte = 0;
    if (sendto(peer, "u", 1, 0, (struct sockaddr *)&destination, sizeof(destination)) != 1 ||
        recv(server, &byte, 1, MSG_DONTWAIT) != 1 || byte != 'u' ||
        connect(server, (struct sockaddr *)&source, sizeof(source)) < 0) {
        goto done;
    }
    for (unsigned mode = 0; mode < 4; mode++) {
        if (sendto(foreign, "x", 1, 0, (struct sockaddr *)&destination, sizeof(destination)) != 1 ||
            sendto(peer, "y", 1, 0, (struct sockaddr *)&destination, sizeof(destination)) != 1) {
            goto done;
        }
        ssize_t count;
        byte = 0;
        if (mode == 0) {
            count = recv(server, &byte, 1, MSG_DONTWAIT);
        } else if (mode == 1) {
            struct sockaddr_in received;
            socklen_t size = sizeof(received);
            count = recvfrom(server, &byte, 1, MSG_DONTWAIT, (struct sockaddr *)&received, &size);
            if (size != sizeof(received) || received.sin_port != source.sin_port) {
                goto done;
            }
        } else {
            struct iovec iov = {&byte, 1};
            struct msghdr msg = {.msg_iov = &iov, .msg_iovlen = mode == 3 ? 0 : 1};
            count = recvmsg(server, &msg, MSG_DONTWAIT);
            if (!!(msg.msg_flags & MSG_TRUNC) != (mode == 3)) {
                goto done;
            }
        }
        if (count != (mode == 3 ? 0 : 1) || (mode != 3 && byte != 'y')) {
            goto done;
        }
    }
    if (sendto(foreign, "x", 1, 0, (struct sockaddr *)&destination, sizeof(destination)) != 1 ||
        recvfrom(server, &byte, 1, MSG_DONTWAIT, NULL, NULL) != -1 || errno != EAGAIN) {
        goto done;
    }
    result = 0;
    printf("netabi: udp connected filtering/unconnected receive ok\n");
done:
    if (server >= 0) close(server);
    if (peer >= 0) close(peer);
    if (foreign >= 0) close(foreign);
    return result ? fail("udp-peers") : 0;
}

int main(void) {
    if (srv_net_dhcp() < 0) {
        return fail("dhcp");
    }
    if (check_udp_datagrams() != 0 ||
        check_udp_peers() != 0 ||
        check_status_truncation() != 0 ||
        check_net_list_truncation() != 0 ||
        check_arp_list_truncation() != 0) {
        return 1;
    }
    printf("netabi: ok\n");
    return 0;
}
