#include <srvros/console.h>
#include <srvros/e1000.h>
#include <srvros/heap.h>
#include <srvros/net.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/syscall_numbers.h>
#include <srvros/timer.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ETH_HEADER_SIZE 14
#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP 0x0806

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

#define TCP_STATE_EMPTY 0
#define TCP_STATE_SYN_RECEIVED 1
#define TCP_STATE_ESTABLISHED 2
#define TCP_STATE_SYN_SENT 3
#define TCP_STATE_FIN_WAIT_1 4
#define TCP_STATE_FIN_WAIT_2 5
#define TCP_STATE_CLOSE_WAIT 6
#define TCP_STATE_LAST_ACK 7
#define TCP_STATE_TIME_WAIT 8

#define TCP_MAX_CONNECTIONS 32
#define UDP_MAX_SOCKETS 8
#define UDP_RX_QUEUE 4
#define UDP_PAYLOAD_MAX 1472
#define NET_LISTENERS 4
#define NET_PENDING_REQUESTS 8
#define NET_CONNECTION_RX_INITIAL_SIZE 4096
#define NET_CONNECTION_RX_MAX_SIZE 32768
#define TCP_RX_WINDOW 4096
#define TCP_SEGMENT_PAYLOAD_MAX 1400
#define TCP_TX_HISTORY 16
#define TCP_TX_WINDOW_BYTES (TCP_SEGMENT_PAYLOAD_MAX * 4)
#define TCP_RETRANSMIT_TICKS 25
#define TCP_RETRANSMIT_MAX 5
#define TCP_ZERO_WINDOW_TICKS 25
#define TCP_ZERO_WINDOW_MAX 12
#define NET_RX_RECOVERY_TICKS 50
#define TCP_CONNECT_TIMEOUT_TICKS 200
#define TCP_CLOSE_TIMEOUT_TICKS 200
#define TCP_TIME_WAIT_TICKS 80
#define TCP_IDLE_TIMEOUT_TICKS 6000
#define TCP_CLIENT_PORT_FIRST 49152
#define TCP_CLIENT_PORT_LAST 60999
#define ARP_CACHE_ENTRIES 8
#define ICMP_PING_ID 0x7372
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_ACK 5
#define DHCP_TIMEOUT_TICKS 200
#define DNS_PORT 53
#define DNS_CLIENT_PORT 53000
#define DNS_TIMEOUT_TICKS 100
#define DNS_RETRIES 3

#define NET_HANDLE_LISTENER 0x1000000000000000ull
#define NET_HANDLE_CONNECTION 0x2000000000000000ull
#define NET_HANDLE_UDP_SOCKET 0x3000000000000000ull
#define NET_HANDLE_MASK 0xf000000000000000ull
#define NET_HANDLE_VALUE_MASK 0x0fffffffffffffffull

struct net_listener {
    bool used;
    uint64_t id;
    struct process *owner;
    uint16_t port;
};

struct tcp_tx_record {
    bool used;
    uint8_t flags;
    uint8_t retries;
    uint16_t local_port;
    uint16_t payload_length;
    uint32_t seq;
    uint64_t deadline;
    uint8_t payload[TCP_SEGMENT_PAYLOAD_MAX];
};

struct tcp_connection {
    uint8_t state;
    bool pending;
    bool accepted;
    bool outbound;
    bool peer_closed;
    bool local_read_closed;
    bool local_write_closed;
    bool fin_pending;
    bool reset;
    bool request_counted;
    uint16_t error;
    uint8_t remote_mac[6];
    struct process *owner;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint16_t peer_window;
    uint32_t local_seq;
    uint32_t remote_next;
    uint64_t id;
    uint8_t tx_next;
    uint64_t rx_length;
    uint64_t rx_offset;
    uint64_t rx_capacity;
    uint64_t close_deadline;
    uint64_t last_activity_tick;
    uint64_t zero_window_deadline;
    char *rx_data;
    uint8_t zero_window_probes;
    struct tcp_tx_record tx_history[TCP_TX_HISTORY];
};

struct pending_request {
    bool used;
    struct process *owner;
    uint64_t connection_index;
    uint64_t connection_id;
    uint16_t port;
    uint64_t length;
};

struct udp_datagram {
    uint32_t remote_ip;
    uint16_t remote_port;
    uint16_t length;
    uint8_t data[UDP_PAYLOAD_MAX];
};

struct udp_socket {
    bool used;
    uint64_t id;
    struct process *owner;
    uint16_t local_port;
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    struct udp_datagram queue[UDP_RX_QUEUE];
};

struct arp_cache_entry {
    bool used;
    uint32_t ip;
    uint8_t mac[6];
    uint64_t updated_tick;
};

static uint8_t local_ip_bytes[4] = {10, 0, 2, 15};
static uint8_t dhcp_server_ip_bytes[4];
static uint8_t dhcp_router_ip_bytes[4];
static uint8_t dhcp_dns_ip_bytes[4];
static uint8_t dhcp_offered_ip_bytes[4];
static uint8_t arp_resolved_mac[6];
static uint32_t arp_resolved_ip;
static bool arp_resolved;
static bool arp_waiting;
static bool icmp_ping_waiting;
static bool icmp_ping_reply;
static uint32_t icmp_ping_ip;
static uint16_t icmp_ping_sequence;
static uint64_t icmp_ping_start_tick;
static uint32_t dns_answer_ip;
static uint16_t dns_query_id = 0x7372;
static bool dns_waiting;
static bool dns_answer_ready;
static bool dns_response_done;
static bool dhcp_configured;
static bool dhcp_in_progress;
static bool dhcp_have_offer;
static bool dhcp_request_sent;
static uint32_t dhcp_xid = 0x73727672u;
static uint32_t local_ip;
static const uint8_t *local_mac;
static bool initialized;
static bool worker_started;
static uint16_t ip_ident;
static uint64_t rx_frames;
static uint64_t tx_frames;
static uint64_t arp_packets;
static uint64_t ipv4_packets;
static uint64_t icmp_packets;
static uint64_t tcp_packets;
static uint64_t udp_packets;
static uint64_t http_requests;
static uint64_t accepted_requests;
static uint64_t completed_requests;
static uint64_t tcp_table_full_drops;
static uint64_t tcp_time_wait_reaped;
static uint64_t next_listener_id = 1;
static uint64_t next_connection_id = 1;
static uint64_t next_udp_socket_id = 1;
static uint16_t next_client_port = TCP_CLIENT_PORT_FIRST;
static uint16_t next_udp_port = TCP_CLIENT_PORT_FIRST;
static struct net_listener listeners[NET_LISTENERS];
static struct tcp_connection tcp_connections[TCP_MAX_CONNECTIONS];
static struct pending_request pending_requests[NET_PENDING_REQUESTS];
static struct udp_socket udp_sockets[UDP_MAX_SOCKETS];
static struct arp_cache_entry arp_cache[ARP_CACHE_ENTRIES];
static struct scheduler_wait_queue accept_wait_queue;
static struct scheduler_wait_queue read_wait_queue;
static struct scheduler_wait_queue write_wait_queue;
static struct scheduler_wait_queue connect_wait_queue;
static struct scheduler_wait_queue udp_wait_queue;
static struct scheduler_wait_queue rx_wait_queue;
static volatile uint64_t rx_signal_generation;
static uint64_t rx_irq_wakeups;
static uint64_t rx_recovery_wakeups;
static uint64_t rx_worker_polls;
static uint64_t rx_worker_empty_polls;
static uint64_t rx_worker_packets;
static uint64_t rx_last_recovery_tick;

static void close_connection(struct tcp_connection *connection);

static uint64_t make_handle(uint64_t type, uint64_t value) {
    return type | (value & NET_HANDLE_VALUE_MASK);
}

static uint64_t handle_type(uint64_t handle) {
    return handle & NET_HANDLE_MASK;
}

static uint64_t handle_value(uint64_t handle) {
    return handle & NET_HANDLE_VALUE_MASK;
}

static uint16_t read_be16(const uint8_t *data) {
    return ((uint16_t)data[0] << 8) | data[1];
}

static uint32_t read_be32(const uint8_t *data) {
    return ((uint32_t)data[0] << 24) |
        ((uint32_t)data[1] << 16) |
        ((uint32_t)data[2] << 8) |
        data[3];
}

static void write_be16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

static void write_be32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        dst[i] = src[i];
    }
}

static void clear_bytes(uint8_t *dst, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        dst[i] = 0;
    }
}

static void set_local_ip_bytes(const uint8_t *bytes) {
    copy_bytes(local_ip_bytes, bytes, 4);
    local_ip = read_be32(local_ip_bytes);
}

