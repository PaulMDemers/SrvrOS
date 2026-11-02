#include <srvros/sys.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
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

/* Exercise alias state through public libc descriptors and the native UDP queue. */
static int check_udp_alias(void) {
    struct srv_net_status_info status;
    if (srv_net_status_info(&status) < 0) return fail("alias-net-status");
    int server = socket(AF_INET, SOCK_DGRAM, 0);
    int sender = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    int alias = -1;
    const char *failure = "alias-open";
    if (server < 0 || sender < 0 || (alias = dup(sender)) < 0) goto done;

    struct sockaddr_in peer = {.sin_family = AF_INET, .sin_port = htons(7109),
        .sin_addr.s_addr = htonl(status.local_ip)};
    failure = "alias-peer";
    if (bind(server, (struct sockaddr *)&peer, sizeof(peer)) < 0 ||
        connect(alias, (struct sockaddr *)&peer, sizeof(peer)) < 0) goto done;

    int option = 512;
    int observed = 0;
    socklen_t size = sizeof(observed);
    failure = "alias-options";
    if (setsockopt(sender, SOL_SOCKET, SO_RCVBUF, &option, sizeof(option)) < 0 ||
        getsockopt(alias, SOL_SOCKET, SO_RCVBUF, &observed, &size) < 0 ||
        observed != option || fcntl(alias, F_GETFD) != 0 ||
        fcntl(sender, F_GETFD) != FD_CLOEXEC) goto done;

    char byte;
    failure = "alias-delivery";
    if (send(sender, "a", 1, 0) != 1 || recv(server, &byte, 1, MSG_DONTWAIT) != 1 ||
        byte != 'a' || close(alias) != 0) goto done;
    alias = -1;
    if (send(sender, "b", 1, 0) != 1 || recv(server, &byte, 1, MSG_DONTWAIT) != 1 ||
        byte != 'b') goto done;
    failure = NULL;
done:
    if (alias >= 0) close(alias);
    if (sender >= 0) close(sender);
    if (server >= 0) close(server);
    if (failure) return fail(failure);
    puts("netabi: UDP duplicate shares peer and options across close");
    return 0;
}

static volatile sig_atomic_t pipe_signals;
static void pipe_handler(int signal) { if(signal==SIGPIPE)pipe_signals++; }
static int check_call_signal(void) {
    struct sigaction action={.sa_handler=pipe_handler},saved;int pair[2];
    if(sigaction(SIGPIPE,&action,&saved)<0)return fail("call-signal-action");
    if(socketpair(AF_UNIX,SOCK_STREAM,0,pair)<0) { sigaction(SIGPIPE,&saved,0);return fail("call-signal-pair"); }
    close(pair[1]);pipe_signals=0;
    int result=send(pair[0],"x",1,MSG_DONTWAIT)==-1 && errno==EPIPE && pipe_signals==1;
    close(pair[0]);sigaction(SIGPIPE,&saved,0);
    if(!result)return fail("call-signal");
    printf("netabi: per-call signal checkpoint ok\n");return 0;
}

static int check_call_flags(void) {
    int pair[2],alias=-1,result=1;char buffer[4096]={0};
    if(socketpair(AF_UNIX,SOCK_STREAM,0,pair)<0)return fail("flags-pair");
    alias=dup(pair[0]);if(alias<0)goto done;
    if(recv(pair[0],buffer,1,MSG_DONTWAIT)!=-1 || errno!=EAGAIN)goto done;
    if((fcntl(pair[0],F_GETFL)|fcntl(alias,F_GETFL))&O_NONBLOCK)goto done;
    if(send(pair[1],"abc",3,MSG_DONTWAIT)!=3 || recv(pair[0],buffer,3,MSG_DONTWAIT)!=3 || memcmp(buffer,"abc",3))goto done;
    unsigned count=0;
    while(count++<1024) {
        ssize_t n=send(pair[0],buffer,sizeof(buffer),MSG_DONTWAIT);
        if(n<0) { if(errno!=EAGAIN)goto done;break; }
    }
    if(count>=1024 || ((fcntl(pair[0],F_GETFL)|fcntl(alias,F_GETFL))&O_NONBLOCK))goto done;
    if(recv(pair[1],buffer,sizeof(buffer),MSG_DONTWAIT)<=0 || send(alias,"x",1,MSG_DONTWAIT)!=1)goto done;
    if(srv_socket_try_io(pair[0],99,buffer,1,0)!=SRV_ERR_INVAL)goto done;
    result=0;printf("netabi: per-call flags/alias/pressure/recovery ok\n");
 done:
    if(alias>=0)close(alias);close(pair[0]);close(pair[1]);return result?fail("call-flags"):0;
}

