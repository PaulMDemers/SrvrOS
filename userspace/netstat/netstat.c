#include <srvros/sys.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *protocol_name(uint64_t kind) {
    if (kind == SRV_NET_KIND_UDP_SOCKET) {
        return "udp";
    }
    return "tcp";
}

static void format_endpoint(char *buffer, size_t capacity, uint32_t ip, uint16_t port) {
    if (capacity == 0) {
        return;
    }
    if (ip == 0 && port == 0) {
        snprintf(buffer, capacity, "*:*");
        return;
    }
    if (ip == 0) {
        snprintf(buffer, capacity, "*:%u", (unsigned)port);
        return;
    }
    snprintf(buffer,
        capacity,
        "%u.%u.%u.%u:%u",
        (unsigned)((ip >> 24) & 0xff),
        (unsigned)((ip >> 16) & 0xff),
        (unsigned)((ip >> 8) & 0xff),
        (unsigned)(ip & 0xff),
        (unsigned)port);
}

static void format_flags(char *buffer, size_t capacity, uint8_t flags) {
    size_t used = 0;
    if (capacity == 0) {
        return;
    }
    if (flags == 0) {
        snprintf(buffer, capacity, "-");
        return;
    }

    struct flag_name {
        uint8_t bit;
        char letter;
    };
    static const struct flag_name names[] = {
        {SRV_NET_FLAG_PENDING, 'P'},
        {SRV_NET_FLAG_ACCEPTED, 'A'},
        {SRV_NET_FLAG_OUTBOUND, 'O'},
        {SRV_NET_FLAG_PEER_CLOSED, 'p'},
        {SRV_NET_FLAG_READ_CLOSED, 'r'},
        {SRV_NET_FLAG_WRITE_CLOSED, 'w'},
        {SRV_NET_FLAG_FIN_PENDING, 'F'},
        {SRV_NET_FLAG_RESET, 'R'},
    };

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]) && used + 1 < capacity; i++) {
        if ((flags & names[i].bit) != 0) {
            buffer[used++] = names[i].letter;
        }
    }
    buffer[used] = '\0';
}

int main(int argc, char **argv) {
    uint64_t index = 0;
    uint64_t count = 0;
    struct srv_net_info info;

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("usage: netstat");
        return 0;
    }
    printf("Proto State        PID Local                 Remote                RX/CAP     TX/AVL     Win Err Flags\n");
    for (;;) {
        long next = srv_net_list(index, &info);
        if (next < 0) {
            printf("netstat: net list failed\n");
            return 1;
        }
        if (next == 0) {
            break;
        }

        char local[32];
        char remote[32];
        char flags[16];
        format_endpoint(local, sizeof(local), info.local_ip, info.local_port);
        format_endpoint(remote, sizeof(remote), info.remote_ip, info.remote_port);
        format_flags(flags, sizeof(flags), info.flags);
        printf("%-5s %-12s %3llu %-21s %-21s %4llu/%-4llu %4llu/%-4llu %5u %3u %s\n",
            protocol_name(info.kind),
            info.state_name,
            (unsigned long long)info.pid,
            local,
            remote,
            (unsigned long long)info.rx_queued,
            (unsigned long long)info.rx_capacity,
            (unsigned long long)info.tx_outstanding,
            (unsigned long long)info.tx_available,
            (unsigned)info.peer_window,
            (unsigned)info.error,
            flags);

        count++;
        index = (uint64_t)next;
    }

    if (count == 0) {
        printf("netstat: no sockets\n");
    }
    return 0;
}
