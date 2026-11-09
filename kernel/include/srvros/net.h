#ifndef SRVROS_NET_H
#define SRVROS_NET_H

#include <stdbool.h>
#include <stdint.h>

#include <srvros/syscall_numbers.h>

struct process;

/* Checked transfers run callbacks before consuming queued bytes or accepting a
 * connection. Callers serialize callbacks and commit on CPU0 and supply owned
 * storage for transmit payloads. The identity callback also ends blocked waits
 * after descriptor replacement, even while another alias keeps the handle. */
struct net_transfer {
    void *context;
    bool (*current)(void *context);
    int64_t (*copyout)(void *context, const uint8_t *data, uint64_t length,
        uint64_t remaining, uint32_t ip, uint16_t port);
};
int64_t net_udp_recv_checked(uint64_t handle, uint64_t capacity, bool nonblock,
    const struct net_transfer *transfer);
int64_t net_accept_checked(uint64_t handle, uint64_t capacity, bool nonblock,
    const struct net_transfer *transfer);
int64_t net_read_checked(uint64_t handle, uint64_t length, bool nonblock, bool peek,
    const struct net_transfer *transfer);
int64_t net_respond_checked(uint64_t handle, const char *data, uint64_t length,
    bool nonblock, const struct net_transfer *transfer);
/* Dormant finite alternatives: no polling, waits, yields, delays or wire retries.
 * connect_try requires a real current process and an initialized local address.
 * A cache-only route miss returns AGAIN without ARP, slot/ID/port allocation or
 * SYN publication. A hit attempts one SYN and returns its SYN_SENT handle;
 * completion/timers and process lifetime remain the caller's responsibility.
 * respond_try_checked attempts at most one MSS/window/history-bounded segment,
 * returning its positive admitted length, AGAIN only before output when no
 * credit is available, zero for a valid zero-length request, or an error.
 * The checked FD callback runs before publication; the handle is refound after
 * it. Admission stays positive if descriptor identity later changes: an error
 * after admission would falsely authorize replay. Output refusal is an error,
 * not retry authority; uncertain provider ownership belongs to the outer owner.
 * Like ordinary sends, CPU0 callers serialize TCP state and the output provider
 * must not reenter/mutate TCP tables. These APIs create no process/timer scope. */
int64_t net_connect_try(uint32_t remote_ip, uint16_t remote_port);
int64_t net_respond_try_checked(uint64_t handle, const char *data, uint64_t length,
    const struct net_transfer *transfer);
int64_t net_udp_send_checked(uint64_t handle, uint32_t ip, uint16_t port,
    const uint8_t *data, uint64_t length, bool nonblock, const struct net_transfer *transfer);

void net_init(void);
/* Explicit one-shot native probe:register the secured provider first. Refuses
 * an initialized stack,worker or DHCP owner. Suppresses startup and DHCP ACK
 * console output for this probe;normal net_init retains ordinary logging.
 * No packet/key/address logging or deferred flush occurs here. Caller records
 * bounded result metadata only after hardware has closed. Other status print
 * APIs must not be invoked in the live MAC window. No worker is started. */
bool net_init_bounded_probe(void);
bool net_start_worker(void);
void net_handle_ethernet_frame(const uint8_t *frame, uint16_t length);
void net_notify_rx(void);
void net_notify_rx_deferred(void *arg);
void net_timer_tick(void);
void net_print_status(void);
int64_t net_dhcp_request(void);
/* Explicit bounded probe variant;the ordinary API keeps its single Discover.
 * CPU0 callbacks must be nonblocking,side-effect-free and remain unchanged for
 * the call. current binds the original selected provider and secured epoch.
 * retry_ready additionally proves the first Discover's exact TX completion and
 * no pending/uncertain transmission;queued send success alone is insufficient.
 * One same-XID retry requires both100ticks and1second,inside the original
 * 200tick budget plus a2second monotonic bound. No ACK/Request retry is added.
 * False guards,down/unknown link,send refusal,clock rollback or20000polls fail.
 * Probe replies require exact BOOTP/XID/client,complete unique critical options
 * and server identity. ACK matches the accepted Offer/Request,address,server,
 * observed source/relay and usable mask/router/DNS/lease. Historical ordinary
 * packet admission is unchanged;this does not authenticate a DHCP server.
 * Failure does not cancel/retire provider or DMA ownership;caller must close
 * publication and retain uncertain transport resources under its own proof. */