struct udp_mapping_receive {
    int fd;
    char *data;
    uint32_t *ip;
    uint16_t port;
    unsigned entered;
    long result;
    struct srv_sched_trace trace;
};
static void *udp_mapping_reader(void *raw) {
    struct udp_mapping_receive *receive = raw;
    if (srv_sched_trace(SRV_SCHED_TRACE_BEGIN, &receive->trace)) return (void *)1;
    __atomic_store_n(&receive->entered, 1, __ATOMIC_RELEASE);
    receive->result = srv_net_udp_recvfrom(receive->fd, receive->data, 2, receive->ip, &receive->port);
    return srv_sched_trace(SRV_SCHED_TRACE_END, &receive->trace) ? (void *)1 : NULL;
}
static int check_udp_mapping(unsigned kind) {
#define UDP_CHECK(x) do { if (!(x)) { printf("netabi: UDP mapping kind=%u line=%d\n", kind, __LINE__); return fail("udp-mapping"); } } while (0)
    struct srv_net_status_info status;
    UDP_CHECK(srv_net_status_info(&status) == 0);
    int fd = srv_net_udp_open(), sender = srv_net_udp_open();
    UDP_CHECK(fd >= 0 && sender >= 0 && srv_net_udp_bind(fd, 7140 + kind) == 0);
    char *mapping = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    UDP_CHECK(mapping != MAP_FAILED);
    char bytes[2] = {'?', '?'};
    uint32_t ip = 0;
    struct udp_mapping_receive receive = {.fd=fd, .data=kind == 0 ? mapping : bytes,
        .ip=kind == 1 ? (uint32_t *)mapping : &ip, .port=0};
    int alias = -1, replacement = -1;
    if (kind == 2) {
        alias = srv_dup(fd); replacement = srv_net_udp_open();
        UDP_CHECK(alias >= 0 && replacement >= 0 && srv_net_udp_bind(replacement, 7145) == 0);
    }
    pthread_t thread;
    UDP_CHECK(!pthread_create(&thread, NULL, udp_mapping_reader, &receive));
    while (!__atomic_load_n(&receive.entered, __ATOMIC_ACQUIRE)) sched_yield();
    usleep(30000);
    if (kind == 2) UDP_CHECK(srv_dup2(replacement, fd) == fd);
    UDP_CHECK(!munmap(mapping, 4096));
    UDP_CHECK(srv_net_udp_sendto(sender, status.local_ip, 7140 + kind, "AB", 2) == 2);
    if (kind == 2) UDP_CHECK(srv_net_udp_sendto(sender, status.local_ip, 7145, "CD", 2) == 2);
    void *result;
    UDP_CHECK(!pthread_join(thread, &result) && !result && receive.trace.blocks > 0);
    UDP_CHECK(receive.result == (kind == 2 ? SRV_ERR_BADF : SRV_ERR_FAULT));
    UDP_CHECK(bytes[0] == '?' && bytes[1] == '?' && receive.port == 0);
    int original = kind == 2 ? alias : fd;
    UDP_CHECK(srv_fcntl(original, SRV_F_SETFL, SRV_FD_NONBLOCK) == 0);
    UDP_CHECK(srv_net_udp_recvfrom(original, bytes, 2, &ip, NULL) == 2 && !memcmp(bytes, "AB", 2));
    if (kind == 2) {
        UDP_CHECK(srv_fcntl(fd, SRV_F_SETFL, SRV_FD_NONBLOCK) == 0);
        UDP_CHECK(srv_net_udp_recvfrom(fd, bytes, 2, &ip, NULL) == 2 && !memcmp(bytes, "CD", 2));
        UDP_CHECK(!srv_close(alias) && !srv_close(replacement));
    }
    UDP_CHECK(!srv_close(fd) && !srv_close(sender));
    return 0;
#undef UDP_CHECK
}

int netabi_rights_test(void);

int main(void) {
    if (srv_net_dhcp() < 0) {
        return fail("dhcp");
    }
    if (check_udp_mapping(0) || check_udp_mapping(1) || check_udp_mapping(2)) return 1;
    puts("netabi: UDP mapping faults preserve datagrams and replacement descriptors");
    if (netabi_rights_test() != 0 || check_udp_alias() != 0 ||
        check_call_signal() != 0 ||
        check_call_flags() != 0 ||
        check_udp_datagrams() != 0 ||
        check_udp_peers() != 0 ||
        check_status_truncation() != 0 ||
        check_net_list_truncation() != 0 ||
        check_arp_list_truncation() != 0) {
        return 1;
    }
    printf("netabi: ok\n");
    return 0;
}
