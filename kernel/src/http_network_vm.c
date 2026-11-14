/* Explicit e1000 software-stack guest test,not a native Wi-Fi endpoint. */
#include <srvros/http_network_vm.h>
#include <srvros/http_network_probe.h>
#include <srvros/e1000.h>
#include <srvros/net_device.h>
#include <srvros/console.h>
#include <srvros/process.h>
#include <srvros/timer.h>
#include <limits.h>
#include <stddef.h>
static bool enabled,attempted,running,real_dns;
static struct http_network_probe probe;
static struct scheduler_wait_queue delay_wait;
static uint64_t pre_go_us,pre_go_ticks;
static struct {
    struct e1000_probe_info before,after;
    struct http_network_probe_io io;
    uint64_t epoch,checks,drains;
    bool receiver_closed;
} vm;
static bool equal_mac(const uint8_t *a,const uint8_t *b){unsigned bits=0;for(unsigned i=0;i<6;i++)bits|=a[i]^b[i];return !bits;}
static bool current(void *p,uint64_t epoch){
    if(p!=&vm || !running || epoch!=vm.epoch || !epoch)return false;
    struct e1000_probe_info sample={0};vm.checks++;
    return e1000_probe_sample(&sample) && sample.selected && sample.irqs_enabled &&
        sample.rx_ring==vm.before.rx_ring && sample.tx_ring==vm.before.tx_ring &&
        equal_mac(sample.mac,vm.before.mac);
}
static bool drained(void *p,uint64_t epoch){
    if(!current(p,epoch))return false;
    struct e1000_probe_info sample={0};vm.drains++;
    return e1000_probe_sample(&sample) && sample.selected && sample.irqs_enabled &&
        !sample.tx_pending && !sample.rx_ready && !sample.rx_dispatching && current(p,epoch);
}
static bool terminal_rx_handoff(void *p,uint64_t epoch){
    if(p!=&vm || vm.receiver_closed || !current(p,epoch))return false;
    struct e1000_probe_info sample={0};
    if(!e1000_probe_sample(&sample) || sample.tx_pending || sample.rx_dispatching)return false;
    /* Registry/direct software delivery is sealed. Raw DD may arrive after the
     * earlier drain; rings and hardware stay retained, never recycled here. */
    if(!e1000_probe_retire_provider())return false;
    vm.receiver_closed=true;return true;
}
static bool tick_due(void *p){return timer_ticks()>=*(uint64_t *)p;}
static bool pre_go_delay(void){
    uint64_t start=timer_monotonic_us(),tick=timer_ticks(),last=start,last_tick=tick;
    for(unsigned i=0;i<300;i++){
        uint64_t now=timer_monotonic_us(),at=timer_ticks();
        if(now<last || at<last_tick || now-start>=2000000 || at-tick>=200)return false;
        last=now;last_tick=at;pre_go_us=now-start;pre_go_ticks=at-tick;
        if(pre_go_us>=1000000 && pre_go_ticks>=100)return net_probe_worker_prepared();
        if(at==UINT64_MAX)return false;
        uint64_t end=at+1;
        if(!scheduler_wait_timeout(&delay_wait,tick_due,&end,end))scheduler_yield();
    }
    return false;
}
void http_network_vm_enable(bool explicit_isolated_ready,bool require_real_dns){
    if(!attempted && !running){enabled=explicit_isolated_ready;real_dns=enabled && require_real_dns;}
}
bool http_network_vm_run(void){
    if(!enabled || attempted || running || process_current()){
        console_write("http-network-vm: REFUSED explicit isolated RAM guest permit required or one-shot consumed\n");return false;
    }
    attempted=true;running=true;bool good=false;
    uint64_t tick_start=timer_ticks(),tick_end=0;
    console_write("http-network-vm: preparing actual HTTP ELF and private parked worker; no physical Wi-Fi or commands needed\n");
    if(!http_network_probe_prepare(&probe))goto done;
    console_write("http-network-vm: holding GO for one real prelive second; live15second window has not started\n");
    if(!pre_go_delay())goto done;
    if(!net_device_select() || !e1000_probe_sample(&vm.before) || !vm.before.selected || !vm.before.irqs_enabled)goto done;
    console_write(real_dns?"http-network-vm: running production DNS bootstrap against private controlled DNS peer; no native acceptance\n":"http-network-vm: running numeric controlled endpoint; real socket/IRQ clocks/worker/TCP retirement; no native acceptance\n");
    uint64_t start=timer_monotonic_us();if(start>INT64_MAX-HTTP_CHILD_WINDOW_US)goto done;
    vm.epoch=UINT64_C(0x6874747000000001);
    vm.io=(struct http_network_probe_io){.context=&vm,.current=current,.retry_ready=drained,.drained=drained,.terminal_rx_handoff=terminal_rx_handoff,
        .epoch=vm.epoch,.start_us=start,.deadline_us=start+HTTP_CHILD_WINDOW_US,.test_ip_override=real_dns?0:UINT32_C(0x0a000202)};
    good=http_network_probe_run(&probe,&vm.io);
 done:
    if(probe.result.prepared && !probe.result.attempted)(void)http_network_probe_abort_prepare(&probe);
    tick_end=timer_ticks();
    bool sampled=e1000_probe_sample(&vm.after);
    bool stable=sampled && vm.before.rx_ring && vm.after.rx_ring==vm.before.rx_ring && vm.after.tx_ring==vm.before.tx_ring && equal_mac(vm.after.mac,vm.before.mac);
    struct net_probe_worker_info worker={0};bool observed=net_probe_worker_snapshot(&worker);
    bool worker_retired=observed && worker.finished && worker.parked && worker.parked_waiters==1 && !worker.active && !worker.busy && !worker.poll_busy && !worker.checking;
    struct srv_net_status_info status={0};bool status_ok=net_status_info(&status)==0;
    bool clocks=tick_end>tick_start && timer_reference_ready() && timer_idle_wakeup_ready();
    good=good && probe.result.test_override==!real_dns && probe.result.dns==(real_dns?1u:0u) && pre_go_us>=1000000 && pre_go_ticks>=100 && status_ok && !status.tcp_connection_count && !status.tcp_time_wait_count && !status.listener_count && !status.pending_count && !status.udp_socket_count && probe.result.complete && probe.result.quiescent && worker_retired && !worker.failed && stable && clocks &&
        vm.receiver_closed && probe.result.receiver_closed && !vm.after.selected && !vm.after.tx_pending && !vm.after.rx_dispatching && vm.after.rx_packets>vm.before.rx_packets && vm.after.tx_packets>vm.before.tx_packets && vm.after.rx_interrupts>vm.before.rx_interrupts;
    http_network_probe_emit(&probe);
    console_printf("http-network-vm: complete=%u real_elf=%u real_sockets=%u pre_go_us=%u pre_go_ticks=%u tick_start=%u tick_end=%u clock_ready=%u worker_identity=%u supervisor_identity=%u worker_turns=%u worker_finished=%u tcp_empty=%u tx_delta=%u rx_delta=%u rx_irq_delta=%u tx_pending=%u rx_ready=%u rx_dispatching=%u rings_stable=%u tcp_slots=%u tcp_timewait=%u listeners=%u pending=%u udp=%u provider_dma_retained=1 receiver_closed=%u provider_selected=%u native_wifi=0\n",
        (uint64_t)good,(uint64_t)probe.child.prepared,(uint64_t)(probe.result.complete && probe.result.go && probe.result.child_observed && !probe.result.child_status),pre_go_us,pre_go_ticks,tick_start,tick_end,(uint64_t)clocks,worker.worker_identity,worker.supervisor_identity,worker.turns,
        (uint64_t)worker_retired,(uint64_t)probe.result.tcp_empty,vm.after.tx_packets-vm.before.tx_packets,
        vm.after.rx_packets-vm.before.rx_packets,vm.after.rx_interrupts-vm.before.rx_interrupts,
        vm.after.tx_pending,vm.after.rx_ready,vm.after.rx_dispatching,(uint64_t)stable,status.tcp_connection_count,status.tcp_time_wait_count,status.listener_count,status.pending_count,status.udp_socket_count,(uint64_t)vm.receiver_closed,(uint64_t)vm.after.selected);
    console_write(good?"http-network-vm: PASS actual ELF exact HTTP worker and terminal TCP proof; native Wi-Fi untested\n":"http-network-vm: FAIL software obligations or controlled guest workload; retained capability on uncertainty\n");
    /* Preserve the stable wrapper/source/context on every uncertain return.
     * e1000 rings remain owned until VM shutdown even on a clean workload. */
    running=false;return good;
}