static bool mac_equals(const uint8_t *a, const uint8_t *b) {
    for (uint64_t i = 0; i < 6; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static bool is_for_local_mac(const uint8_t *mac) {
    bool broadcast = true;
    for (uint64_t i = 0; i < 6; i++) {
        if (mac[i] != 0xff) {
            broadcast = false;
        }
    }

    return broadcast || mac_equals(mac, local_mac);
}

static uint16_t checksum_bytes(const uint8_t *data, uint32_t length, uint32_t initial) {
    uint32_t sum = initial;
    uint32_t i = 0;

    while (i + 1 < length) {
        sum += ((uint16_t)data[i] << 8) | data[i + 1];
        i += 2;
    }
    if (i < length) {
        sum += ((uint16_t)data[i] << 8);
    }

    while ((sum >> 16) != 0) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

static uint32_t checksum_add_bytes(const uint8_t *data, uint32_t length, uint32_t sum) {
    uint32_t i = 0;
    while (i + 1 < length) {
        sum += ((uint16_t)data[i] << 8) | data[i + 1];
        i += 2;
    }
    if (i < length) {
        sum += ((uint16_t)data[i] << 8);
    }
    return sum;
}

static uint16_t checksum_finish(uint32_t sum) {
    while ((sum >> 16) != 0) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip, const uint8_t *tcp, uint16_t tcp_length) {
    uint32_t sum = 0;
    sum += (src_ip >> 16) & 0xffff;
    sum += src_ip & 0xffff;
    sum += (dst_ip >> 16) & 0xffff;
    sum += dst_ip & 0xffff;
    sum += IP_PROTO_TCP;
    sum += tcp_length;
    sum = checksum_add_bytes(tcp, tcp_length, sum);
    return checksum_finish(sum);
}

static uint16_t udp_checksum(uint32_t src_ip, uint32_t dst_ip, const uint8_t *udp, uint16_t udp_length) {
    uint32_t sum = 0;
    sum += (src_ip >> 16) & 0xffff;
    sum += src_ip & 0xffff;
    sum += (dst_ip >> 16) & 0xffff;
    sum += dst_ip & 0xffff;
    sum += IP_PROTO_UDP;
    sum += udp_length;
    sum = checksum_add_bytes(udp, udp_length, sum);
    return checksum_finish(sum);
}

static bool send_ethernet(uint8_t *frame, uint16_t length) {
    if (e1000_send_frame(frame, length)) {
        tx_frames++;
        return true;
    }
    return false;
}

static bool send_arp_reply(const uint8_t *request) {
    uint8_t frame[42];
    const uint8_t *sender_mac = request + 22;
    const uint8_t *sender_ip = request + 28;

    copy_bytes(frame + 0, sender_mac, 6);
    copy_bytes(frame + 6, local_mac, 6);
    write_be16(frame + 12, ETH_TYPE_ARP);

    write_be16(frame + 14, 1);
    write_be16(frame + 16, ETH_TYPE_IPV4);
    frame[18] = 6;
    frame[19] = 4;
    write_be16(frame + 20, 2);
    copy_bytes(frame + 22, local_mac, 6);
    copy_bytes(frame + 28, local_ip_bytes, 4);
    copy_bytes(frame + 32, sender_mac, 6);
    copy_bytes(frame + 38, sender_ip, 4);

    return send_ethernet(frame, sizeof(frame));
}

static bool send_arp_request(uint32_t target_ip) {
    uint8_t frame[42];
    for (uint64_t i = 0; i < 6; i++) {
        frame[i] = 0xff;
    }
    copy_bytes(frame + 6, local_mac, 6);
    write_be16(frame + 12, ETH_TYPE_ARP);

    write_be16(frame + 14, 1);
    write_be16(frame + 16, ETH_TYPE_IPV4);
    frame[18] = 6;
    frame[19] = 4;
    write_be16(frame + 20, 1);
    copy_bytes(frame + 22, local_mac, 6);
    copy_bytes(frame + 28, local_ip_bytes, 4);
    clear_bytes(frame + 32, 6);
    write_be32(frame + 38, target_ip);
    return send_ethernet(frame, sizeof(frame));
}

static void arp_cache_update(uint32_t ip, const uint8_t *mac) {
    if (ip == 0 || mac == 0) {
        return;
    }

    uint64_t oldest = 0;
    uint64_t oldest_tick = UINT64_MAX;
    for (uint64_t i = 0; i < ARP_CACHE_ENTRIES; i++) {
        if (arp_cache[i].used && arp_cache[i].ip == ip) {
            copy_bytes(arp_cache[i].mac, mac, sizeof(arp_cache[i].mac));
            arp_cache[i].updated_tick = timer_ticks();
            return;
        }
        if (!arp_cache[i].used) {
            oldest = i;
            oldest_tick = 0;
        } else if (oldest_tick != 0 && arp_cache[i].updated_tick < oldest_tick) {
            oldest = i;
            oldest_tick = arp_cache[i].updated_tick;
        }
    }

    arp_cache[oldest].used = true;
    arp_cache[oldest].ip = ip;
    copy_bytes(arp_cache[oldest].mac, mac, sizeof(arp_cache[oldest].mac));
    arp_cache[oldest].updated_tick = timer_ticks();
}

static bool arp_cache_lookup(uint32_t ip, uint8_t *mac_out) {
    if (ip == 0 || mac_out == 0) {
        return false;
    }
    for (uint64_t i = 0; i < ARP_CACHE_ENTRIES; i++) {
        if (arp_cache[i].used && arp_cache[i].ip == ip) {
            copy_bytes(mac_out, arp_cache[i].mac, sizeof(arp_cache[i].mac));
            return true;
        }
    }
    return false;
}

static bool send_ipv4_packet_from(const uint8_t *dst_mac,
    const uint8_t *src_ip_bytes,
    uint32_t dst_ip,
    uint8_t protocol,
    const uint8_t *payload,
    uint16_t payload_length) {
    uint8_t frame[1514];
    uint16_t total_length = 20 + payload_length;

    if (total_length + ETH_HEADER_SIZE > sizeof(frame)) {
        return false;
    }

    copy_bytes(frame + 0, dst_mac, 6);
    copy_bytes(frame + 6, local_mac, 6);
    write_be16(frame + 12, ETH_TYPE_IPV4);

    uint8_t *ip = frame + ETH_HEADER_SIZE;
    ip[0] = 0x45;
    ip[1] = 0;
    write_be16(ip + 2, total_length);
    write_be16(ip + 4, ip_ident++);
    write_be16(ip + 6, 0x4000);
    ip[8] = 64;
    ip[9] = protocol;
    write_be16(ip + 10, 0);
    copy_bytes(ip + 12, src_ip_bytes, 4);
    write_be32(ip + 16, dst_ip);
    write_be16(ip + 10, checksum_bytes(ip, 20, 0));

    copy_bytes(ip + 20, payload, payload_length);
    return send_ethernet(frame, ETH_HEADER_SIZE + total_length);
}

static bool send_ipv4_packet(const uint8_t *dst_mac,
    uint32_t dst_ip,
    uint8_t protocol,
    const uint8_t *payload,
    uint16_t payload_length) {
    return send_ipv4_packet_from(dst_mac, local_ip_bytes, dst_ip, protocol, payload, payload_length);
}

static bool send_udp_packet(const uint8_t *dst_mac,
    const uint8_t *src_ip_bytes,
    uint32_t dst_ip,
    uint16_t src_port,
    uint16_t dst_port,
    const uint8_t *payload,
    uint16_t payload_length) {
    uint8_t packet[1500];
    uint16_t udp_length = payload_length + 8;
    if ((uint64_t)udp_length > sizeof(packet)) {
        return false;
    }

    write_be16(packet + 0, src_port);
    write_be16(packet + 2, dst_port);
    write_be16(packet + 4, udp_length);
    write_be16(packet + 6, 0);
    copy_bytes(packet + 8, payload, payload_length);
    return send_ipv4_packet_from(dst_mac, src_ip_bytes, dst_ip, IP_PROTO_UDP, packet, udp_length);
}

static bool resolve_route_mac(uint32_t remote_ip, uint8_t *mac_out);

static bool send_icmp_echo_request(uint32_t remote_ip, uint16_t sequence) {
    uint8_t remote_mac[6];
    if (!resolve_route_mac(remote_ip, remote_mac)) {
        return false;
    }

    uint8_t packet[40];
    clear_bytes(packet, sizeof(packet));
    packet[0] = 8;
    packet[1] = 0;
    write_be16(packet + 4, ICMP_PING_ID);
    write_be16(packet + 6, sequence);
    const char payload[] = "srvros ping";
    copy_bytes(packet + 8, (const uint8_t *)payload, sizeof(payload) - 1);
    write_be16(packet + 2, checksum_bytes(packet, sizeof(packet), 0));
    return send_ipv4_packet(remote_mac, remote_ip, IP_PROTO_ICMP, packet, sizeof(packet));
}

static bool resolve_ipv4_mac(uint32_t ip, uint8_t *mac_out) {
    if (mac_out == 0 || !initialized || local_mac == 0) {
        return false;
    }
    if (arp_cache_lookup(ip, mac_out)) {
        return true;
    }

    arp_resolved_ip = ip;
    arp_resolved = false;
    arp_waiting = true;
    if (!send_arp_request(ip)) {
        arp_waiting = false;
        return false;
    }

    uint64_t start = timer_ticks();
    while (timer_ticks() - start < 100) {
        if (arp_resolved) {
            copy_bytes(mac_out, arp_resolved_mac, 6);
            return true;
        }
        (void)e1000_poll(32, false);
        scheduler_yield();
    }
    arp_waiting = false;
    return false;
}

static uint32_t default_gateway_ip(void) {
    uint32_t router = read_be32(dhcp_router_ip_bytes);
    if (router != 0) {
        return router;
    }
    return ((uint32_t)10 << 24) | ((uint32_t)0 << 16) | ((uint32_t)2 << 8) | 2;
}

static uint32_t route_next_hop(uint32_t remote_ip) {
    if ((remote_ip & 0xffffff00u) == (local_ip & 0xffffff00u)) {
        return remote_ip;
    }
    return default_gateway_ip();
}

static bool resolve_route_mac(uint32_t remote_ip, uint8_t *mac_out) {
    return resolve_ipv4_mac(route_next_hop(remote_ip), mac_out);
}

static bool dns_encode_name(uint8_t *packet, uint64_t *offset, uint64_t capacity, const char *name) {
    uint64_t label_start = 0;
    uint64_t i = 0;
    while (true) {
        if (name[i] == '.' || name[i] == '\0') {
            uint64_t label_length = i - label_start;
            if (label_length == 0 || label_length > 63 || *offset + label_length + 1 >= capacity) {
                return false;
            }
            packet[(*offset)++] = (uint8_t)label_length;
            for (uint64_t j = label_start; j < i; j++) {
                packet[(*offset)++] = (uint8_t)name[j];
            }
            if (name[i] == '\0') {
                if (*offset >= capacity) {
                    return false;
                }
                packet[(*offset)++] = 0;
                return true;
            }
            label_start = i + 1;
        }
        i++;
        if (i > 120) {
            return false;
        }
    }
}

static bool send_dns_query(uint32_t server_ip, const uint8_t *server_mac, const char *name, uint16_t query_id) {
    uint8_t packet[512];
    clear_bytes(packet, sizeof(packet));
    write_be16(packet + 0, query_id);
    write_be16(packet + 2, 0x0100);
    write_be16(packet + 4, 1);

    uint64_t offset = 12;
    if (!dns_encode_name(packet, &offset, sizeof(packet), name) || offset + 4 > sizeof(packet)) {
        return false;
    }
    write_be16(packet + offset, 1);
    offset += 2;
    write_be16(packet + offset, 1);
    offset += 2;

    return send_udp_packet(server_mac,
        local_ip_bytes,
        server_ip,
        DNS_CLIENT_PORT,
        DNS_PORT,
        packet,
        (uint16_t)offset);
}

static uint64_t dns_skip_name(const uint8_t *packet, uint64_t length, uint64_t offset) {
    uint64_t jumps = 0;
    while (offset < length && jumps++ < 64) {
        uint8_t label = packet[offset++];
        if (label == 0) {
            return offset;
        }
        if ((label & 0xc0) == 0xc0) {
            return offset + 1 <= length ? offset + 1 : 0;
        }
        if ((label & 0xc0) != 0 || offset + label > length) {
            return 0;
        }
        offset += label;
    }
    return 0;
}

static void handle_dns_packet(const uint8_t *payload, uint16_t length) {
    if (!dns_waiting || length < 12 || read_be16(payload + 0) != dns_query_id) {
        return;
    }

    uint16_t flags = read_be16(payload + 2);
    uint16_t qdcount = read_be16(payload + 4);
    uint16_t ancount = read_be16(payload + 6);
    if ((flags & 0x8000) == 0 || qdcount == 0) {
        return;
    }
    if ((flags & 0x000f) != 0) {
        dns_waiting = false;
        dns_response_done = true;
        return;
    }

    uint64_t offset = 12;
    for (uint16_t i = 0; i < qdcount; i++) {
        offset = dns_skip_name(payload, length, offset);
        if (offset == 0 || offset + 4 > length) {
            dns_waiting = false;
            dns_response_done = true;
            return;
        }
        offset += 4;
    }

    for (uint16_t i = 0; i < ancount; i++) {
        offset = dns_skip_name(payload, length, offset);
        if (offset == 0 || offset + 10 > length) {
            dns_waiting = false;
            dns_response_done = true;
            return;
        }
        uint16_t type = read_be16(payload + offset);
        uint16_t klass = read_be16(payload + offset + 2);
        uint16_t rdlength = read_be16(payload + offset + 8);
        offset += 10;
        if (offset + rdlength > length) {
            dns_waiting = false;
            dns_response_done = true;
            return;
        }
        if (type == 1 && klass == 1 && rdlength == 4) {
            dns_answer_ip = read_be32(payload + offset);
            dns_answer_ready = true;
            dns_waiting = false;
            dns_response_done = true;
            return;
        }
        offset += rdlength;
    }
    dns_waiting = false;
    dns_response_done = true;
}

static void append_dhcp_option_ip(uint8_t *packet, uint64_t *offset, uint8_t option, const uint8_t *ip) {
    packet[(*offset)++] = option;
    packet[(*offset)++] = 4;
    copy_bytes(packet + *offset, ip, 4);
    *offset += 4;
}

static bool send_dhcp_message(uint8_t message_type, const uint8_t *requested_ip, const uint8_t *server_ip) {
    static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const uint8_t zero_ip[4] = {0, 0, 0, 0};
    uint8_t packet[300];
    clear_bytes(packet, sizeof(packet));

    packet[0] = 1;
    packet[1] = 1;
    packet[2] = 6;
    write_be32(packet + 4, dhcp_xid);
    write_be16(packet + 10, 0x8000);
    copy_bytes(packet + 28, local_mac, 6);
    packet[236] = 0x63;
    packet[237] = 0x82;
    packet[238] = 0x53;
    packet[239] = 0x63;

    uint64_t offset = 240;
    packet[offset++] = 53;
    packet[offset++] = 1;
    packet[offset++] = message_type;
    packet[offset++] = 61;
    packet[offset++] = 7;
    packet[offset++] = 1;
    copy_bytes(packet + offset, local_mac, 6);
    offset += 6;
    if (message_type == DHCP_REQUEST && requested_ip != 0) {
        append_dhcp_option_ip(packet, &offset, 50, requested_ip);
    }
    if (message_type == DHCP_REQUEST && server_ip != 0) {
        append_dhcp_option_ip(packet, &offset, 54, server_ip);
    }
    packet[offset++] = 55;
    packet[offset++] = 4;
    packet[offset++] = 1;
    packet[offset++] = 3;
    packet[offset++] = 6;
    packet[offset++] = 15;
    packet[offset++] = 255;

    return send_udp_packet(broadcast_mac,
        zero_ip,
        0xffffffffu,
        DHCP_CLIENT_PORT,
        DHCP_SERVER_PORT,
        packet,
        sizeof(packet));
}

static void handle_dhcp_packet(const uint8_t *payload, uint16_t length) {
    if (!dhcp_in_progress || length < 240 || read_be32(payload + 4) != dhcp_xid) {
        return;
    }
    if (payload[0] != 2 || payload[1] != 1 || payload[2] != 6 ||
        payload[236] != 0x63 || payload[237] != 0x82 ||
        payload[238] != 0x53 || payload[239] != 0x63) {
        return;
    }

    uint8_t message_type = 0;
    uint8_t server_id[4] = {0, 0, 0, 0};
    uint8_t router[4] = {0, 0, 0, 0};
    uint8_t dns[4] = {0, 0, 0, 0};
    uint64_t offset = 240;
    while (offset < length) {
        uint8_t option = payload[offset++];
        if (option == 0) {
            continue;
        }
        if (option == 255 || offset >= length) {
            break;
        }
        uint8_t option_length = payload[offset++];
        if (offset + option_length > length) {
            break;
        }
        if (option == 53 && option_length >= 1) {
            message_type = payload[offset];
        } else if (option == 54 && option_length >= 4) {
            copy_bytes(server_id, payload + offset, 4);
        } else if (option == 3 && option_length >= 4) {
            copy_bytes(router, payload + offset, 4);
        } else if (option == 6 && option_length >= 4) {
            copy_bytes(dns, payload + offset, 4);
        }
        offset += option_length;
    }

    if (message_type == DHCP_OFFER && !dhcp_request_sent) {
        copy_bytes(dhcp_offered_ip_bytes, payload + 16, 4);
        copy_bytes(dhcp_server_ip_bytes, server_id, 4);
        dhcp_have_offer = true;
        dhcp_request_sent = send_dhcp_message(DHCP_REQUEST, dhcp_offered_ip_bytes, dhcp_server_ip_bytes);
    } else if (message_type == DHCP_ACK) {
        set_local_ip_bytes(payload + 16);
        copy_bytes(dhcp_server_ip_bytes, server_id, 4);
        copy_bytes(dhcp_router_ip_bytes, router, 4);
        copy_bytes(dhcp_dns_ip_bytes, dns, 4);
        dhcp_configured = true;
        dhcp_in_progress = false;
        console_printf("net: dhcp ipv4=%u.%u.%u.%u\n",
            local_ip_bytes[0],
            local_ip_bytes[1],
            local_ip_bytes[2],
            local_ip_bytes[3]);
    }
}

static void handle_arp(const uint8_t *frame, uint16_t length) {
    if (length < 42) {
        return;
    }

    const uint8_t *arp = frame + ETH_HEADER_SIZE;
    if (read_be16(arp + 0) != 1 ||
        read_be16(arp + 2) != ETH_TYPE_IPV4 ||
        arp[4] != 6 ||
        arp[5] != 4) {
        return;
    }

    arp_packets++;
    uint16_t opcode = read_be16(arp + 6);
    uint32_t sender_ip = read_be32(arp + 14);
    arp_cache_update(sender_ip, arp + 8);
    if (opcode == 2) {
        if (arp_waiting && sender_ip == arp_resolved_ip) {
            copy_bytes(arp_resolved_mac, arp + 8, 6);
            arp_resolved = true;
            arp_waiting = false;
        }
    }
    if (opcode == 1 && read_be32(arp + 24) == local_ip) {
        send_arp_reply(frame);
    }
}

static void handle_icmp(const uint8_t *frame,
    uint16_t length,
    const uint8_t *ip,
    uint16_t ip_header_length,
    uint16_t ip_payload_length,
    uint32_t src_ip) {
    if (ip_payload_length < 8 || length < ETH_HEADER_SIZE + ip_header_length + ip_payload_length) {
        return;
    }

    const uint8_t *icmp = ip + ip_header_length;
    icmp_packets++;

    if (icmp[0] == 0) {
        if (checksum_bytes(icmp, ip_payload_length, 0) == 0 &&
            icmp_ping_waiting &&
            src_ip == icmp_ping_ip &&
            read_be16(icmp + 4) == ICMP_PING_ID &&
            read_be16(icmp + 6) == icmp_ping_sequence) {
            icmp_ping_reply = true;
            icmp_ping_waiting = false;
        }
        return;
    }

    if (icmp[0] != 8) {
        return;
    }

    uint8_t reply[1480];
    if (ip_payload_length > sizeof(reply)) {
        return;
    }

    copy_bytes(reply, icmp, ip_payload_length);
    reply[0] = 0;
    reply[2] = 0;
    reply[3] = 0;
    write_be16(reply + 2, checksum_bytes(reply, ip_payload_length, 0));
    send_ipv4_packet(frame + 6, src_ip, IP_PROTO_ICMP, reply, ip_payload_length);
}

static struct udp_socket *find_udp_socket(uint64_t socket_id) {
    if (handle_type(socket_id) != NET_HANDLE_UDP_SOCKET) {
        return 0;
    }

    uint64_t value = handle_value(socket_id);
    for (uint64_t i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_sockets[i].used && udp_sockets[i].id == value) {
            return &udp_sockets[i];
        }
    }
    return 0;
}

static struct udp_socket *find_udp_socket_for_port(uint16_t port) {
    if (port == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_sockets[i].used && udp_sockets[i].local_port == port) {
            return &udp_sockets[i];
        }
    }
    return 0;
}

static bool udp_port_reserved(uint16_t port) {
    return port == DHCP_CLIENT_PORT || port == DNS_CLIENT_PORT;
}

static uint16_t allocate_udp_port(void) {
    for (uint64_t tries = 0; tries <= TCP_CLIENT_PORT_LAST - TCP_CLIENT_PORT_FIRST; tries++) {
        uint16_t port = next_udp_port++;
        if (next_udp_port > TCP_CLIENT_PORT_LAST) {
            next_udp_port = TCP_CLIENT_PORT_FIRST;
        }
        if (!udp_port_reserved(port) && find_udp_socket_for_port(port) == 0) {
            return port;
        }
    }
    return 0;
}

static bool udp_queue_push(struct udp_socket *socket,
    uint32_t remote_ip,
    uint16_t remote_port,
    const uint8_t *payload,
    uint16_t length) {
    if (socket == 0 || (payload == 0 && length != 0) || length > UDP_PAYLOAD_MAX) {
        return false;
    }

    if (socket->count == UDP_RX_QUEUE) {
        socket->head = (uint8_t)((socket->head + 1) % UDP_RX_QUEUE);
        socket->count--;
    }

    struct udp_datagram *datagram = &socket->queue[socket->tail];
    datagram->remote_ip = remote_ip;
    datagram->remote_port = remote_port;
    datagram->length = length;
    copy_bytes(datagram->data, payload, length);
    socket->tail = (uint8_t)((socket->tail + 1) % UDP_RX_QUEUE);
    socket->count++;
    return true;
}

static void udp_deliver(uint16_t dst_port,
    uint32_t remote_ip,
    uint16_t remote_port,
    const uint8_t *payload,
    uint16_t length) {
    struct udp_socket *socket = find_udp_socket_for_port(dst_port);
    if (socket == 0) {
        return;
    }
    if (udp_queue_push(socket, remote_ip, remote_port, payload, length)) {
        scheduler_wake_all(&udp_wait_queue);
        process_file_poll_wake();
    }
}

static void handle_udp(const uint8_t *ip, uint16_t ip_header_length, uint16_t ip_payload_length) {
    if (ip_payload_length < 8) {
        return;
    }

    const uint8_t *udp = ip + ip_header_length;
    uint16_t src_port = read_be16(udp + 0);
    uint16_t dst_port = read_be16(udp + 2);
    uint16_t udp_length = read_be16(udp + 4);
    if (udp_length < 8 || udp_length > ip_payload_length) {
        return;
    }
    uint16_t checksum = read_be16(udp + 6);
    if (checksum != 0 && udp_checksum(read_be32(ip + 12), read_be32(ip + 16), udp, udp_length) != 0) {
        return;
    }
    udp_packets++;

    if (src_port == DHCP_SERVER_PORT && dst_port == DHCP_CLIENT_PORT) {
        handle_dhcp_packet(udp + 8, udp_length - 8);
    } else if (src_port == DNS_PORT && dst_port == DNS_CLIENT_PORT) {
        handle_dns_packet(udp + 8, udp_length - 8);
    }

    udp_deliver(dst_port, read_be32(ip + 12), src_port, udp + 8, udp_length - 8);
}

static struct tcp_connection *find_connection(uint16_t local_port, uint32_t remote_ip, uint16_t remote_port) {
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (tcp_connections[i].state != TCP_STATE_EMPTY &&
            tcp_connections[i].local_port == local_port &&
            tcp_connections[i].remote_ip == remote_ip &&
            tcp_connections[i].remote_port == remote_port) {
            return &tcp_connections[i];
        }
    }

    return 0;
}

