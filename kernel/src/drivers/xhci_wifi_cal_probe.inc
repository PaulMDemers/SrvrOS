static bool wifi_cal_probe_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->cal_probe_mode && m->cal_probe_active && m->cal_probe.attempted &&
        m->mac_cycle.started && m->mac_cycle.snapshot_complete && !m->mac_cycle.complete &&
        m->mac_cycle.tx_alc==0x2f2f1b1b && m->mac_cycle.ibi9==UINT32_MAX && wifi_mac_cycle_owned(d,false);
}
static bool wifi_cal_probe_usb(struct xhci_device *d,bool cleanup,bool write,uint32_t a,uint32_t *v) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!v || !m->cal_probe_mode || !m->cal_probe_active || !m->cal_probe.attempted ||
       !(cleanup?wifi_mac_cycle_owned(d,true):wifi_cal_probe_owned(d)))return false;
    if(a!=0x1004 && a!=0x2000 && a!=0x208 && a!=0x13b0 && a!=0x2124)return false;
    unsigned op=m->cal_probe_ops++;
    if(write)m->cal_probe_writes++;else m->cal_probe_reads++;
    bool ok=wifi_mac_private_usb(d,write,a,v);
    console_printf("wifi-cal-probe-usb: op=%u cleanup=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)cleanup,(uint64_t)write,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);
    return ok;
}
static bool wifi_cal_probe_read(void *context,uint32_t a,uint32_t *v){return wifi_cal_probe_usb(context,false,false,a,v);}
static bool wifi_cal_probe_write(void *context,uint32_t a,uint32_t v){
    if(!((a==0x13b0 && v==0) || (a==0x2124 && v==0xffffff7e)))return false;
    return wifi_cal_probe_usb(context,false,true,a,&v);
}
static bool wifi_cal_probe_restore(void *context,uint32_t a,uint32_t saved,uint32_t *observed) {
    struct xhci_device *d=context;struct mt76_calibration_probe *s=&d->firmware.mcu.cal_probe;
    unsigned i=a==0x13b0?0:1;
    if(!observed || (a!=0x13b0 && a!=0x2124) || !(s->modified_mask&(1u<<i)) || !(s->saved_mask&(1u<<i)) || saved!=s->saved[i])return false;
    bool written=wifi_cal_probe_usb(d,true,true,a,&saved);*observed=0;
    bool read=wifi_cal_probe_usb(d,true,false,a,observed);
    return written && read;
}
static bool wifi_cal_probe_delay(void *context,unsigned us) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(us!=500 || !wifi_cal_probe_owned(d))return false;
    unsigned op=m->cal_probe_ops++;m->cal_probe_waits++;
    uint64_t start=timer_monotonic_us();bool ok=timer_delay_us(us);uint64_t end=timer_monotonic_us();
    ok=ok && end>=start && end-start>=us;
    console_printf("wifi-cal-probe-delay: op=%u requested_us=500 elapsed_us=%u ok=%u\n",(uint64_t)op,end>=start?end-start:0,(uint64_t)ok);
    return ok;
}
static bool wifi_cal_probe_inspect(void *context,uint32_t tx_alc,uint32_t ibi9) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_calibration_probe *s=&m->cal_probe;
    if(!m->cal_probe_mode || s->attempted || m->cal_probe_active || tx_alc!=0x2f2f1b1b || ibi9!=UINT32_MAX)return false;
    m->cal_probe_active=true;
    console_write("wifi-cal-probe-progress: save,suppress TX_ALC0,wait500us,observe IBI9 write,restore; no calibration commands\n");
    const struct mt76_calibration_io io={d,wifi_cal_probe_owned,wifi_cal_probe_read,wifi_cal_probe_write,NULL,wifi_cal_probe_delay,wifi_cal_probe_restore};
    enum mt76_calibration_status status=mt76_calibration_probe_run(s,&io);m->cal_probe_active=false;
    console_printf("wifi-cal-probe: status=%u attempted=%u complete=%u operations=%u reads=%u writes=%u waits=%u saved_mask=%u modified_mask=%u restore_attempted=%u raw_restore_mask=%u ibi_write_matched=%u ibi_changed=%u calibration_commands=0 observation_only=1\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)m->cal_probe_ops,(uint64_t)m->cal_probe_reads,(uint64_t)m->cal_probe_writes,(uint64_t)m->cal_probe_waits,(uint64_t)s->saved_mask,(uint64_t)s->modified_mask,(uint64_t)s->restore_attempted,(uint64_t)s->raw_restore_mask,(uint64_t)s->ibi_write_matched,(uint64_t)s->ibi_changed);
    console_printf("wifi-cal-probe-values: saved_mask=%u tx_alc=%x ibi_saved=%x ibi_observed=%x raw_restore_mask=%u tx_alc_restored=%x ibi_restored=%x\n",(uint64_t)s->saved_mask,(uint64_t)s->saved[0],(uint64_t)s->saved[1],(uint64_t)s->observed_ibi,(uint64_t)s->raw_restore_mask,(uint64_t)s->restored[0],(uint64_t)s->restored[1]);
    return status==MT76_CAL_OK && s->complete;
}
