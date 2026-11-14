#ifndef SRVROS_NET_H
#define SRVROS_NET_H

#include <stdbool.h>
#include <stdint.h>

#include <srvros/syscall_numbers.h>

struct process;
struct net_bounded_probe;
struct net_bounded_probe_io;
struct net_device_ops;

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
/* Explicit one-shot CPU0 probe only. Prepare allocates the scheduler stack
 * BEFORE any live provider/key ownership, then yield until prepared() confirms
 * the exact private thread is parked. Existing ordinary startup is unchanged.
 * handoff consumes/closes the original open bounded bootstrap before activating
 * this prepared worker, retaining its immutable provider/epoch, ORIGINAL15s
 * deadline, monotonic/tick limits and aggregate150000poll budget. Source IO and
 * its context must remain stable until finish; callbacks are nonblocking and
 * side-effect-free, provider poll/send must not yield or reenter TCP tables.
 * In this scope IRQ ticks only wake; worker turns perform TCP maintenance.
 * Caller must supervise real progress/clocks/child lifetime, revoke child GO,
 * reap child and keep key/provider live through actual FIN/TIME_WAIT retirement.
 * empty() requires every TCP slot EMPTY (not app-close or TIME_WAIT), plus no
 * listeners,UDP,pending/bootstrap owner. stop() terminally cancels a prepared unused worker or parks a transferred worker without freeing resources;
 * finish requires exact parked ack,empty tables and no callback/turn in flight,
 * then scrubs borrowed references. An unused worker finish does not close any external bootstrap; caller must separately finish/scrub that object. Parked thread/stack/tombstone remain until
 * reboot. Failure never grants transport,provider,key or process retirement. */
struct net_probe_worker_info {
    uint64_t prepared,parked,active,handoff_started,ever_active,busy,poll_busy,checking,stop_requested,finished,failed;
    uint64_t parked_waiters,worker_identity,supervisor_identity,start_us,last_us,deadline_us,start_tick,last_tick,polls,turns;
};
/* Snapshot is kernel-only observation into caller-owned nonaliasing storage;
 * no callbacks,clock reads or ownership/state changes,including after failure. */
bool net_probe_worker_snapshot(struct net_probe_worker_info *out);
bool net_prepare_probe_worker(void);
/* Explicit two-lifetime HTTP experiment. Each call consumes a fresh retained
 * worker object/thread. Second preparation requires the first successfully
 * finished and deconfigured, exact adapter identity and a DOWN link. Never
 * reinitializes a worker, restarts a deadline or admits a third lifetime. */
bool net_prepare_connection_probe_worker(void);
/* Consume the exact closed connection bootstrap only after the transferred
 * worker finished successfully with empty tables. Caller separately proves
 * child retirement and original live provider guard. Not hardware retirement. */
bool net_probe_worker_disconnect(struct net_bounded_probe *probe);
/* Terminal pre-handoff protocol failure only: unused prepared worker must be
 * stopped/finished and all protocol tables empty. Bind original IO/epoch/window
 * independently, preserve failed history, deconfigure and fuse later admission.
 * Caller separately proves child retirement and transport/RX ownership. */
bool net_probe_worker_abort_connection(struct net_bounded_probe *probe,
    const struct net_bounded_probe_io *original,uint64_t epoch,uint64_t deadline_us);
bool net_probe_worker_prepared(void);
bool net_probe_worker_handoff(struct net_bounded_probe *probe);
bool net_probe_worker_current(void);
bool net_probe_worker_empty(void);
bool net_probe_worker_stop(void);
bool net_probe_worker_finish(void);
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
/* Observation only, scoped by the existing CPU0 bounded poll owner. Codes are
 * stable log metadata, never packet admission or retirement authority. No
 * address, XID, payload or key is retained. Non-DHCP traffic may contribute
 * early Ethernet/IP/UDP refusals; message counts require a parsed DHCP type. */