static struct net_listener *find_listener_for_port(uint16_t port) {
    for (uint64_t i = 0; i < NET_LISTENERS; i++) {
        if (listeners[i].used && listeners[i].port == port) {
            return &listeners[i];
        }
    }

    return 0;
}

static struct net_listener *find_listener(uint64_t listener_id) {
    if (handle_type(listener_id) != NET_HANDLE_LISTENER) {
        return 0;
    }

    uint64_t value = handle_value(listener_id);
    for (uint64_t i = 0; i < NET_LISTENERS; i++) {
        if (listeners[i].used && listeners[i].id == value) {
            return &listeners[i];
        }
    }

    return 0;
}

static bool port_is_listening(uint16_t port) {
    return find_listener_for_port(port) != 0;
}

static bool tcp_reap_expired_time_wait(uint64_t now) {
    bool reaped = false;
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        struct tcp_connection *connection = &tcp_connections[i];
        if (connection->state == TCP_STATE_TIME_WAIT &&
            connection->close_deadline != 0 &&
            now >= connection->close_deadline) {
            close_connection(connection);
            tcp_time_wait_reaped++;
            reaped = true;
        }
    }
    return reaped;
}

static bool tcp_reclaim_oldest_time_wait(uint64_t now) {
    struct tcp_connection *oldest = 0;
    uint64_t oldest_deadline = UINT64_MAX;
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        struct tcp_connection *connection = &tcp_connections[i];
        if (connection->state != TCP_STATE_TIME_WAIT) {
            continue;
        }
        if (connection->close_deadline != 0 && now >= connection->close_deadline) {
            close_connection(connection);
            tcp_time_wait_reaped++;
            return true;
        }
        if (connection->close_deadline < oldest_deadline) {
            oldest_deadline = connection->close_deadline;
            oldest = connection;
        }
    }
    if (oldest == 0) {
        return false;
    }
    close_connection(oldest);
    tcp_time_wait_reaped++;
    return true;
}

static struct tcp_connection *alloc_connection(uint16_t local_port,
    uint32_t remote_ip,
    uint16_t remote_port,
    const uint8_t *remote_mac) {
    struct net_listener *listener = find_listener_for_port(local_port);
    if (listener == 0) {
        return 0;
    }

    uint64_t now = timer_ticks();
    (void)tcp_reap_expired_time_wait(now);

    struct tcp_connection *connection = find_connection(local_port, remote_ip, remote_port);
    if (connection != 0) {
        if (connection->state == TCP_STATE_TIME_WAIT) {
            return 0;
        }
        return connection;
    }

retry:
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (tcp_connections[i].state == TCP_STATE_EMPTY) {
            tcp_connections[i].state = TCP_STATE_SYN_RECEIVED;
            tcp_connections[i].pending = false;
            tcp_connections[i].accepted = false;
            tcp_connections[i].outbound = false;
            tcp_connections[i].peer_closed = false;
            tcp_connections[i].local_read_closed = false;
            tcp_connections[i].local_write_closed = false;
            tcp_connections[i].fin_pending = false;
            tcp_connections[i].reset = false;
            tcp_connections[i].request_counted = false;
            tcp_connections[i].error = SRV_NETERR_NONE;
            tcp_connections[i].owner = listener->owner;
            tcp_connections[i].remote_ip = remote_ip;
            tcp_connections[i].local_port = local_port;
            tcp_connections[i].remote_port = remote_port;
            tcp_connections[i].peer_window = TCP_RX_WINDOW;
            tcp_connections[i].local_seq = 0x73720000u + (uint32_t)remote_port;
            tcp_connections[i].remote_next = 0;
            tcp_connections[i].id = next_connection_id++;
            tcp_connections[i].tx_next = 0;
            tcp_connections[i].close_deadline = 0;
            tcp_connections[i].last_activity_tick = timer_ticks();
            tcp_connections[i].zero_window_deadline = 0;
            tcp_connections[i].zero_window_probes = 0;
            clear_bytes((uint8_t *)tcp_connections[i].tx_history, sizeof(tcp_connections[i].tx_history));
            if (next_connection_id == 0) {
                next_connection_id = 1;
            }
            copy_bytes(tcp_connections[i].remote_mac, remote_mac, 6);
            return &tcp_connections[i];
        }
    }

    if (tcp_reclaim_oldest_time_wait(now)) {
        goto retry;
    }

    tcp_table_full_drops++;
    return 0;
}

static bool local_port_available(uint16_t port) {
    if (port_is_listening(port)) {
        return false;
    }
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (tcp_connections[i].state != TCP_STATE_EMPTY &&
            tcp_connections[i].local_port == port) {
            return false;
        }
    }
    return true;
}

static uint16_t allocate_client_port(void) {
    for (uint32_t attempts = 0; attempts <= TCP_CLIENT_PORT_LAST - TCP_CLIENT_PORT_FIRST; attempts++) {
        uint16_t port = next_client_port++;
        if (next_client_port > TCP_CLIENT_PORT_LAST) {
            next_client_port = TCP_CLIENT_PORT_FIRST;
        }
        if (local_port_available(port)) {
            return port;
        }
    }
    return 0;
}

static struct tcp_connection *find_connection_by_id_owner(uint64_t id, struct process *owner) {
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (tcp_connections[i].state != TCP_STATE_EMPTY &&
            tcp_connections[i].id == id &&
            tcp_connections[i].owner == owner) {
            return &tcp_connections[i];
        }
    }
    return 0;
}

static struct tcp_connection *alloc_client_connection(uint32_t remote_ip,
    uint16_t remote_port,
    const uint8_t *remote_mac) {
    struct process *owner = process_current();
    uint64_t now = timer_ticks();
    uint16_t local_port;
    (void)tcp_reap_expired_time_wait(now);

retry:
    local_port = allocate_client_port();
    if (owner == 0 || local_port == 0) {
        if (owner != 0) {
            tcp_table_full_drops++;
        }
        return 0;
    }

    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (tcp_connections[i].state == TCP_STATE_EMPTY) {
            tcp_connections[i].state = TCP_STATE_SYN_SENT;
            tcp_connections[i].pending = false;
            tcp_connections[i].accepted = true;
            tcp_connections[i].outbound = true;
            tcp_connections[i].peer_closed = false;
            tcp_connections[i].local_read_closed = false;
            tcp_connections[i].local_write_closed = false;
            tcp_connections[i].fin_pending = false;
            tcp_connections[i].reset = false;
            tcp_connections[i].request_counted = false;
            tcp_connections[i].error = SRV_NETERR_NONE;
            tcp_connections[i].owner = owner;
            tcp_connections[i].remote_ip = remote_ip;
            tcp_connections[i].local_port = local_port;
            tcp_connections[i].remote_port = remote_port;
            tcp_connections[i].peer_window = TCP_RX_WINDOW;
            tcp_connections[i].local_seq = 0x63720000u + (uint32_t)local_port;
            tcp_connections[i].remote_next = 0;
            tcp_connections[i].id = next_connection_id++;
            tcp_connections[i].tx_next = 0;
            tcp_connections[i].close_deadline = 0;
            tcp_connections[i].last_activity_tick = timer_ticks();
            tcp_connections[i].zero_window_deadline = 0;
            tcp_connections[i].zero_window_probes = 0;
            clear_bytes((uint8_t *)tcp_connections[i].tx_history, sizeof(tcp_connections[i].tx_history));
            if (next_connection_id == 0) {
                next_connection_id = 1;
            }
            copy_bytes(tcp_connections[i].remote_mac, remote_mac, 6);
            return &tcp_connections[i];
        }
    }

    if (tcp_reclaim_oldest_time_wait(now)) {
        goto retry;
    }

    tcp_table_full_drops++;
    return 0;
}

static uint32_t tcp_sequence_advance(uint8_t flags, uint16_t payload_length) {
    uint32_t advance = payload_length;
    if ((flags & TCP_SYN) != 0) {
        advance++;
    }
    if ((flags & TCP_FIN) != 0) {
        advance++;
    }
    return advance;
}

static bool tcp_sequence_acked(uint32_t end, uint32_t ack) {
    return (int32_t)(ack - end) >= 0;
}

static bool tcp_tx_free_slot(const struct tcp_connection *connection) {
    if (connection == 0) {
        return false;
    }
    for (uint8_t i = 0; i < TCP_TX_HISTORY; i++) {
        if (!connection->tx_history[i].used) {
            return true;
        }
    }
    return false;
}

static uint32_t tcp_tx_outstanding_bytes(const struct tcp_connection *connection) {
    if (connection == 0) {
        return 0;
    }

    uint32_t outstanding = 0;
    for (uint8_t i = 0; i < TCP_TX_HISTORY; i++) {
        const struct tcp_tx_record *record = &connection->tx_history[i];
        if (record->used) {
            outstanding += tcp_sequence_advance(record->flags, record->payload_length);
        }
    }
    return outstanding;
}

