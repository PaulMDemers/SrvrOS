#ifndef HTTPGET_WIFI_EXACT_H
#define HTTPGET_WIFI_EXACT_H
#include <stdint.h>
/* Explicit capture mode only: /httpget --wifi-exact 3. No service startup,
 * hostname lookup, allocation, console/file output, or argv mutation.
 * Fresh kernel-owned NONBLOCK read pipe FD3 carries exactly96 bytes then EOF:
 * 0:8 ASCII SRVRHTP1, 8:u32 version1, 12:u32 size96, 16:u64 pid,
 * 24:u64 lifetime,32:u64 image generation,40:u64 provider epoch,
 * 48:u64 original start_us,56:u64 original deadline_us,64:u32 IPv4,
 * 68:u16 port80,70:u16 zero,72:24 zero. All integers little-endian;
 * IPv4 integer uses the SDK's high-octet-first numeric representation.
 * Deadline must equal start+15000000, start must be no earlier than this
 * prepared process and no later than current monotonic time. Tokens/epoch
 * must be nonzero; kernel ticket, not this reader, authenticates PID/image/
 * lifetime/FD identity. No new producer/syscall or global DNS cache is supplied.
 *
 * GO wait uses its separate60s monotonic deadline while calls/ticks progress;
 * PREGO AGAIN sleeps one tick with clock checks before/after. A broken IRQ/tick
 * can stall inside SYS_SLEEP; this does not promise a wall-time/wait-free bound
 * for that syscall. Parent supervises cancellation and retains resources.
 * The150000check cap bounds returned loops, including returned frozen clocks;
 * it cannot force a blocked syscall to return. After GO, the received original
 * 15s deadline bounds pipe EOF and network work.150000 checks span all phases,
 * never reset, so frozen clock/AGAIN loops terminate. Checks before/after calls
 * reject late results and rollback; these cannot bound time spent INSIDE normal
 * ISR admission or syscall implementations. Existing try-read performs one
 * implicit provider poll on empty input. Parent must account it and enforce
 * original provider/clock/credit identity on every publication independently.
 *
 * Success means exact request and HTTP body+actual recv EOF, errorNONE and
 * accepted close before observed deadline. It does NOT mean TCP TIME_WAIT,
 * worker/provider/key/DMA or process-lifetime retirement. Any failure is
 * terminal: no connect/send retry after an error; runtime exit owns FD cleanup,
 * whose possible FINs still require the parent's provider publication guard.
 */
#define WIFI_EXACT_GO_BYTES 96u
#define WIFI_EXACT_WINDOW_US UINT64_C(15000000)
#define WIFI_EXACT_GO_WAIT_US UINT64_C(60000000)
#define WIFI_EXACT_CHECK_CAP 150000u
enum wifi_exact_exit {
 WIFI_EXACT_OK=0,WIFI_EXACT_ARGS=2,WIFI_EXACT_PREP=10,WIFI_EXACT_ABI=11,
 WIFI_EXACT_FD=12,WIFI_EXACT_CLOCK=13,WIFI_EXACT_READY=14,WIFI_EXACT_PIPE=15,
 WIFI_EXACT_GO=16,WIFI_EXACT_CONNECT=17,WIFI_EXACT_STATUS=18,WIFI_EXACT_SEND=19,
 WIFI_EXACT_RECV=20,WIFI_EXACT_HTTP=21,WIFI_EXACT_CLOSE=22,
 WIFI_EXACT_CHECKS=23,WIFI_EXACT_DEADLINE=24,WIFI_EXACT_SLEEP=25
};
/* --wifi-preflight shares preparation/READY/GO/real EOF/close, then exits
 * before every socket operation; it is a process/pipe-only guest diagnostic. */
int wifi_exact_main(int argc,char **argv);
#endif