struct net_dhcp_retry_guard {
    void *context;
    bool (*current)(void *context);
    bool (*retry_ready)(void *context);
};
int64_t net_dhcp_request_discover_retry(const struct net_dhcp_retry_guard *guard);
/* Explicit finite native-probe APIs,not an ordinary network policy switch.
 * Zero object initialized once;never copy,reinitialize or modify its fields.
 * Keep the borrowed IO table/context alive and unchanged until finish. Both
 * callbacks are side-effect-free/nonblocking. current binds the original
 * selected provider and secured epoch;retry_ready independently proves exact
 * first-Discover TX completion with no pending/uncertain transmission.
 * Caller supplies the ORIGINAL absolute window deadline,at most15seconds away.
 * Last clocks and150000 aggregate polls persist across all calls and nested
 * ARP. Each transaction also has finite local time/poll caps. No guarded path
 * waits,yields or delays:only bounded provider polling and CPU relax.
 * Failure is terminal for this object and never grants DMA retirement. */
struct net_bounded_probe_io {
    void *context;
    bool (*current)(void *context,uint64_t epoch);
    bool (*retry_ready)(void *context,uint64_t epoch);
};
struct net_bounded_probe {
    uintptr_t identity;
    const struct net_bounded_probe_io *source;
    struct net_bounded_probe_io io,io_guard;
    const uint8_t *station;
    uint8_t mac[6];
    uint64_t epoch,epoch_guard,deadline_us,deadline_guard;
    uint64_t start_us,start_tick,last_us,last_tick,polls;
    /* Request counters describe queued admission;actual TX completion remains
     * the native owner's proof. Matched pings require exact40-byte ICMP message
     * length and all32 payload bytes,not merely ID/sequence/checksum. */
    uint64_t dhcp_retries,arp_resolutions,dns_queries,pings_sent,pings_matched;
    bool busy,checking,failed,closed;
};
bool net_bounded_probe_init(struct net_bounded_probe *probe,
    const struct net_bounded_probe_io *io,uint64_t epoch,uint64_t deadline_us);
bool net_bounded_probe_current(struct net_bounded_probe *probe);
bool net_bounded_probe_poll(struct net_bounded_probe *probe);
int64_t net_dhcp_request_bounded(struct net_bounded_probe *probe);
int64_t net_dns_resolve_bounded(struct net_bounded_probe *probe,const char *name,uint32_t *ip_out);
int64_t net_ping_bounded(struct net_bounded_probe *probe,uint32_t remote_ip,
    uint16_t sequence,uint64_t *elapsed_ticks_out);
/* Final owner/clock check,then clear callback/context/MAC references and close
 * even on failure. Retains its identity tombstone/counters;no reinitialization
 * or hardware retirement authority. No transaction-global probe pointer survives
 * an API return;the object retains its borrowed IO until finish. */
bool net_bounded_probe_finish(struct net_bounded_probe *probe);
int64_t net_dns_resolve(const char *name, uint32_t *ip_out);
int64_t net_list(uint64_t index, struct srv_net_info *info);
int64_t net_status_info(struct srv_net_status_info *info);
int64_t net_arp_list(uint64_t index, struct srv_arp_info *info);
int64_t net_ping(uint32_t remote_ip, uint16_t sequence, uint64_t timeout_ticks, uint64_t *elapsed_ticks_out);
int64_t net_listen(uint16_t port);
int64_t net_connect(uint32_t remote_ip, uint16_t remote_port, bool nonblock);
int64_t net_udp_open(void);
int64_t net_udp_bind(uint64_t socket_id, uint16_t port);
int64_t net_udp_sendto_mode(uint64_t socket_id, uint32_t remote_ip, uint16_t remote_port,
    const uint8_t *buffer, uint64_t length, bool dontwait);
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
int64_t net_peek(uint64_t connection_id, char *buffer, uint64_t length, bool nonblock);
int64_t net_respond(uint64_t connection_id, const char *buffer, uint64_t length, bool nonblock);
uint16_t net_poll_events(uint64_t handle, uint16_t events);
int64_t net_hold(uint64_t handle);
int64_t net_close(uint64_t handle);
void net_process_cleanup(struct process *process);
void net_process_wake(struct process *process);

#endif