static void tcp_set_peer_window(struct tcp_connection *connection, uint16_t window) {
    if (connection == 0) {
        return;
    }

    uint16_t previous = connection->peer_window;
    connection->peer_window = window;
    if (window == 0) {
        if (connection->zero_window_deadline == 0) {
            connection->zero_window_deadline = timer_ticks() + TCP_ZERO_WINDOW_TICKS;
        }
    } else {
        connection->zero_window_deadline = 0;
        connection->zero_window_probes = 0;
    }
    if (window > previous) {
        scheduler_wake_all(&write_wait_queue);
        process_file_poll_wake();
    }
}

static void tcp_touch_activity(struct tcp_connection *connection, uint64_t now) {
    if (connection != 0) {
        connection->last_activity_tick = now;
    }
}

static uint16_t tcp_receive_window(const struct tcp_connection *connection) {
    if (connection == 0 || connection->local_read_closed || connection->reset) {
        return 0;
    }

    uint64_t unread = connection->rx_length >= connection->rx_offset ?
        connection->rx_length - connection->rx_offset :
        0;
    if (unread >= NET_CONNECTION_RX_MAX_SIZE) {
        return 0;
    }
    uint64_t available = NET_CONNECTION_RX_MAX_SIZE - unread;
    if (available > 65535) {
        available = 65535;
    }
    return (uint16_t)available;
}

static uint16_t tcp_tx_available_payload(const struct tcp_connection *connection) {
    if (connection == 0 || !tcp_tx_free_slot(connection)) {
        return 0;
    }

    uint32_t outstanding = tcp_tx_outstanding_bytes(connection);
    if (outstanding >= TCP_TX_WINDOW_BYTES) {
        return 0;
    }

    uint32_t available = TCP_TX_WINDOW_BYTES - outstanding;
    if (outstanding >= connection->peer_window) {
        return 0;
    }
    uint32_t peer_available = (uint32_t)connection->peer_window - outstanding;
    if (available > peer_available) {
        available = peer_available;
    }
    return available > TCP_SEGMENT_PAYLOAD_MAX ? TCP_SEGMENT_PAYLOAD_MAX : (uint16_t)available;
}

static bool tcp_tx_has_space(const struct tcp_connection *connection, uint8_t flags, uint16_t payload_length) {
    uint32_t advance = tcp_sequence_advance(flags, payload_length);
    if (advance == 0) {
        return true;
    }
    if (connection == 0 || !tcp_tx_free_slot(connection)) {
        return false;
    }
    uint32_t outstanding = tcp_tx_outstanding_bytes(connection);
    if (outstanding + advance > TCP_TX_WINDOW_BYTES) {
        return false;
    }
    if (payload_length != 0 && outstanding + payload_length > connection->peer_window) {
        return false;
    }
    return true;
}

static uint32_t tcp_receive_advance(uint8_t flags, uint16_t payload_length) {
    uint32_t advance = payload_length;
    if ((flags & TCP_FIN) != 0) {
        advance++;
    }
    return advance;
}

static bool send_tcp_segment_at(struct tcp_connection *connection,
    uint16_t local_port,
    uint32_t seq,
    uint8_t flags,
    const uint8_t *payload,
    uint16_t payload_length) {
    uint8_t segment[1500];
    uint16_t tcp_length = 20 + payload_length;

    if (tcp_length > sizeof(segment) || (payload_length != 0 && payload == 0)) {
        return false;
    }

    write_be16(segment + 0, local_port);
    write_be16(segment + 2, connection->remote_port);
    write_be32(segment + 4, seq);
    write_be32(segment + 8, connection->remote_next);
    segment[12] = 5 << 4;
    segment[13] = flags;
    write_be16(segment + 14, tcp_receive_window(connection));
    write_be16(segment + 16, 0);
    write_be16(segment + 18, 0);
    if (payload_length != 0) {
        copy_bytes(segment + 20, payload, payload_length);
    }
    write_be16(segment + 16, tcp_checksum(local_ip, connection->remote_ip, segment, tcp_length));

    if (!send_ipv4_packet(connection->remote_mac, connection->remote_ip, IP_PROTO_TCP, segment, tcp_length)) {
        return false;
    }

    tcp_touch_activity(connection, timer_ticks());
    return true;
}

static void tcp_record_tx(struct tcp_connection *connection,
    uint16_t local_port,
    uint32_t seq,
    uint8_t flags,
    const uint8_t *payload,
    uint16_t payload_length) {
    if (connection == 0 || tcp_sequence_advance(flags, payload_length) == 0) {
        return;
    }
    if (payload_length > TCP_SEGMENT_PAYLOAD_MAX || (payload_length != 0 && payload == 0)) {
        return;
    }

    uint8_t slot = connection->tx_next;
    for (uint8_t i = 0; i < TCP_TX_HISTORY; i++) {
        uint8_t candidate = (connection->tx_next + i) % TCP_TX_HISTORY;
        if (!connection->tx_history[candidate].used) {
            slot = candidate;
            break;
        }
    }

    struct tcp_tx_record *record = &connection->tx_history[slot];
    record->used = true;
    record->flags = flags;
    record->retries = 0;
    record->local_port = local_port;
    record->payload_length = payload_length;
    record->seq = seq;
    record->deadline = timer_ticks() + TCP_RETRANSMIT_TICKS;
    if (payload_length != 0) {
        copy_bytes(record->payload, payload, payload_length);
    }
    connection->tx_next = (slot + 1) % TCP_TX_HISTORY;
}

static bool send_tcp_segment(struct tcp_connection *connection,
    uint16_t local_port,
    uint8_t flags,
    const uint8_t *payload,
    uint16_t payload_length) {
    uint32_t seq = connection->local_seq;
    if (!tcp_tx_has_space(connection, flags, payload_length)) {
        return false;
    }
    if (!send_tcp_segment_at(connection, local_port, seq, flags, payload, payload_length)) {
        return false;
    }

    tcp_record_tx(connection, local_port, seq, flags, payload, payload_length);
    connection->local_seq += tcp_sequence_advance(flags, payload_length);
    return true;
}

static void send_tcp_reset(const uint8_t *dst_mac,
    uint32_t dst_ip,
    uint16_t local_port,
    uint16_t remote_port,
    uint32_t seq,
    uint32_t ack,
    uint8_t flags,
    uint16_t payload_length) {
    if (dst_mac == 0 || (flags & TCP_RST) != 0) {
        return;
    }

    uint8_t segment[20];
    uint8_t response_flags = TCP_RST;
    uint32_t response_seq = ack;
    uint32_t response_ack = 0;
    if ((flags & TCP_ACK) == 0) {
        response_flags = TCP_RST | TCP_ACK;
        response_seq = 0;
        response_ack = seq + tcp_sequence_advance(flags, payload_length);
    }

    write_be16(segment + 0, local_port);
    write_be16(segment + 2, remote_port);
    write_be32(segment + 4, response_seq);
    write_be32(segment + 8, response_ack);
    segment[12] = 5 << 4;
    segment[13] = response_flags;
    write_be16(segment + 14, 0);
    write_be16(segment + 16, 0);
    write_be16(segment + 18, 0);
    write_be16(segment + 16, tcp_checksum(local_ip, dst_ip, segment, sizeof(segment)));

    (void)send_ipv4_packet(dst_mac, dst_ip, IP_PROTO_TCP, segment, sizeof(segment));
}

static void close_connection(struct tcp_connection *connection) {
    if (connection == 0) {
        return;
    }

    for (uint64_t i = 0; i < NET_PENDING_REQUESTS; i++) {
        if (pending_requests[i].used && pending_requests[i].connection_id == connection->id) {
            pending_requests[i].used = false;
        }
    }

    connection->state = TCP_STATE_EMPTY;
    connection->pending = false;
    connection->accepted = false;
    connection->outbound = false;
    connection->peer_closed = false;
    connection->local_read_closed = false;
    connection->local_write_closed = false;
    connection->fin_pending = false;
    connection->reset = false;
    connection->request_counted = false;
    connection->error = SRV_NETERR_NONE;
    connection->peer_window = TCP_RX_WINDOW;
    connection->rx_length = 0;
    connection->rx_offset = 0;
    connection->rx_capacity = 0;
    connection->tx_next = 0;
    connection->close_deadline = 0;
    connection->last_activity_tick = 0;
    connection->zero_window_deadline = 0;
    connection->zero_window_probes = 0;
    clear_bytes((uint8_t *)connection->tx_history, sizeof(connection->tx_history));
    if (connection->rx_data != 0) {
        kfree(connection->rx_data);
        connection->rx_data = 0;
    }
    scheduler_wake_all(&accept_wait_queue);
    scheduler_wake_all(&read_wait_queue);
    scheduler_wake_all(&write_wait_queue);
    scheduler_wake_all(&connect_wait_queue);
    process_file_poll_wake();
}

static bool tcp_state_can_read(uint8_t state) {
    return state == TCP_STATE_ESTABLISHED ||
        state == TCP_STATE_FIN_WAIT_1 ||
        state == TCP_STATE_FIN_WAIT_2 ||
        state == TCP_STATE_CLOSE_WAIT;
}

static bool tcp_state_can_write(uint8_t state) {
    return state == TCP_STATE_ESTABLISHED || state == TCP_STATE_CLOSE_WAIT;
}

static bool tcp_state_closing(uint8_t state) {
    return state == TCP_STATE_FIN_WAIT_1 ||
        state == TCP_STATE_FIN_WAIT_2 ||
        state == TCP_STATE_LAST_ACK ||
        state == TCP_STATE_TIME_WAIT;
}

static void tcp_fail_connection(struct tcp_connection *connection, uint16_t error, uint64_t now) {
    if (connection == 0) {
        return;
    }

    connection->reset = true;
    connection->peer_closed = true;
    connection->local_write_closed = true;
    connection->fin_pending = false;
    connection->error = error == SRV_NETERR_NONE ? SRV_NETERR_RESET : error;
    connection->close_deadline = now + TCP_CLOSE_TIMEOUT_TICKS;
    scheduler_wake_all(&read_wait_queue);
    scheduler_wake_all(&write_wait_queue);
    scheduler_wake_all(&connect_wait_queue);
    process_file_poll_wake();
}

static void tcp_enter_time_wait(struct tcp_connection *connection) {
    if (connection == 0) {
        return;
    }
    connection->state = TCP_STATE_TIME_WAIT;
    connection->local_read_closed = true;
    connection->local_write_closed = true;
    connection->fin_pending = false;
    connection->peer_closed = true;
    connection->close_deadline = timer_ticks() + TCP_TIME_WAIT_TICKS;
    scheduler_wake_all(&read_wait_queue);
    scheduler_wake_all(&write_wait_queue);
    scheduler_wake_all(&connect_wait_queue);
    process_file_poll_wake();
}

static bool tcp_send_fin(struct tcp_connection *connection) {
    if (connection == 0 || connection->reset) {
        return true;
    }
    if (connection->local_write_closed && !connection->fin_pending) {
        return true;
    }
    if (!tcp_tx_has_space(connection, TCP_ACK | TCP_FIN, 0)) {
        connection->fin_pending = true;
        connection->local_write_closed = true;
        process_file_poll_wake();
        return true;
    }
    if (!send_tcp_segment(connection, connection->local_port, TCP_ACK | TCP_FIN, 0, 0)) {
        return false;
    }
    connection->fin_pending = false;
    connection->local_write_closed = true;
    connection->close_deadline = timer_ticks() + TCP_CLOSE_TIMEOUT_TICKS;
    if (connection->peer_closed || connection->state == TCP_STATE_CLOSE_WAIT) {
        connection->state = TCP_STATE_LAST_ACK;
    } else if (connection->state == TCP_STATE_ESTABLISHED || connection->state == TCP_STATE_SYN_RECEIVED) {
        connection->state = TCP_STATE_FIN_WAIT_1;
    }
    process_file_poll_wake();
    return true;
}

static void tcp_handle_fin(struct tcp_connection *connection, bool ack_flag) {
    if (connection == 0) {
        return;
    }
    connection->peer_closed = true;
    send_tcp_segment(connection, connection->local_port, TCP_ACK, 0, 0);
    if (connection->state == TCP_STATE_FIN_WAIT_1) {
        tcp_enter_time_wait(connection);
    } else if (connection->state == TCP_STATE_FIN_WAIT_2) {
        tcp_enter_time_wait(connection);
    } else if (connection->state == TCP_STATE_ESTABLISHED) {
        connection->state = connection->local_write_closed ? TCP_STATE_LAST_ACK : TCP_STATE_CLOSE_WAIT;
        connection->close_deadline = timer_ticks() + TCP_CLOSE_TIMEOUT_TICKS;
    } else if (connection->state == TCP_STATE_SYN_RECEIVED) {
        connection->state = TCP_STATE_CLOSE_WAIT;
        connection->close_deadline = timer_ticks() + TCP_CLOSE_TIMEOUT_TICKS;
    } else if (ack_flag && connection->state == TCP_STATE_LAST_ACK) {
        tcp_enter_time_wait(connection);
    }
    scheduler_wake_all(&read_wait_queue);
    scheduler_wake_all(&write_wait_queue);
    scheduler_wake_all(&connect_wait_queue);
    process_file_poll_wake();
}

static bool tcp_ack_tx_records(struct tcp_connection *connection, uint32_t ack) {
    if (connection == 0) {
        return false;
    }

    bool acked_fin = false;
    bool cleared = false;
    for (uint8_t i = 0; i < TCP_TX_HISTORY; i++) {
        struct tcp_tx_record *record = &connection->tx_history[i];
        if (!record->used) {
            continue;
        }
        uint32_t end = record->seq + tcp_sequence_advance(record->flags, record->payload_length);
        if (tcp_sequence_acked(end, ack)) {
            if ((record->flags & TCP_FIN) != 0) {
                acked_fin = true;
            }
            clear_bytes((uint8_t *)record, sizeof(*record));
            cleared = true;
        }
    }
    if (cleared) {
        if (connection->fin_pending) {
            (void)tcp_send_fin(connection);
        }
        scheduler_wake_all(&write_wait_queue);
        process_file_poll_wake();
    }
    return acked_fin;
}

