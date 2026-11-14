#ifndef SRVROS_HTTP_CHILD_H
#define SRVROS_HTTP_CHILD_H
#include <srvros/process.h>
#include <stdbool.h>
#include <stdint.h>
#define HTTP_CHILD_GO_BYTES 96u
#define HTTP_CHILD_WINDOW_US UINT64_C(15000000)
/* Zero once; stable caller-owned disjoint storage until retired. Never copy,
 * reinitialize or reuse a consumed owner/ticket. Private lifetime is authority. */
struct http_child_owner {
    uintptr_t self;
    struct process_pipe_handoff pipe;
    struct process_pipe_identity identity;
    uint64_t token,pid,ready_us,epoch,start,deadline,status;
    bool prepared,published,observed,reaped,retired;
};
/* PRELIVE only: actual ELF loading, allocation, READY wait and admission seal.
 * Failure retains state; caller retires it at a safe point. */
bool http_child_prepare(struct http_child_owner *,bool preflight);
/* Finite encoder/publication. Original15s window supplied by external owner,
 * never reset. No DNS/global cache or provider authority. Exact ready/image/FD
 * rechecks. Buffers must be stable and disjoint from all private owner storage. */
bool http_child_make_go(struct http_child_owner *,uint64_t epoch,uint64_t start,
    uint64_t deadline,uint32_t ip,uint8_t record[HTTP_CHILD_GO_BYTES]);
bool http_child_publish(struct http_child_owner *,uint64_t epoch,uint64_t start,
    uint64_t deadline,uint32_t ip);
/* Nonmutating complete process teardown observation, no reap or TCP/FIN/
 * TIME_WAIT, worker, provider, key or DMA retirement claim. */
bool http_child_observe(struct http_child_owner *,uint64_t *status);
/* Bounded cancellation signal only. Child teardown may send FIN; retain all
 * external guards/worker/provider/key until independently drained. */
bool http_child_cancel(struct http_child_owner *);
/* Ordinary cleanup/reap can yield/free. Call only at supervisor safe point
 * with external network obligations discharged. False retains ownership. */
bool http_child_reap(struct http_child_owner *);
bool http_child_retire(struct http_child_owner *);
#endif
