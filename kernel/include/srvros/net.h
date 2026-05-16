#ifndef SRVROS_NET_H
#define SRVROS_NET_H

#include <stdbool.h>
#include <stdint.h>

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
int64_t net_listen(uint16_t port);
int64_t net_connect(uint32_t remote_ip, uint16_t remote_port, bool nonblock);
int64_t net_accept(uint64_t listener_id, char *buffer, uint64_t capacity, uint64_t *length_out, bool nonblock);
int64_t net_read(uint64_t connection_id, char *buffer, uint64_t length, bool nonblock);
int64_t net_respond(uint64_t connection_id, const char *buffer, uint64_t length);
uint16_t net_poll_events(uint64_t handle, uint16_t events);
int64_t net_close(uint64_t handle);
void net_process_cleanup(struct process *process);
void net_process_wake(struct process *process);

#endif