static void tcp_handle_ack(struct tcp_connection *connection, uint32_t ack) {
    if (connection == 0) {
        return;
    }
    bool acked_fin = tcp_ack_tx_records(connection, ack);
    if (connection->state == TCP_STATE_FIN_WAIT_1 && acked_fin) {
        connection->state = connection->peer_closed ? TCP_STATE_TIME_WAIT : TCP_STATE_FIN_WAIT_2;
        connection->close_deadline = timer_ticks() +
            (connection->state == TCP_STATE_TIME_WAIT ? TCP_TIME_WAIT_TICKS : TCP_CLOSE_TIMEOUT_TICKS);
    } else if (connection->state == TCP_STATE_LAST_ACK && acked_fin) {
        tcp_enter_time_wait(connection);
    } else if (connection->state == TCP_STATE_TIME_WAIT) {
        connection->close_deadline = timer_ticks() + TCP_TIME_WAIT_TICKS;
    }
}

static void tcp_retransmit_due(uint64_t now) {
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        struct tcp_connection *connection = &tcp_connections[i];
        if (connection->state == TCP_STATE_EMPTY || connection->reset) {
            continue;
        }

        for (uint8_t j = 0; j < TCP_TX_HISTORY; j++) {
            struct tcp_tx_record *record = &connection->tx_history[j];
            if (!record->used || now < record->deadline) {
                continue;
            }

            if (record->retries >= TCP_RETRANSMIT_MAX) {
                tcp_fail_connection(connection,
                    connection->state == TCP_STATE_SYN_SENT ? SRV_NETERR_TIMEDOUT : SRV_NETERR_RESET,
                    now);
                break;
            }

            send_tcp_segment_at(connection,
                record->local_port,
                record->seq,
                record->flags,
                record->payload,
                record->payload_length);
            record->retries++;
            record->deadline = now + TCP_RETRANSMIT_TICKS * (uint64_t)(record->retries + 1);
        }
    }
}

static void tcp_zero_window_probe_due(uint64_t now) {
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        struct tcp_connection *connection = &tcp_connections[i];
        if (connection->state == TCP_STATE_EMPTY ||
            connection->reset ||
            !tcp_state_can_write(connection->state) ||
            connection->local_write_closed ||
            connection->peer_window != 0 ||
            connection->zero_window_deadline == 0 ||
            now < connection->zero_window_deadline) {
            continue;
        }

        if (connection->zero_window_probes >= TCP_ZERO_WINDOW_MAX) {
            tcp_fail_connection(connection, SRV_NETERR_TIMEDOUT, now);
            continue;
        }

        (void)send_tcp_segment_at(connection,
            connection->local_port,
            connection->local_seq,
            TCP_ACK,
            0,
            0);
        connection->zero_window_probes++;
        uint64_t delay = TCP_ZERO_WINDOW_TICKS * (uint64_t)(connection->zero_window_probes + 1);
        if (delay > TCP_CLOSE_TIMEOUT_TICKS) {
            delay = TCP_CLOSE_TIMEOUT_TICKS;
        }
        connection->zero_window_deadline = now + delay;
    }
}

static void tcp_idle_cleanup_due(uint64_t now) {
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        struct tcp_connection *connection = &tcp_connections[i];
        if (connection->state == TCP_STATE_EMPTY ||
            connection->reset ||
            tcp_state_closing(connection->state) ||
            connection->last_activity_tick == 0 ||
            now - connection->last_activity_tick < TCP_IDLE_TIMEOUT_TICKS) {
            continue;
        }

        tcp_fail_connection(connection, SRV_NETERR_TIMEDOUT, now);
    }
}

static uint64_t connection_index(struct tcp_connection *connection) {
    return (uint64_t)(connection - tcp_connections);
}

static bool append_connection_data(struct tcp_connection *connection,
    const uint8_t *payload,
    uint16_t payload_length) {
    if (connection == 0 || payload == 0 || payload_length == 0) {
        return false;
    }

    if (connection->rx_offset != 0) {
        uint64_t remaining = connection->rx_length - connection->rx_offset;
        for (uint64_t i = 0; i < remaining; i++) {
            connection->rx_data[i] = connection->rx_data[connection->rx_offset + i];
        }
        connection->rx_length = remaining;
        connection->rx_offset = 0;
    }

    uint64_t needed = connection->rx_length + payload_length;
    if (needed > NET_CONNECTION_RX_MAX_SIZE) {
        return false;
    }
    if (needed > connection->rx_capacity) {
        uint64_t next_capacity = connection->rx_capacity == 0 ?
            NET_CONNECTION_RX_INITIAL_SIZE :
            connection->rx_capacity * 2;
        while (next_capacity < needed) {
            next_capacity *= 2;
        }
        if (next_capacity > NET_CONNECTION_RX_MAX_SIZE) {
            next_capacity = NET_CONNECTION_RX_MAX_SIZE;
        }

        char *next_buffer = kmalloc(next_capacity);
        if (next_buffer == 0) {
            return false;
        }
        for (uint64_t i = 0; i < connection->rx_length; i++) {
            next_buffer[i] = connection->rx_data[i];
        }
        if (connection->rx_data != 0) {
            kfree(connection->rx_data);
        }
        connection->rx_data = next_buffer;
        connection->rx_capacity = next_capacity;
    }

    for (uint64_t i = 0; i < payload_length; i++) {
        connection->rx_data[connection->rx_length + i] = (char)payload[i];
    }
    connection->rx_length += payload_length;
    scheduler_wake_all(&read_wait_queue);
    process_file_poll_wake();
    return true;
}

static bool ensure_pending_connection(struct tcp_connection *connection, uint16_t port) {
    if (connection == 0) {
        return false;
    }
    if (connection->accepted || connection->pending) {
        return true;
    }

    for (uint64_t i = 0; i < NET_PENDING_REQUESTS; i++) {
        if (!pending_requests[i].used) {
            struct pending_request *request = &pending_requests[i];
            request->used = true;
            request->owner = connection->owner;
            request->connection_index = connection_index(connection);
            request->connection_id = connection->id;
            request->port = port;
            request->length = connection->rx_length;
            connection->pending = true;
            scheduler_wake_all(&accept_wait_queue);
            process_file_poll_wake();
            return true;
        }
    }

    return false;
}

static void send_unavailable_response(struct tcp_connection *connection) {
    static const char response[] =
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "Content-Length: 34\r\n"
        "\r\n"
        "srvros web request queue is full\n";

    send_tcp_segment(connection, connection->local_port, TCP_ACK | TCP_PSH, (const uint8_t *)response, sizeof(response) - 1);
    http_requests++;
    send_tcp_segment(connection, connection->local_port, TCP_ACK | TCP_FIN, 0, 0);
    close_connection(connection);
}

static void handle_tcp(const uint8_t *frame,
    uint16_t length,
    const uint8_t *ip,
    uint16_t ip_header_length,
    uint16_t ip_payload_length,
    uint32_t src_ip) {
    if (ip_payload_length < 20 || length < ETH_HEADER_SIZE + ip_header_length + ip_payload_length) {
        return;
    }

    const uint8_t *tcp = ip + ip_header_length;
    uint16_t src_port = read_be16(tcp + 0);
    uint16_t dst_port = read_be16(tcp + 2);
    uint32_t seq = read_be32(tcp + 4);
    uint32_t ack = read_be32(tcp + 8);
    uint8_t data_offset = (tcp[12] >> 4) * 4;
    uint8_t flags = tcp[13];
    uint16_t advertised_window = read_be16(tcp + 14);

    if (data_offset < 20 || data_offset > ip_payload_length) {
        return;
    }
    if (tcp_checksum(src_ip, read_be32(ip + 16), tcp, ip_payload_length) != 0) {
        return;
    }

    tcp_packets++;
    uint16_t data_length = ip_payload_length - data_offset;

    if ((flags & TCP_RST) != 0) {
        struct tcp_connection *old = find_connection(dst_port, src_ip, src_port);
        if (old != 0) {
            tcp_fail_connection(old,
                old->state == TCP_STATE_SYN_SENT ? SRV_NETERR_REFUSED : SRV_NETERR_RESET,
                timer_ticks());
        }
        return;
    }

    if ((flags & TCP_SYN) != 0) {
        if (!port_is_listening(dst_port)) {
            struct tcp_connection *client = find_connection(dst_port, src_ip, src_port);
            if (client != 0 && client->state == TCP_STATE_SYN_SENT && (flags & TCP_ACK) != 0) {
                copy_bytes(client->remote_mac, frame + 6, 6);
                tcp_set_peer_window(client, advertised_window);
                client->remote_next = seq + 1;
                tcp_ack_tx_records(client, ack);
                client->state = TCP_STATE_ESTABLISHED;
                send_tcp_segment(client, client->local_port, TCP_ACK, 0, 0);
                scheduler_wake_all(&connect_wait_queue);
                process_file_poll_wake();
            } else {
                send_tcp_reset(frame + 6, src_ip, dst_port, src_port, seq, ack, flags, data_length);
            }
            return;
        }
        struct tcp_connection *connection = alloc_connection(dst_port, src_ip, src_port, frame + 6);
        if (connection == 0) {
            send_tcp_reset(frame + 6, src_ip, dst_port, src_port, seq, ack, flags, data_length);
            return;
        }
        tcp_set_peer_window(connection, advertised_window);
        connection->remote_next = seq + 1;
        send_tcp_segment(connection, connection->local_port, TCP_SYN | TCP_ACK, 0, 0);
        return;
    }

    struct tcp_connection *connection = find_connection(dst_port, src_ip, src_port);
    if (connection == 0) {
        send_tcp_reset(frame + 6, src_ip, dst_port, src_port, seq, ack, flags, data_length);
        return;
    }

    copy_bytes(connection->remote_mac, frame + 6, 6);
    tcp_touch_activity(connection, timer_ticks());
    tcp_set_peer_window(connection, advertised_window);
    uint32_t receive_advance = tcp_receive_advance(flags, data_length);
    if (receive_advance != 0 && seq != connection->remote_next) {
        send_tcp_segment(connection, connection->local_port, TCP_ACK, 0, 0);
        return;
    }

    if (connection->state == TCP_STATE_SYN_RECEIVED && (flags & TCP_ACK) != 0) {
        connection->state = TCP_STATE_ESTABLISHED;
    }

    if ((flags & TCP_ACK) != 0) {
        tcp_handle_ack(connection, ack);
    }

    if (tcp_state_can_read(connection->state) && data_length != 0) {
        if (connection->local_read_closed) {
            connection->remote_next = seq + receive_advance;
            if ((flags & TCP_FIN) != 0) {
                tcp_handle_fin(connection, (flags & TCP_ACK) != 0);
            } else {
                send_tcp_segment(connection, connection->local_port, TCP_ACK, 0, 0);
            }
            return;
        }
        if (connection->outbound) {
            if (!append_connection_data(connection, tcp + data_offset, data_length)) {
                close_connection(connection);
                return;
            }
            connection->remote_next = seq + receive_advance;
            if ((flags & TCP_FIN) != 0) {
                tcp_handle_fin(connection, (flags & TCP_ACK) != 0);
            } else {
                send_tcp_segment(connection, connection->local_port, TCP_ACK, 0, 0);
            }
        } else {
            bool appended = append_connection_data(connection, tcp + data_offset, data_length);
            if (appended) {
                connection->remote_next = seq + receive_advance;
            }
            if (appended && ensure_pending_connection(connection, dst_port)) {
                if (!connection->request_counted) {
                    http_requests++;
                    connection->request_counted = true;
                }
                if ((flags & TCP_FIN) != 0) {
                    tcp_handle_fin(connection, (flags & TCP_ACK) != 0);
                } else {
                    send_tcp_segment(connection, connection->local_port, TCP_ACK, 0, 0);
                }
            } else {
                send_unavailable_response(connection);
            }
        }
        if ((flags & TCP_FIN) != 0) {
            scheduler_wake_all(&read_wait_queue);
            process_file_poll_wake();
        }
        return;
    }

    if ((flags & TCP_FIN) != 0) {
        connection->remote_next = seq + receive_advance;
        tcp_handle_fin(connection, (flags & TCP_ACK) != 0);
    }
}

static void handle_ipv4(const uint8_t *frame, uint16_t length) {
    if (length < ETH_HEADER_SIZE + 20) {
        return;
    }

    const uint8_t *ip = frame + ETH_HEADER_SIZE;
    uint8_t version = ip[0] >> 4;
    uint16_t header_length = (ip[0] & 0x0f) * 4;
    uint16_t total_length = read_be16(ip + 2);

    uint32_t dst_ip = read_be32(ip + 16);
    if (version != 4 ||
        header_length < 20 ||
        total_length < header_length ||
        length < ETH_HEADER_SIZE + total_length ||
        (dst_ip != local_ip && dst_ip != 0xffffffffu)) {
        return;
    }
    if (checksum_bytes(ip, header_length, 0) != 0) {
        return;
    }

    ipv4_packets++;
    uint16_t payload_length = total_length - header_length;
    uint32_t src_ip = read_be32(ip + 12);

    if (ip[9] == IP_PROTO_ICMP) {
        handle_icmp(frame, length, ip, header_length, payload_length, src_ip);
    } else if (ip[9] == IP_PROTO_TCP) {
        handle_tcp(frame, length, ip, header_length, payload_length, src_ip);
    } else if (ip[9] == IP_PROTO_UDP) {
        handle_udp(ip, header_length, payload_length);
    }
}

void net_handle_ethernet_frame(const uint8_t *frame, uint16_t length) {
    if (!initialized || frame == 0 || length < ETH_HEADER_SIZE || !is_for_local_mac(frame)) {
        return;
    }

    rx_frames++;
    uint16_t ethertype = read_be16(frame + 12);
    if (ethertype == ETH_TYPE_ARP) {
        handle_arp(frame, length);
    } else if (ethertype == ETH_TYPE_IPV4) {
        handle_ipv4(frame, length);
    }
}

void net_notify_rx(void) {
    rx_irq_wakeups++;
    rx_signal_generation++;
    scheduler_wake_all(&rx_wait_queue);
}