enum net_dhcp_reject {
    NET_DHCP_RX_OK, NET_DHCP_RX_OWNER, NET_DHCP_RX_ETHERNET,
    NET_DHCP_RX_IP_HEADER, NET_DHCP_RX_IP_DESTINATION, NET_DHCP_RX_IP_CHECKSUM,
    NET_DHCP_RX_IP_FRAGMENT, NET_DHCP_RX_UDP_LENGTH, NET_DHCP_RX_UDP_CHECKSUM,
    NET_DHCP_RX_UDP_TTL, NET_DHCP_RX_BOOTP, NET_DHCP_RX_XID,
    NET_DHCP_RX_CLIENT, NET_DHCP_RX_SOURCE, NET_DHCP_RX_OPTION_BOUNDS,
    NET_DHCP_RX_OPTION_END, NET_DHCP_RX_OPTION_DUPLICATE, NET_DHCP_RX_OPTION_SHAPE,
    NET_DHCP_RX_ADDRESS, NET_DHCP_RX_OFFER_STATE, NET_DHCP_RX_ACK_STATE,
    NET_DHCP_RX_ACK_BINDING, NET_DHCP_RX_ACK_CONFIG, NET_DHCP_RX_MESSAGE
};
enum net_dhcp_end {
    NET_DHCP_NOT_RUN, NET_DHCP_SUCCESS, NET_DHCP_GUARD_FAILED,
    NET_DHCP_TIME_LIMIT, NET_DHCP_POLL_LIMIT, NET_DHCP_SEND_FAILED,
    NET_DHCP_REQUEST_FAILED, NET_DHCP_BUSY, NET_DHCP_RETRY_FAILED
};
struct net_dhcp_observation {
    uint64_t version,started,finished,success,end_reason;
    uint64_t discovers,discover_queued,requests,request_queued;
    uint64_t frames,ipv4,udp,dhcp,offers,acks,naks,accepted_offer,accepted_ack;
    uint64_t rejected,first_reject,last_reject,reject_mask,last_message,last_options;
    uint64_t have_offer,request_sent,configured,polls,elapsed_us,elapsed_ticks;
};
/* Categorical diagnostic only; no packet/identity values or admission grant. */
struct net_dhcp_identity5 {
    uint64_t version,tx_discover,tx_request,tx_request_mask,tx_invalid;
    uint64_t rx_client,rx_self_discover,rx_self_request,rx_client_mismatch,rx_client_mask;
    uint64_t rx_offer,rx_ack,rx_server_other,rx_server_mask,rx_malformed;
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
    struct net_dhcp_observation dhcp_observation;
    struct net_dhcp_identity5 dhcp_identity;
    /* Set only by an ordinary negative protocol result while all preceding
     * guards were healthy. Any guard failure clears it. Never permits forward
     * progress or clears failed; only explicit terminal cleanup may consume it. */
    bool busy,checking,failed,protocol_failed,closed,connection_scope,disconnected;
};
bool net_bounded_probe_init(struct net_bounded_probe *probe,
    const struct net_bounded_probe_io *io,uint64_t epoch,uint64_t deadline_us);
/* Opt-in sequential connection scope on ONE registered/selected adapter.
 * First admission performs ordinary quiet first-init; later admission requires
 * a successful disconnect of the preceding scope and a strictly newer epoch.
 * All scopes use fresh zero objects, bound by an independent scalar active
 * object identity; copied scopes cannot acquire the original authority.
 * Adapter registry identity/MAC, monotonic
 * IDs and epoch high-water survive. A worker handoff is allowed only through the
 * explicit fresh connection worker; ordinary one-shot workers still refuse.
 * Disconnect consumes an OPEN current scope after empty protocol tables and
 * final owner/link/MAC/clock checks, closes/scrubs it and clears IPv4/DHCP/DNS/
 * ARP/ICMP configuration. It is not provider/key/DMA retirement authority.
 * Native caller must then disconnect/retire old hardware before a new attach.
 * Failed/current busy boundaries fuse further admission; stale closed scopes
 * refuse unchanged. Ordinary finish instead of disconnect terminally closes
 * admission. No former connection IO callback/context is retained; stable
 * adapter registry identity/MAC and epoch high-water remain. */
bool net_bounded_probe_init_connection(struct net_bounded_probe *probe,
    const struct net_bounded_probe_io *io,const struct net_device_ops *provider,
    uint64_t epoch,uint64_t deadline_us);
bool net_bounded_probe_disconnect(struct net_bounded_probe *probe);
bool net_bounded_probe_current(struct net_bounded_probe *probe);
bool net_bounded_probe_poll(struct net_bounded_probe *probe);
/* Prewarm exact target's next-hop MAC inside the same bounded owner/window.
 * No TCP slot,port,SYN or route fallback; one ARP request when uncached,at most
 * 1second/100ticks/10000polls,then terminal refusal. Success leaves only cache. */
int64_t net_prepare_route_bounded(struct net_bounded_probe *probe,uint32_t remote_ip);
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
/* IF0 through proof and consumption. Complete current RAM snapshot only when
 * no provider, initialized stack or worker exists; no allocation/callbacks. */
bool net_status_info_try_without_device(struct srv_net_status_info *info);
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

/* Read-only presence snapshot; caller holds IF0 until consuming the result. */
bool net_process_owned(struct process *process);
#endif
