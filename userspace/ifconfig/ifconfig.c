#include <srvros/sys.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

int main(int argc, char **argv) {
    struct srv_net_status_info info;
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("usage: ifconfig");
        return 0;
    }
    if (srv_net_status_info(&info) < 0) {
        printf("ifconfig: net status failed\n");
        return 1;
    }

    printf("e1000: flags=%s,%s dhcp=%s\n",
        info.initialized != 0 ? "UP" : "DOWN",
        info.worker_started != 0 ? "RUNNING" : "NO-CARRIER",
        info.dhcp_configured != 0 ? "yes" : "no");
    printf("  inet ");
    print_ip(info.local_ip);
    printf("\n  ether ");
    print_mac(info.local_mac);
    printf("\n");
    printf("  rx frames %llu tx frames %llu\n",
        (unsigned long long)info.rx_frames,
        (unsigned long long)info.tx_frames);
    printf("  arp %llu ipv4 %llu icmp %llu tcp %llu udp %llu\n",
        (unsigned long long)info.arp_packets,
        (unsigned long long)info.ipv4_packets,
        (unsigned long long)info.icmp_packets,
        (unsigned long long)info.tcp_packets,
        (unsigned long long)info.udp_packets);
    printf("  listeners %llu tcp_conn %llu/%llu time_wait %llu udp_sock %llu pending %llu\n",
        (unsigned long long)info.listener_count,
        (unsigned long long)info.tcp_connection_count,
        (unsigned long long)info.tcp_connection_capacity,
        (unsigned long long)info.tcp_time_wait_count,
        (unsigned long long)info.udp_socket_count,
        (unsigned long long)info.pending_count);
    printf("  tcp pressure full %llu time_wait_reaped %llu close_timeout %llut time_wait %llut\n",
        (unsigned long long)info.tcp_table_full_drops,
        (unsigned long long)info.tcp_time_wait_reaped,
        (unsigned long long)info.tcp_close_timeout_ticks,
        (unsigned long long)info.tcp_time_wait_ticks);
    printf("  worker polls %llu empty %llu packets %llu irq %llu recovery %llu gen %llu\n",
        (unsigned long long)info.rx_worker_polls,
        (unsigned long long)info.rx_worker_empty_polls,
        (unsigned long long)info.rx_worker_packets,
        (unsigned long long)info.rx_irq_wakeups,
        (unsigned long long)info.rx_recovery_wakeups,
        (unsigned long long)info.rx_signal_generation);
    return 0;
}
