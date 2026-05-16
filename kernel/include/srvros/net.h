#ifndef SRVROS_NET_H
#define SRVROS_NET_H

#include <stdbool.h>
#include <stdint.h>

#include <srvros/syscall_numbers.h>

struct process;

void net_init(void);
bool net_start_worker(void);
void net_handle_ethernet_frame(const uint8_t *frame, uint16_t length);
void net_notify_rx(void);
void net_notify_rx_deferred(void *arg);
void net_timer_tick(void);
void net_print_status(void);
int64_t net_dhcp_request(void);
int64_t net_dns_resolve(const char *name, uint32_t *ip_out);
int64_t net_list(uint64_t index, struct srv_net_info *info);
int64_t net_status_info(struct srv_net_status_info *info);
int64_t net_arp_list(uint64_t index, struct srv_arp_info *info);
int64_t net_ping(uint32_t remote_ip, uint16_t sequence, uint64_t timeout_ticks, uint64_t *elapsed_ticks_out);
int64_t net_listen(uint16_t port);
int64_t net_connect(uint32_t remote_ip, uint16_t remote_port, bool nonblock);
int64_t net_udp_open(void);
int64_t net_udp_bind(uint64_t socket_id, uint16_t port);
int64_t net_udp_sendto(uint64_t socket_id,
    uint32_t remote_ip,
    uint16_t remote_port,
    const uint8_t *buffer,
    uint64_t length);
int64_t net_udp_recvfrom(uint64_t socket_id,
    uint8_t *buffer,
    uint64_t capacity,
    uint32_t *remote_ip_out,
    uint16_t *remote_port_out,
    bool nonblock);
int64_t net_sockname(uint64_t handle, uint32_t *ip_out, uint16_t *port_out);
int64_t net_peername(uint64_t handle, uint32_t *ip_out, uint16_t *port_out);
int64_t net_shutdown(uint64_t handle, int how);
int64_t net_error(uint64_t handle);
int64_t net_accept(uint64_t listener_id, char *buffer, uint64_t capacity, uint64_t *length_out, bool nonblock);
int64_t net_read(uint64_t connection_id, char *buffer, uint64_t length, bool nonblock);
int64_t net_respond(uint64_t connection_id, const char *buffer, uint64_t length, bool nonblock);
uint16_t net_poll_events(uint64_t handle, uint16_t events);
int64_t net_close(uint64_t handle);
void net_process_cleanup(struct process *process);
void net_process_wake(struct process *process);

#endif
