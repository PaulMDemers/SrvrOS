/* Only reached inside the explicit retained-RX diagnostic. v92 alone admits
 * five fixed calibration commands through its inspection hook; no network data
 * OUT is submitted. Cleanup remains independently admitted after forward
 * failure, but never uses a quarantined EP0 or uncertain command ring. */
static bool wifi_mac_cycle_owned(void *context,bool cleanup) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct xhci_wifi_rx *r=&m->rx;
    if(!m->mac_cycle_mode || !m->mac_cycle.attempted || !r->active || !r->attempted ||
       !r->configured || r->posts!=1 || r->stopped || r->stop_attempted ||
       !d->present || !d->slot_id || d->ep0_quarantined || !d->ep0_ring ||
       !d->descriptor_buffer || !d->descriptor_buffer_phys || firmware_command_failed || strict_control.device)return false;
    return cleanup || wifi_rx_owned(d);
}
static bool wifi_mac_cycle_read(void *context,uint32_t a,uint32_t *v,bool cleanup) {
    struct xhci_device *d=context;struct mt76_mac_cycle *s=&d->firmware.mcu.mac_cycle;
    if(!v || !wifi_mac_cycle_owned(d,cleanup))return false;
    bool ok=wifi_mac_private_usb(d,false,a,v);
    console_printf("wifi-mac-cycle-usb: op=%u cleanup=%u write=0 address=%x value=%x ok=%u\n",(uint64_t)(s->operations-1),(uint64_t)cleanup,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);
    return ok;
}
static bool wifi_mac_cycle_write(void *context,uint32_t a,uint32_t v,bool cleanup) {
    struct xhci_device *d=context;struct mt76_mac_cycle *s=&d->firmware.mcu.mac_cycle;
    if(!wifi_mac_cycle_owned(d,cleanup) || (a!=0x1004 && a!=0x1400 && a!=0x1340))return false;
    bool ok=wifi_mac_private_usb(d,true,a,&v);
    console_printf("wifi-mac-cycle-usb: op=%u cleanup=%u write=1 address=%x value=%x ok=%u\n",(uint64_t)(s->operations-1),(uint64_t)cleanup,(uint64_t)a,(uint64_t)v,(uint64_t)ok);
    return ok;
}
static bool wifi_mac_cycle_delay(void *context,unsigned us,bool cleanup) {
    struct xhci_device *d=context;struct mt76_mac_cycle *s=&d->firmware.mcu.mac_cycle;
    if(!wifi_mac_cycle_owned(d,cleanup))return false;
    uint64_t start=timer_monotonic_us();bool ok=timer_delay_us(us);uint64_t end=timer_monotonic_us();
    ok=ok && end>=start && end-start>=us;
    console_printf("wifi-mac-cycle-delay: op=%u cleanup=%u requested_us=%u elapsed_us=%u ok=%u\n",(uint64_t)(s->operations-1),(uint64_t)cleanup,(uint64_t)us,end>=start?end-start:0,(uint64_t)ok);
    return ok;
}
static uint64_t wifi_mac_cycle_now(void *context){(void)context;return timer_monotonic_us();}
#include "xhci_wifi_cal_probe.inc"
#include "xhci_wifi_poweron.inc"
#include "xhci_wifi_passive.inc"
static bool wifi_mcu_mac_cycle(struct xhci_device *d) {
    struct mt76_mac_cycle *s=&d->firmware.mcu.mac_cycle;
    if(s->attempted)return false;
    console_write(d->firmware.mcu.poweron_mode?"wifi-mac-cycle-progress: bounded MAC with power-on calibration; retained RX and independent teardown\n":"wifi-mac-cycle-progress: bounded MAC start,snapshot,device drain,disable,restore; retained RX; no calibration\n");
    const struct mt76_mac_cycle_io io={d,wifi_mac_cycle_owned,wifi_mac_cycle_read,wifi_mac_cycle_write,wifi_mac_cycle_delay,wifi_mac_cycle_now,d->firmware.mcu.passive_mode?wifi_passive_inspect:d->firmware.mcu.poweron_mode?wifi_poweron_inspect:d->firmware.mcu.cal_probe_mode?wifi_cal_probe_inspect:NULL};
    bool ok=mt76_mac_cycle_run(s,&io);
    console_printf("wifi-mac-cycle: ok=%u attempted=%u complete=%u failed=%u started=%u snapshot=%u operations=%u reads=%u writes=%u waits=%u tx_rounds=%u rx_rounds=%u mac_disabled=%u tx_drained=%u rx_drained=%u filter_restored=%u txop_restored=%u hold_us=%u calibration_commands=%u retained=1\n",(uint64_t)ok,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->failed,(uint64_t)s->started,(uint64_t)s->snapshot_complete,(uint64_t)s->operations,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->waits,(uint64_t)s->tx_rounds,(uint64_t)s->rx_rounds,(uint64_t)s->mac_disabled,(uint64_t)s->tx_drained,(uint64_t)s->rx_drained,(uint64_t)s->filter_restored,(uint64_t)s->txop_restored,s->hold_us,(uint64_t)d->firmware.mcu.poweron_commands);
    if(s->snapshot_complete)console_printf("wifi-mac-cycle-snapshot: filter=%x txop=%x tx_alc=%x ibi9=%x observation_only=1\n",(uint64_t)s->filter,(uint64_t)s->txop,(uint64_t)s->tx_alc,(uint64_t)s->ibi9);
    return ok;
}