void net_notify_rx_deferred(void *arg) {
    (void)arg;
    net_notify_rx();
}

void net_timer_tick(void) {
    if (!initialized || !worker_started) {
        return;
    }

    uint64_t now = timer_ticks();
    tcp_retransmit_due(now);
    tcp_zero_window_probe_due(now);
    tcp_idle_cleanup_due(now);
    (void)tcp_reap_expired_time_wait(now);
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        struct tcp_connection *connection = &tcp_connections[i];
        if (connection->state != TCP_STATE_EMPTY &&
            (connection->reset || tcp_state_closing(connection->state)) &&
            connection->close_deadline != 0 &&
            now >= connection->close_deadline) {
            close_connection(connection);
        }
    }

    if (now - rx_last_recovery_tick < NET_RX_RECOVERY_TICKS) {
        return;
    }

    rx_last_recovery_tick = now;
    rx_recovery_wakeups++;
    rx_signal_generation++;
    scheduler_wake_all(&rx_wait_queue);
}

static void net_worker(void *arg) {
    (void)arg;

    for (;;) {
        uint64_t received = e1000_poll(32, false);
        rx_worker_polls++;
        if (received != 0) {
            rx_worker_packets += received;
            continue;
        }
        rx_worker_empty_polls++;
        scheduler_yield();
    }
}

void net_init(void) {
    set_local_ip_bytes(local_ip_bytes);
    local_mac = e1000_mac();

    if (local_mac == 0) {
        console_write("net: no e1000 link, stack idle\n");
        initialized = false;
        return;
    }

    initialized = true;
    console_printf("net: static ipv4=%u.%u.%u.%u tcp=:80 icmp=on dhcp=available\n",
        local_ip_bytes[0],
        local_ip_bytes[1],
        local_ip_bytes[2],
        local_ip_bytes[3]);
}

bool net_start_worker(void) {
    if (!initialized) {
        return false;
    }
    if (worker_started) {
        return true;
    }

    worker_started = scheduler_spawn("net-poll", net_worker, 0);
    if (worker_started) {
        rx_last_recovery_tick = timer_ticks();
    }
    return worker_started;
}

int64_t net_dhcp_request(void) {
    if (!initialized || local_mac == 0) {
        return -1;
    }

    dhcp_xid += 0x1021u;
    if (dhcp_xid == 0) {
        dhcp_xid = 0x73727672u;
    }
    dhcp_configured = false;
    dhcp_in_progress = true;
    dhcp_have_offer = false;
    dhcp_request_sent = false;
    clear_bytes(dhcp_offered_ip_bytes, sizeof(dhcp_offered_ip_bytes));
    clear_bytes(dhcp_server_ip_bytes, sizeof(dhcp_server_ip_bytes));
    clear_bytes(dhcp_router_ip_bytes, sizeof(dhcp_router_ip_bytes));
    clear_bytes(dhcp_dns_ip_bytes, sizeof(dhcp_dns_ip_bytes));

    if (!send_dhcp_message(DHCP_DISCOVER, 0, 0)) {
        dhcp_in_progress = false;
        return -1;
    }

    uint64_t start = timer_ticks();
    while (timer_ticks() - start < DHCP_TIMEOUT_TICKS) {
        if (dhcp_configured) {
            return (int64_t)local_ip;
        }
        (void)e1000_poll(32, false);
        if (dhcp_have_offer && !dhcp_request_sent) {
            (void)send_dhcp_message(DHCP_REQUEST, dhcp_offered_ip_bytes, dhcp_server_ip_bytes);
        }
        scheduler_yield();
    }

    dhcp_in_progress = false;
    return -1;
}

static uint32_t configured_dns_server(void) {
    uint32_t dns = read_be32(dhcp_dns_ip_bytes);
    if (dns != 0) {
        return dns;
    }
    return ((uint32_t)10 << 24) | ((uint32_t)0 << 16) | ((uint32_t)2 << 8) | 3;
}

int64_t net_dns_resolve(const char *name, uint32_t *ip_out) {
    if (!initialized || name == 0 || name[0] == '\0' || ip_out == 0) {
        return -1;
    }

    uint32_t server_ip = configured_dns_server();
    uint8_t server_mac[6];
    if (!resolve_ipv4_mac(server_ip, server_mac)) {
        return -1;
    }

    for (uint64_t attempt = 0; attempt < DNS_RETRIES; attempt++) {
        dns_query_id++;
        if (dns_query_id == 0) {
            dns_query_id = 0x7372;
        }
        dns_answer_ip = 0;
        dns_answer_ready = false;
        dns_response_done = false;
        dns_waiting = true;
        if (!send_dns_query(server_ip, server_mac, name, dns_query_id)) {
            dns_waiting = false;
            return -1;
        }

        uint64_t start = timer_ticks();
        while (timer_ticks() - start < DNS_TIMEOUT_TICKS) {
            if (dns_answer_ready) {
                *ip_out = dns_answer_ip;
                dns_waiting = false;
                return 0;
            }
            if (dns_response_done) {
                break;
            }
            (void)e1000_poll(32, false);
            scheduler_yield();
        }
        dns_waiting = false;
    }
    return -1;
}

void net_print_status(void) {
    uint64_t pending = 0;
    uint64_t tcp_count = 0;
    uint64_t tcp_time_wait_count = 0;
    for (uint64_t i = 0; i < NET_PENDING_REQUESTS; i++) {
        if (pending_requests[i].used) {
            pending++;
        }
    }
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (tcp_connections[i].state != TCP_STATE_EMPTY) {
            tcp_count++;
            if (tcp_connections[i].state == TCP_STATE_TIME_WAIT) {
                tcp_time_wait_count++;
            }
        }
    }

    uint64_t listener_count = 0;
    for (uint64_t i = 0; i < NET_LISTENERS; i++) {
        if (listeners[i].used) {
            listener_count++;
        }
    }

    console_printf("net: stack=%s worker=%s ip=%u.%u.%u.%u dhcp=%s listeners=%u rx=%u tx=%u arp=%u ipv4=%u icmp=%u tcp=%u udp=%u queued=%u accepted=%u done=%u\n",
        initialized ? "ready" : "down",
        worker_started ? "on" : "off",
        local_ip_bytes[0],
        local_ip_bytes[1],
        local_ip_bytes[2],
        local_ip_bytes[3],
        dhcp_configured ? "yes" : "no",
        listener_count,
        rx_frames,
        tx_frames,
        arp_packets,
        ipv4_packets,
        icmp_packets,
        tcp_packets,
        udp_packets,
        pending,
        accepted_requests,
        completed_requests);
    console_printf("net: worker polls=%u empty=%u packets=%u irq_wakeups=%u recovery_wakeups=%u generation=%u\n",
        rx_worker_polls,
        rx_worker_empty_polls,
        rx_worker_packets,
        rx_irq_wakeups,
        rx_recovery_wakeups,
        rx_signal_generation);
    console_printf("net: dns=%u.%u.%u.%u router=%u.%u.%u.%u\n",
        dhcp_dns_ip_bytes[0],
        dhcp_dns_ip_bytes[1],
        dhcp_dns_ip_bytes[2],
        dhcp_dns_ip_bytes[3],
        dhcp_router_ip_bytes[0],
        dhcp_router_ip_bytes[1],
        dhcp_router_ip_bytes[2],
        dhcp_router_ip_bytes[3]);
    console_printf("net: tcp slots=%u/%u time_wait=%u full=%u time_wait_reaped=%u close_timeout=%ut time_wait_timeout=%ut\n",
        tcp_count,
        TCP_MAX_CONNECTIONS,
        tcp_time_wait_count,
        tcp_table_full_drops,
        tcp_time_wait_reaped,
        TCP_CLOSE_TIMEOUT_TICKS,
        TCP_TIME_WAIT_TICKS);
}

static const char *tcp_state_name(uint8_t state) {
    switch (state) {
    case TCP_STATE_SYN_RECEIVED:
        return "SYN-RECV";
    case TCP_STATE_ESTABLISHED:
        return "ESTABLISHED";
    case TCP_STATE_SYN_SENT:
        return "SYN-SENT";
    case TCP_STATE_FIN_WAIT_1:
        return "FIN-WAIT-1";
    case TCP_STATE_FIN_WAIT_2:
        return "FIN-WAIT-2";
    case TCP_STATE_CLOSE_WAIT:
        return "CLOSE-WAIT";
    case TCP_STATE_LAST_ACK:
        return "LAST-ACK";
    case TCP_STATE_TIME_WAIT:
        return "TIME-WAIT";
    default:
        return "UNKNOWN";
    }
}

static void copy_state_name(char *destination, uint64_t capacity, const char *source) {
    uint64_t i = 0;
    if (capacity == 0) {
        return;
    }
    while (source != 0 && source[i] != '\0' && i + 1 < capacity) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

static uint64_t listener_pending_count(uint16_t port) {
    uint64_t count = 0;
    for (uint64_t i = 0; i < NET_PENDING_REQUESTS; i++) {
        if (pending_requests[i].used && pending_requests[i].port == port) {
            count++;
        }
    }
    return count;
}

static uint8_t tcp_net_flags(const struct tcp_connection *connection) {
    uint8_t flags = 0;
    if (connection->pending) {
        flags |= SRV_NET_FLAG_PENDING;
    }
    if (connection->accepted) {
        flags |= SRV_NET_FLAG_ACCEPTED;
    }
    if (connection->outbound) {
        flags |= SRV_NET_FLAG_OUTBOUND;
    }
    if (connection->peer_closed) {
        flags |= SRV_NET_FLAG_PEER_CLOSED;
    }
    if (connection->local_read_closed) {
        flags |= SRV_NET_FLAG_READ_CLOSED;
    }
    if (connection->local_write_closed) {
        flags |= SRV_NET_FLAG_WRITE_CLOSED;
    }
    if (connection->fin_pending) {
        flags |= SRV_NET_FLAG_FIN_PENDING;
    }
    if (connection->reset) {
        flags |= SRV_NET_FLAG_RESET;
    }
    return flags;
}

int64_t net_list(uint64_t index, struct srv_net_info *info) {
    if (info == 0) {
        return -1;
    }

    uint64_t total = NET_LISTENERS + TCP_MAX_CONNECTIONS + UDP_MAX_SOCKETS;
    for (uint64_t slot = index; slot < total; slot++) {
        clear_bytes((uint8_t *)info, sizeof(*info));
        info->abi_version = SRV_NET_ABI_VERSION;
        info->struct_size = sizeof(*info);
        if (slot < NET_LISTENERS) {
            const struct net_listener *listener = &listeners[slot];
            if (!listener->used) {
                continue;
            }
            info->kind = SRV_NET_KIND_TCP_LISTENER;
            info->id = listener->id;
            info->pid = listener->owner != 0 ? process_pid(listener->owner) : 0;
            info->rx_queued = listener_pending_count(listener->port);
            info->rx_capacity = NET_PENDING_REQUESTS;
            info->local_ip = local_ip;
            info->local_port = listener->port;
            copy_state_name(info->state_name, sizeof(info->state_name), "LISTEN");
            return (int64_t)(slot + 1);
        }

        uint64_t tcp_slot = slot - NET_LISTENERS;
        if (tcp_slot < TCP_MAX_CONNECTIONS) {
            const struct tcp_connection *connection = &tcp_connections[tcp_slot];
            if (connection->state == TCP_STATE_EMPTY) {
                continue;
            }
            info->kind = SRV_NET_KIND_TCP_CONNECTION;
            info->id = connection->id;
            info->pid = connection->owner != 0 ? process_pid(connection->owner) : 0;
            info->state = connection->state;
            info->rx_queued = connection->rx_length >= connection->rx_offset ?
                connection->rx_length - connection->rx_offset :
                0;
            info->rx_capacity = connection->rx_capacity;
            info->tx_outstanding = tcp_tx_outstanding_bytes(connection);
            info->tx_available = tcp_tx_available_payload(connection);
            info->close_deadline = connection->close_deadline;
            info->last_activity_tick = connection->last_activity_tick;
            info->local_ip = local_ip;
            info->remote_ip = connection->remote_ip;
            info->local_port = connection->local_port;
            info->remote_port = connection->remote_port;
            info->peer_window = connection->peer_window;
            info->error = connection->error;
            info->flags = tcp_net_flags(connection);
            copy_state_name(info->state_name, sizeof(info->state_name), tcp_state_name(connection->state));
            return (int64_t)(slot + 1);
        }

        uint64_t udp_slot = tcp_slot - TCP_MAX_CONNECTIONS;
        const struct udp_socket *socket = &udp_sockets[udp_slot];
        if (!socket->used) {
            continue;
        }
        info->kind = SRV_NET_KIND_UDP_SOCKET;
        info->id = socket->id;
        info->pid = socket->owner != 0 ? process_pid(socket->owner) : 0;
        info->rx_queued = socket->count;
        info->rx_capacity = UDP_RX_QUEUE;
        info->local_ip = local_ip;
        info->local_port = socket->local_port;
        copy_state_name(info->state_name, sizeof(info->state_name), "OPEN");
        return (int64_t)(slot + 1);
    }

    return 0;
}

int64_t net_status_info(struct srv_net_status_info *info) {
    if (info == 0) {
        return -1;
    }

    clear_bytes((uint8_t *)info, sizeof(*info));
    info->abi_version = SRV_NET_ABI_VERSION;
    info->struct_size = sizeof(*info);
    info->initialized = initialized ? 1 : 0;
    info->worker_started = worker_started ? 1 : 0;
    info->dhcp_configured = dhcp_configured ? 1 : 0;
    info->tcp_connection_capacity = TCP_MAX_CONNECTIONS;
    info->tcp_table_full_drops = tcp_table_full_drops;
    info->tcp_time_wait_reaped = tcp_time_wait_reaped;
    info->tcp_close_timeout_ticks = TCP_CLOSE_TIMEOUT_TICKS;
    info->tcp_time_wait_ticks = TCP_TIME_WAIT_TICKS;
    info->local_ip = local_ip;
    info->router_ip = read_be32(dhcp_router_ip_bytes);
    info->dns_ip = read_be32(dhcp_dns_ip_bytes);
    info->rx_frames = rx_frames;
    info->tx_frames = tx_frames;
    info->arp_packets = arp_packets;
    info->ipv4_packets = ipv4_packets;
    info->icmp_packets = icmp_packets;
    info->tcp_packets = tcp_packets;
    info->udp_packets = udp_packets;
    info->http_requests = http_requests;
    info->accepted_requests = accepted_requests;
    info->completed_requests = completed_requests;
    info->rx_worker_polls = rx_worker_polls;
    info->rx_worker_empty_polls = rx_worker_empty_polls;
    info->rx_worker_packets = rx_worker_packets;
    info->rx_irq_wakeups = rx_irq_wakeups;
    info->rx_recovery_wakeups = rx_recovery_wakeups;
    info->rx_signal_generation = rx_signal_generation;
    if (local_mac != 0) {
        copy_bytes(info->local_mac, local_mac, sizeof(info->local_mac));
    }
    if (arp_resolved) {
        info->arp_valid = 1;
        info->arp_ip = arp_resolved_ip;
        copy_bytes(info->arp_mac, arp_resolved_mac, sizeof(info->arp_mac));
    } else {
        for (uint64_t i = 0; i < ARP_CACHE_ENTRIES; i++) {
            if (arp_cache[i].used) {
                info->arp_valid = 1;
                info->arp_ip = arp_cache[i].ip;
                copy_bytes(info->arp_mac, arp_cache[i].mac, sizeof(info->arp_mac));
                break;
            }
        }
    }

    for (uint64_t i = 0; i < NET_LISTENERS; i++) {
        if (listeners[i].used) {
            info->listener_count++;
        }
    }
    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (tcp_connections[i].state != TCP_STATE_EMPTY) {
            info->tcp_connection_count++;
            if (tcp_connections[i].state == TCP_STATE_TIME_WAIT) {
                info->tcp_time_wait_count++;
            }
        }
    }
    for (uint64_t i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_sockets[i].used) {
            info->udp_socket_count++;
        }
    }
    for (uint64_t i = 0; i < NET_PENDING_REQUESTS; i++) {
        if (pending_requests[i].used) {
            info->pending_count++;
        }
    }
    return 0;
}

