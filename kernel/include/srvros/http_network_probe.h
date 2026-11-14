#ifndef SRVROS_HTTP_NETWORK_PROBE_H
#define SRVROS_HTTP_NETWORK_PROBE_H
#include <srvros/http_child.h>
#include <srvros/net.h>
#include <srvros/scheduler.h>
/* CPU0 one-shot supervisor. Original source/context must have STATIC or equally
 * retained lifetime through every failure. Callbacks are finite/nonblocking,
 * with no waits/allocations. Live observations do not change tables; the
 * explicitly consuming terminal callback alone closes publication authority.
 * current binds exact selected provider,
 * secured epoch and original transport/packet/credit authority; retry_ready
 * proves bootstrap Discover completion; drained proves no pending/queued TX,
 * cached/borrowed RX/fence/command (an ordinary posted RX may remain). Native
 * caller retains 80TX/252RX/4fence/150000poll/15s and all hardware guards. */
struct http_network_probe_io {
    void *context;
    bool (*current)(void *,uint64_t);
    bool (*retry_ready)(void *,uint64_t);
    bool (*drained)(void *,uint64_t);
    /* Consuming CPU0/IRQ-off terminal operation, REQUIRED for a run. Called
     * once only after child reaping/retirement, TCP EMPTY and worker finish.
     * Must seal every software packet publisher and hand still-owned raw RX
     * to the retained outer cleanup owner; no decoded cache/borrowed loan or
     * pending TX may be ignored. Never frees keys, DMA or hardware contexts.
     * Success may consume current(): post-call checks use frozen descriptor
     * identity and the ORIGINAL monotonic deadline, not another live drain. */
    bool (*terminal_rx_handoff)(void *,uint64_t);
    uint64_t epoch,start_us,deadline_us;
    /* Explicit VM test endpoint only. Native MUST pass zero; receipts label
     * override use and native collector must refuse it. This explicit local-VM
     * path bypasses DNS; native zero-override resolves fixed httpbin.org. */
    uint32_t test_ip_override;
};
enum http_network_phase {HTTP_NET_EMPTY,HTTP_NET_PREPARE,HTTP_NET_READY,HTTP_NET_DHCP,
    HTTP_NET_DNS,HTTP_NET_ROUTE,HTTP_NET_HANDOFF,HTTP_NET_GO,HTTP_NET_DRAIN,
    HTTP_NET_FINISH,HTTP_NET_DONE};
enum http_network_reason {HTTP_NET_OK,HTTP_NET_ADMISSION,HTTP_NET_PRELIVE,
    HTTP_NET_OWNER,HTTP_NET_CLOCK,HTTP_NET_INIT,HTTP_NET_DHCP_FAILED,
    HTTP_NET_DNS_FAILED,HTTP_NET_ROUTE_FAILED,HTTP_NET_DRAIN_FAILED,
    HTTP_NET_HANDOFF_FAILED,HTTP_NET_GO_FAILED,HTTP_NET_CHILD_FAILED,
    HTTP_NET_WORKER_FAILED,HTTP_NET_RETIRE_FAILED,HTTP_NET_NOT_RUN,HTTP_NET_RX_HANDOFF_FAILED};
struct http_network_result {
    uint64_t version,phase,reason,dhcp,retries,dns,route,go,child_status;
    uint64_t bootstrap_polls,worker_polls,worker_turns,supervisor_turns,elapsed_us;
    bool prepared,worker_allocated,attempted,handoff,child_observed,child_reaped,child_retired;
    bool connection_scope,disconnected;
    /* provider_drained is the atomic PRE-STOP software drain snapshot; it is
     * NOT a later claim that raw RX stayed empty. receiver_closed proves the
     * consuming software ingress handoff; hardware/DMA remains outer-owned. */
    bool tcp_empty,provider_drained,receiver_closed,worker_finished,quiescent,complete,refs_scrubbed,test_override;
};
/* Zero once,retain original address forever after attempted preparation. This
 * contains live capabilities,never copy/scrub/reinitialize while retained. */
struct http_network_probe {
    uintptr_t self;
    struct http_child_owner child;
    struct net_bounded_probe bootstrap;
    struct net_bounded_probe_io net_io;
    /* Admission anchor is retained unchanged across every live callback.
     * Validate source address before dereferencing it; compare both the live
     * descriptor and original source against this independent frozen copy.
     * Scrub these capability references only after proven quiescence. */
    struct http_network_probe_io io,io_guard;
    const struct http_network_probe_io *source,*source_guard;
    struct net_probe_worker_info worker;
    struct scheduler_wait_queue wait;
    struct http_network_result result;
    uint64_t last_us,last_tick;
    uint32_t answer;
    bool checking,guard_failed,bootstrap_open,handoff_attempted,worker_stopped;
};
/* PRELIVE loading/allocation/READY wait and parked-worker allocation. No stack
 * initialization or provider attach here. Failed preparation retains capability
 * state and reports whether cleanup completed; never retry same object. */
bool http_network_probe_prepare(struct http_network_probe *);
/* Explicit fresh one-shot object for each connection; at most two worker
 * lifetimes. Preparation follows complete prior native/provider detach with
 * link DOWN, before acquiring the next connection's keys. */
bool http_network_probe_prepare_connection(struct http_network_probe *);
/* PRELIVE or never-exercised only. Strictly refuses attempted run/handoff/GO.
 * Seals unused worker admission and retires it+child without inventing a source
 * IO or hardware permission. Use after early RF-parent refusal before run().
 * May wait/yield; failure retains original object and uncertainty. */
bool http_network_probe_abort_prepare(struct http_network_probe *);
/* Run only with original live provider selected/attached and worker parked.
 * No pings precede HTTP. Original window is never restarted. False does NOT
 * authorize key/provider/DMA destruction. result.quiescent only proves child,
 * worker/TCP and software ingress handoff finished; caller still owns hardware proof.
 * If not quiescent, KEEP object/source/context/provider/key alive; halt/quarantine
 * rather than enter ordinary unconditional key teardown or reuse state. */
bool http_network_probe_run(struct http_network_probe *,const struct http_network_probe_io *);
/* Same original deadline/child/worker/terminal authority, with an exact adapter
 * identity. Deconfigures protocol state only after child retirement, TCP EMPTY
 * and worker finish, before consuming terminal RX handoff. */
bool http_network_probe_run_connection(struct http_network_probe *,const struct http_network_probe_io *,const struct net_device_ops *);
/* Post-live emission only; no address,packet,key or credential bytes. */
void http_network_probe_emit(const struct http_network_probe *);
#endif