int64_t net_arp_list(uint64_t index, struct srv_arp_info *info) {
    if (info == 0) {
        return -1;
    }

    for (uint64_t slot = index; slot < ARP_CACHE_ENTRIES; slot++) {
        if (!arp_cache[slot].used) {
            continue;
        }
        clear_bytes((uint8_t *)info, sizeof(*info));
        info->abi_version = SRV_NET_ABI_VERSION;
        info->struct_size = sizeof(*info);
        info->ip = arp_cache[slot].ip;
        copy_bytes(info->mac, arp_cache[slot].mac, sizeof(info->mac));
        info->updated_tick = arp_cache[slot].updated_tick;
        return (int64_t)(slot + 1);
    }
    return 0;
}

int64_t net_ping(uint32_t remote_ip, uint16_t sequence, uint64_t timeout_ticks, uint64_t *elapsed_ticks_out) {
    if (!initialized || remote_ip == 0) {
        return -1;
    }
    if (timeout_ticks == 0) {
        timeout_ticks = 100;
    }

    icmp_ping_ip = remote_ip;
    icmp_ping_sequence = sequence;
    icmp_ping_start_tick = timer_ticks();
    icmp_ping_reply = false;
    icmp_ping_waiting = true;
    if (!send_icmp_echo_request(remote_ip, sequence)) {
        icmp_ping_waiting = false;
        return -1;
    }

    while (timer_ticks() - icmp_ping_start_tick < timeout_ticks) {
        if (icmp_ping_reply) {
            if (elapsed_ticks_out != 0) {
                *elapsed_ticks_out = timer_ticks() - icmp_ping_start_tick;
            }
            return 0;
        }
        (void)e1000_poll(32, false);
        scheduler_yield();
    }

    icmp_ping_waiting = false;
    return -1;
}

int64_t net_listen(uint16_t port) {
    if (!initialized || port == 0) {
        return -1;
    }

    struct process *owner = process_current();
    if (owner == 0) {
        return -1;
    }

    for (uint64_t i = 0; i < NET_LISTENERS; i++) {
        if (listeners[i].used && listeners[i].port == port) {
            if (listeners[i].owner != owner) {
                return -1;
            }
            return (int64_t)make_handle(NET_HANDLE_LISTENER, listeners[i].id);
        }
    }

    for (uint64_t i = 0; i < NET_LISTENERS; i++) {
        if (!listeners[i].used) {
            listeners[i].used = true;
            listeners[i].id = next_listener_id++;
            listeners[i].owner = owner;
            listeners[i].port = port;
            if (next_listener_id == 0) {
                next_listener_id = 1;
            }
            return (int64_t)make_handle(NET_HANDLE_LISTENER, listeners[i].id);
        }
    }

    return -1;
}

int64_t net_udp_open(void) {
    if (!initialized) {
        return -1;
    }

    struct process *owner = process_current();
    if (owner == 0) {
        return -1;
    }

    for (uint64_t i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (!udp_sockets[i].used) {
            clear_bytes((uint8_t *)&udp_sockets[i], sizeof(udp_sockets[i]));
            udp_sockets[i].used = true;
            udp_sockets[i].id = next_udp_socket_id++;
            udp_sockets[i].owner = owner;
            if (next_udp_socket_id == 0) {
                next_udp_socket_id = 1;
            }
            return (int64_t)make_handle(NET_HANDLE_UDP_SOCKET, udp_sockets[i].id);
        }
    }

    return -1;
}

int64_t net_udp_bind(uint64_t socket_id, uint16_t port) {
    struct udp_socket *socket = find_udp_socket(socket_id);
    if (!initialized || socket == 0 || socket->owner != process_current()) {
        return -1;
    }

    uint16_t selected = port == 0 ? allocate_udp_port() : port;
    if (selected == 0 || udp_port_reserved(selected)) {
        return -1;
    }

    struct udp_socket *existing = find_udp_socket_for_port(selected);
    if (existing != 0 && existing != socket) {
        return -1;
    }

    socket->local_port = selected;
    return 0;
}

int64_t net_udp_sendto(uint64_t socket_id,
    uint32_t remote_ip,
    uint16_t remote_port,
    const uint8_t *buffer,
    uint64_t length) {
    if (!initialized || remote_ip == 0 || remote_port == 0 || length > UDP_PAYLOAD_MAX ||
        (length != 0 && buffer == 0)) {
        return -1;
    }

    struct udp_socket *socket = find_udp_socket(socket_id);
    if (socket == 0 || socket->owner != process_current()) {
        return -1;
    }

    if (socket->local_port == 0 && net_udp_bind(socket_id, 0) < 0) {
        return -1;
    }

    if (remote_ip == local_ip) {
        udp_deliver(remote_port, local_ip, socket->local_port, buffer, (uint16_t)length);
        return (int64_t)length;
    }

    uint8_t remote_mac[6];
    if (!resolve_route_mac(remote_ip, remote_mac)) {
        return -1;
    }

    return send_udp_packet(remote_mac,
        local_ip_bytes,
        remote_ip,
        socket->local_port,
        remote_port,
        buffer,
        (uint16_t)length) ? (int64_t)length : -1;
}

struct udp_wait_arg {
    struct process *owner;
    uint64_t socket_id;
};

static bool udp_recv_ready(void *arg) {
    struct udp_wait_arg *wait = arg;
    if (wait == 0 || wait->owner == 0 || process_should_exit_current()) {
        return true;
    }

    for (uint64_t i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_sockets[i].used &&
            udp_sockets[i].id == wait->socket_id &&
            udp_sockets[i].owner == wait->owner) {
            return udp_sockets[i].count != 0;
        }
    }
    return true;
}

int64_t net_udp_recvfrom(uint64_t socket_id,
    uint8_t *buffer,
    uint64_t capacity,
    uint32_t *remote_ip_out,
    uint16_t *remote_port_out,
    bool nonblock) {
    if (!initialized || (capacity != 0 && buffer == 0)) {
        return -1;
    }

    struct udp_socket *socket = find_udp_socket(socket_id);
    struct process *owner = process_current();
    if (socket == 0 || owner == 0 || socket->owner != owner) {
        return -1;
    }
    if (socket->local_port == 0 && net_udp_bind(socket_id, 0) < 0) {
        return -1;
    }

    struct udp_wait_arg wait = {
        .owner = owner,
        .socket_id = handle_value(socket_id),
    };
    for (;;) {
        if (process_should_exit_current()) {
            return -1;
        }

        socket = find_udp_socket(socket_id);
        if (socket == 0 || socket->owner != owner) {
            return -1;
        }
        if (socket->count != 0) {
            struct udp_datagram *datagram = &socket->queue[socket->head];
            uint64_t copied = capacity < datagram->length ? capacity : datagram->length;
            copy_bytes(buffer, datagram->data, copied);
            if (remote_ip_out != 0) {
                *remote_ip_out = datagram->remote_ip;
            }
            if (remote_port_out != 0) {
                *remote_port_out = datagram->remote_port;
            }
            socket->head = (uint8_t)((socket->head + 1) % UDP_RX_QUEUE);
            socket->count--;
            return (int64_t)copied;
        }

        e1000_poll(32, false);
        if (nonblock) {
            return SRV_ERR_AGAIN;
        }
        if (!scheduler_wait(&udp_wait_queue, udp_recv_ready, &wait)) {
            scheduler_yield();
        }
    }
}

int64_t net_sockname(uint64_t handle, uint32_t *ip_out, uint16_t *port_out) {
    if (!initialized || ip_out == 0 || port_out == 0) {
        return -1;
    }

    struct process *owner = process_current();
    if (owner == 0) {
        return -1;
    }

    if (handle_type(handle) == NET_HANDLE_LISTENER) {
        struct net_listener *listener = find_listener(handle);
        if (listener == 0 || listener->owner != owner) {
            return -1;
        }
        *ip_out = local_ip;
        *port_out = listener->port;
        return 0;
    }

    if (handle_type(handle) == NET_HANDLE_CONNECTION) {
        struct tcp_connection *connection = find_connection_by_id_owner(handle_value(handle), owner);
        if (connection == 0) {
            return -1;
        }
        *ip_out = local_ip;
        *port_out = connection->local_port;
        return 0;
    }

    if (handle_type(handle) == NET_HANDLE_UDP_SOCKET) {
        struct udp_socket *socket = find_udp_socket(handle);
        if (socket == 0 || socket->owner != owner) {
            return -1;
        }
        *ip_out = local_ip;
        *port_out = socket->local_port;
        return 0;
    }

    return -1;
}

int64_t net_peername(uint64_t handle, uint32_t *ip_out, uint16_t *port_out) {
    if (!initialized || ip_out == 0 || port_out == 0) {
        return -1;
    }

    struct process *owner = process_current();
    if (owner == 0 || handle_type(handle) != NET_HANDLE_CONNECTION) {
        return -1;
    }

    struct tcp_connection *connection = find_connection_by_id_owner(handle_value(handle), owner);
    if (connection == 0) {
        return -1;
    }
    *ip_out = connection->remote_ip;
    *port_out = connection->remote_port;
    return 0;
}

int64_t net_shutdown(uint64_t handle, int how) {
    if (!initialized || handle_type(handle) != NET_HANDLE_CONNECTION || how < 0 || how > 2) {
        return -1;
    }

    struct process *owner = process_current();
    if (owner == 0) {
        return -1;
    }

    struct tcp_connection *connection = find_connection_by_id_owner(handle_value(handle), owner);
    if (connection == 0 || !tcp_state_can_read(connection->state)) {
        return -1;
    }

    if (how == 0 || how == 2) {
        connection->local_read_closed = true;
        connection->rx_offset = connection->rx_length;
        scheduler_wake_all(&read_wait_queue);
    }
    if ((how == 1 || how == 2) && !connection->local_write_closed) {
        if (!tcp_send_fin(connection)) {
            return -1;
        }
    }

    process_file_poll_wake();
    return 0;
}

int64_t net_error(uint64_t handle) {
    if (!initialized || handle_type(handle) != NET_HANDLE_CONNECTION) {
        return -1;
    }

    struct process *owner = process_current();
    if (owner == 0) {
        return -1;
    }

    struct tcp_connection *connection = find_connection_by_id_owner(handle_value(handle), owner);
    if (connection == 0) {
        return -1;
    }
    if (connection->state == TCP_STATE_SYN_SENT) {
        return SRV_NETERR_INPROGRESS;
    }
    if (connection->reset) {
        return connection->error == SRV_NETERR_NONE ? SRV_NETERR_RESET : connection->error;
    }
    return SRV_NETERR_NONE;
}

struct connect_wait_arg {
    struct process *owner;
    uint64_t connection_id;
};

static bool connect_ready(void *arg) {
    struct connect_wait_arg *wait = arg;
    if (wait == 0 || wait->owner == 0 || process_should_exit_current()) {
        return true;
    }

    struct tcp_connection *connection = find_connection_by_id_owner(wait->connection_id, wait->owner);
    return connection == 0 ||
        connection->state == TCP_STATE_ESTABLISHED ||
        connection->reset ||
        connection->peer_closed;
}

int64_t net_connect(uint32_t remote_ip, uint16_t remote_port, bool nonblock) {
    if (!initialized || remote_ip == 0 || remote_port == 0) {
        return -1;
    }

    uint8_t remote_mac[6];
    if (!resolve_route_mac(remote_ip, remote_mac)) {
        return -1;
    }

    struct tcp_connection *connection = alloc_client_connection(remote_ip, remote_port, remote_mac);
    if (connection == 0) {
        return -1;
    }
    uint64_t connection_id = connection->id;
    struct process *owner = connection->owner;
    if (!send_tcp_segment(connection, connection->local_port, TCP_SYN, 0, 0)) {
        close_connection(connection);
        return -1;
    }

    if (nonblock) {
        return (int64_t)make_handle(NET_HANDLE_CONNECTION, connection_id);
    }

    struct connect_wait_arg wait = {
        .owner = owner,
        .connection_id = connection_id,
    };
    uint64_t deadline = timer_ticks() + TCP_CONNECT_TIMEOUT_TICKS;
    for (;;) {
        if (process_should_exit_current()) {
            close_connection(find_connection_by_id_owner(connection_id, owner));
            return -1;
        }

        connection = find_connection_by_id_owner(connection_id, owner);
        if (connection == 0) {
            return -1;
        }
        if (connection->state == TCP_STATE_ESTABLISHED) {
            return (int64_t)make_handle(NET_HANDLE_CONNECTION, connection_id);
        }
        if (connection->reset || connection->peer_closed) {
            int64_t result = connection->error == SRV_NETERR_NONE ?
                -SRV_NETERR_RESET :
                -(int64_t)connection->error;
            close_connection(connection);
            return result;
        }
        if (timer_ticks() >= deadline) {
            close_connection(connection);
            return -SRV_NETERR_TIMEDOUT;
        }

        e1000_poll(32, false);
        if (!scheduler_wait_timeout(&connect_wait_queue, connect_ready, &wait, deadline)) {
            scheduler_yield();
        }
    }
}

static bool accept_ready(void *arg) {
    struct net_listener *listener = arg;
    if (listener == 0 || !listener->used || process_should_exit_current()) {
        return true;
    }

    for (uint64_t i = 0; i < NET_PENDING_REQUESTS; i++) {
        struct pending_request *request = &pending_requests[i];
        if (request->used && request->port == listener->port && request->owner == listener->owner) {
            return true;
        }
    }
    return false;
}

int64_t net_accept(uint64_t listener_id,
    char *buffer,
    uint64_t capacity,
    uint64_t *length_out,
    bool nonblock) {
    if (!initialized) {
        return -1;
    }

    struct net_listener *listener = find_listener(listener_id);
    if (listener == 0 || listener->owner != process_current()) {
        return -1;
    }

    for (;;) {
        if (process_should_exit_current()) {
            return -1;
        }

        for (uint64_t i = 0; i < NET_PENDING_REQUESTS; i++) {
            struct pending_request *request = &pending_requests[i];
            if (request->used && request->port == listener->port && request->owner == listener->owner) {
                struct tcp_connection *connection = &tcp_connections[request->connection_index];
                uint64_t copied = 0;
                if (buffer != 0 && capacity != 0) {
                    uint64_t remaining = connection->rx_length - connection->rx_offset;
                    copied = remaining < capacity ? remaining : capacity;
                    for (uint64_t j = 0; j < copied; j++) {
                        buffer[j] = connection->rx_data[connection->rx_offset + j];
                    }
                    connection->rx_offset = copied;
                    if (copied != 0) {
                        (void)send_tcp_segment(connection, connection->local_port, TCP_ACK, 0, 0);
                    }
                }
                if (length_out != 0) {
                    *length_out = connection->rx_length - connection->rx_offset;
                }
                request->used = false;
                connection->pending = false;
                connection->accepted = true;
                accepted_requests++;
                return (int64_t)make_handle(NET_HANDLE_CONNECTION, request->connection_id);
            }
        }

        e1000_poll(32, false);
        if (nonblock) {
            return SRV_ERR_AGAIN;
        }
        if (!scheduler_wait(&accept_wait_queue, accept_ready, listener)) {
            scheduler_yield();
        }
    }
}

struct read_wait_arg {
    struct process *owner;
    uint64_t connection_id;
};

static bool read_ready(void *arg) {
    struct read_wait_arg *wait = arg;
    if (wait == 0 || wait->owner == 0 || process_should_exit_current()) {
        return true;
    }

    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        struct tcp_connection *connection = &tcp_connections[i];
        if (connection->state != TCP_STATE_EMPTY &&
            connection->id == wait->connection_id &&
            connection->owner == wait->owner) {
            return connection->rx_length != connection->rx_offset || connection->peer_closed || connection->reset;
        }
    }
    return true;
}

int64_t net_read(uint64_t connection_id, char *buffer, uint64_t length, bool nonblock) {
    if (!initialized || (buffer == 0 && length != 0)) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }
    if (handle_type(connection_id) != NET_HANDLE_CONNECTION) {
        return -1;
    }

    struct process *owner = process_current();
    if (owner == 0) {
        return -1;
    }

    uint64_t value = handle_value(connection_id);
    struct read_wait_arg wait = {
        .owner = owner,
        .connection_id = value,
    };
    for (;;) {
        if (process_should_exit_current()) {
            return -1;
        }

        bool found = false;
        for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
            struct tcp_connection *connection = &tcp_connections[i];
            if (connection->state != TCP_STATE_EMPTY && connection->id == value && connection->owner == owner) {
                found = true;
                if (connection->reset) {
                    return -1;
                }
                if (connection->local_read_closed) {
                    return 0;
                }
                if (!tcp_state_can_read(connection->state)) {
                    break;
                }
                uint64_t remaining = connection->rx_length - connection->rx_offset;
                if (remaining != 0) {
                    uint64_t copied = length < remaining ? length : remaining;
                    for (uint64_t j = 0; j < copied; j++) {
                        buffer[j] = connection->rx_data[connection->rx_offset + j];
                    }
                    connection->rx_offset += copied;
                    (void)send_tcp_segment(connection, connection->local_port, TCP_ACK, 0, 0);
                    return (int64_t)copied;
                }

                if (connection->peer_closed) {
                    return 0;
                }
                break;
            }
        }
        if (!found) {
            return -1;
        }

        e1000_poll(32, false);
        if (nonblock) {
            return SRV_ERR_AGAIN;
        }
        if (!scheduler_wait(&read_wait_queue, read_ready, &wait)) {
            scheduler_yield();
        }
    }
}

struct write_wait_arg {
    struct process *owner;
    uint64_t connection_id;
};

static bool write_ready(void *arg) {
    struct write_wait_arg *wait = arg;
    if (wait == 0 || wait->owner == 0 || process_should_exit_current()) {
        return true;
    }

    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        struct tcp_connection *connection = &tcp_connections[i];
        if (connection->state != TCP_STATE_EMPTY &&
            connection->id == wait->connection_id &&
            connection->owner == wait->owner) {
            return connection->reset ||
                connection->local_write_closed ||
                !tcp_state_can_write(connection->state) ||
                tcp_tx_available_payload(connection) != 0;
        }
    }
    return true;
}

int64_t net_respond(uint64_t connection_id, const char *buffer, uint64_t length, bool nonblock) {
    if (!initialized || (buffer == 0 && length != 0)) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }
    if (handle_type(connection_id) != NET_HANDLE_CONNECTION) {
        return -1;
    }

    struct process *owner = process_current();
    if (owner == 0) {
        return -1;
    }

    uint64_t value = handle_value(connection_id);
    uint64_t sent = 0;
    struct write_wait_arg wait = {
        .owner = owner,
        .connection_id = value,
    };
    for (;;) {
        if (process_should_exit_current()) {
            return sent == 0 ? -1 : (int64_t)sent;
        }

        bool found = false;
        for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
            struct tcp_connection *connection = &tcp_connections[i];
            if (connection->state != TCP_STATE_EMPTY && connection->id == value && connection->owner == owner) {
                found = true;
                if (!tcp_state_can_write(connection->state) ||
                    connection->local_write_closed ||
                    connection->reset) {
                    return sent == 0 ? -1 : (int64_t)sent;
                }

                uint16_t available = tcp_tx_available_payload(connection);
                if (available == 0) {
                    break;
                }

                uint64_t remaining = length - sent;
                uint16_t chunk = remaining > available ? available : (uint16_t)remaining;
                if (!send_tcp_segment(connection,
                    connection->local_port,
                    TCP_ACK | TCP_PSH,
                    (const uint8_t *)buffer + sent,
                    chunk)) {
                    return sent == 0 ? -1 : (int64_t)sent;
                }
                sent += chunk;
                e1000_poll(16, false);
                scheduler_yield();
                if (sent == length) {
                    return (int64_t)sent;
                }
                break;
            }
        }
        if (!found) {
            return sent == 0 ? -1 : (int64_t)sent;
        }
        if (sent != 0) {
            return (int64_t)sent;
        }

        e1000_poll(32, false);
        if (nonblock) {
            return SRV_ERR_AGAIN;
        }
        if (!scheduler_wait(&write_wait_queue, write_ready, &wait)) {
            scheduler_yield();
        }
    }
}

uint16_t net_poll_events(uint64_t handle, uint16_t events) {
    if (!initialized) {
        return SRV_POLLNVAL;
    }

    struct process *owner = process_current();
    if (owner == 0) {
        return SRV_POLLNVAL;
    }

    e1000_poll(32, false);

    if (handle_type(handle) == NET_HANDLE_LISTENER) {
        struct net_listener *listener = find_listener(handle);
        if (listener == 0 || listener->owner != owner) {
            return SRV_POLLNVAL;
        }

        uint16_t revents = 0;
        if ((events & SRV_POLLIN) != 0) {
            for (uint64_t i = 0; i < NET_PENDING_REQUESTS; i++) {
                struct pending_request *request = &pending_requests[i];
                if (request->used && request->port == listener->port && request->owner == owner) {
                    revents |= SRV_POLLIN;
                    break;
                }
            }
        }
        return revents;
    }

    if (handle_type(handle) == NET_HANDLE_CONNECTION) {
        uint64_t value = handle_value(handle);
        for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
            struct tcp_connection *connection = &tcp_connections[i];
            if (connection->state == TCP_STATE_EMPTY ||
                connection->id != value ||
                connection->owner != owner) {
                continue;
            }

            uint16_t revents = 0;
            if ((events & SRV_POLLIN) != 0 &&
                (connection->rx_length != connection->rx_offset ||
                    connection->peer_closed ||
                    connection->local_read_closed)) {
                revents |= SRV_POLLIN;
            }
            if ((events & SRV_POLLOUT) != 0 &&
                tcp_state_can_write(connection->state) &&
                !connection->local_write_closed &&
                !connection->reset &&
                tcp_tx_available_payload(connection) != 0) {
                revents |= SRV_POLLOUT;
            }
            if (connection->reset) {
                revents |= SRV_POLLERR;
            }
            if (connection->peer_closed) {
                revents |= SRV_POLLHUP;
            }
            return revents;
        }
        return SRV_POLLNVAL;
    }

    if (handle_type(handle) == NET_HANDLE_UDP_SOCKET) {
        struct udp_socket *socket = find_udp_socket(handle);
        if (socket == 0 || socket->owner != owner) {
            return SRV_POLLNVAL;
        }

        uint16_t revents = 0;
        if ((events & SRV_POLLIN) != 0 && socket->count != 0) {
            revents |= SRV_POLLIN;
        }
        if ((events & SRV_POLLOUT) != 0) {
            revents |= SRV_POLLOUT;
        }
        return revents;
    }

    return SRV_POLLNVAL;
}

int64_t net_close(uint64_t handle) {
    if (!initialized) {
        return -1;
    }

    struct process *owner = process_current();
    if (owner == 0) {
        return -1;
    }

    if (handle_type(handle) == NET_HANDLE_CONNECTION) {
        uint64_t value = handle_value(handle);
        for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
            struct tcp_connection *connection = &tcp_connections[i];
            if (connection->state != TCP_STATE_EMPTY && connection->id == value && connection->owner == owner) {
                if (connection->reset || connection->state == TCP_STATE_SYN_SENT) {
                    close_connection(connection);
                } else if (tcp_state_can_write(connection->state) && !connection->local_write_closed) {
                    if (!tcp_send_fin(connection)) {
                        close_connection(connection);
                    }
                } else if (tcp_state_closing(connection->state) && connection->close_deadline == 0) {
                    connection->close_deadline = timer_ticks() + TCP_CLOSE_TIMEOUT_TICKS;
                }
                completed_requests++;
                return 0;
            }
        }
        return -1;
    }

    if (handle_type(handle) == NET_HANDLE_LISTENER) {
        struct net_listener *listener = find_listener(handle);
        if (listener == 0 || listener->owner != owner) {
            return -1;
        }

        uint16_t port = listener->port;
        listener->used = false;
        for (uint64_t i = 0; i < NET_PENDING_REQUESTS; i++) {
            if (pending_requests[i].used && pending_requests[i].port == port) {
                pending_requests[i].used = false;
            }
        }
        for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
            if (tcp_connections[i].state != TCP_STATE_EMPTY &&
                tcp_connections[i].local_port == port &&
                tcp_connections[i].owner == owner &&
                !tcp_connections[i].accepted) {
                close_connection(&tcp_connections[i]);
            }
        }
        return 0;
    }

    if (handle_type(handle) == NET_HANDLE_UDP_SOCKET) {
        struct udp_socket *socket = find_udp_socket(handle);
        if (socket == 0 || socket->owner != owner) {
            return -1;
        }
        clear_bytes((uint8_t *)socket, sizeof(*socket));
        scheduler_wake_all(&udp_wait_queue);
        process_file_poll_wake();
        return 0;
    }

    return -1;
}

void net_process_cleanup(struct process *process) {
    if (process == 0) {
        return;
    }

    for (uint64_t i = 0; i < NET_PENDING_REQUESTS; i++) {
        if (pending_requests[i].used && pending_requests[i].owner == process) {
            pending_requests[i].used = false;
        }
    }

    for (uint64_t i = 0; i < NET_LISTENERS; i++) {
        if (listeners[i].used && listeners[i].owner == process) {
            listeners[i].used = false;
        }
    }

    for (uint64_t i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (tcp_connections[i].state != TCP_STATE_EMPTY &&
            tcp_connections[i].owner == process) {
            close_connection(&tcp_connections[i]);
        }
    }

    for (uint64_t i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_sockets[i].used && udp_sockets[i].owner == process) {
            clear_bytes((uint8_t *)&udp_sockets[i], sizeof(udp_sockets[i]));
        }
    }
}

void net_process_wake(struct process *process) {
    (void)process;
    scheduler_wake_all(&accept_wait_queue);
    scheduler_wake_all(&read_wait_queue);
    scheduler_wake_all(&connect_wait_queue);
    scheduler_wake_all(&udp_wait_queue);
    process_file_poll_wake();
}
