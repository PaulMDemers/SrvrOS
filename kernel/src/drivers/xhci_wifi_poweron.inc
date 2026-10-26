static bool wifi_poweron_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->poweron_mode && m->poweron_active && m->poweron.attempted &&
        m->cal_probe.complete && m->cal_probe.ibi_write_matched && m->cal_probe.ibi_changed &&
        m->cal_probe.raw_restore_mask==3 && mt76_ant_fields_valid(&m->ant_fields) &&
        mt76_trim_fields_valid(&m->trim_fields) && m->mac_cycle.started &&
        m->mac_cycle.snapshot_complete && !m->mac_cycle.complete &&
        m->mac_cycle.tx_alc==0x2f2f1b1b && m->mac_cycle.ibi9==UINT32_MAX &&
        wifi_mac_cycle_owned(d,false);
}
static bool wifi_poweron_usb(struct xhci_device *d,bool cleanup,bool write,uint32_t a,uint32_t *v) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!v || !m->poweron_mode || !m->poweron_active || !m->poweron.attempted ||
       !(cleanup?wifi_mac_cycle_owned(d,true):wifi_poweron_owned(d)))return false;
    if(a!=0x1004 && a!=0x2000 && a!=0x208 && a!=0x13b0 && a!=0x2124)return false;
    unsigned op=m->poweron_ops++;if(write)m->poweron_writes++;else m->poweron_reads++;
    bool ok=wifi_mac_private_usb(d,write,a,v);
    console_printf("wifi-poweron-usb: op=%u cleanup=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)cleanup,(uint64_t)write,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);
    return ok;
}
static bool wifi_poweron_read(void *c,uint32_t a,uint32_t *v){return wifi_poweron_usb(c,false,false,a,v);}
static bool wifi_poweron_write(void *c,uint32_t a,uint32_t v){
    if(!((a==0x13b0 && v==0) || (a==0x2124 && v==0xffffff7e)))return false;
    return wifi_poweron_usb(c,false,true,a,&v);
}
static bool wifi_poweron_restore(void *context,uint32_t a,uint32_t saved,uint32_t *seen) {
    struct xhci_device *d=context;struct mt76_calibration *s=&d->firmware.mcu.poweron;
    unsigned i=a==0x13b0?0:1,bit=1u<<i;
    if(!seen || (a!=0x13b0 && a!=0x2124) || !(s->saved_mask&bit) || !(s->modified_mask&bit) || saved!=s->saved[i])return false;
    bool written=wifi_poweron_usb(d,true,true,a,&saved);*seen=0;
    bool read=wifi_poweron_usb(d,true,false,a,seen);return written && read;
}
static bool wifi_poweron_delay(void *context,unsigned us) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if((us!=10 && us!=500 && us!=15000) || !wifi_poweron_owned(d))return false;
    unsigned op=m->poweron_ops++;m->poweron_waits++;
    uint64_t start=timer_monotonic_us();bool ok=timer_delay_us(us);uint64_t end=timer_monotonic_us();
    ok=ok && end>=start && end-start>=us;
    console_printf("wifi-poweron-delay: op=%u requested_us=%u elapsed_us=%u ok=%u\n",(uint64_t)op,(uint64_t)us,end>=start?end-start:0,(uint64_t)ok);
    return ok;
}
static bool wifi_poweron_command(void *context,enum mt76_poweron_command cmd,uint32_t argument) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    static const unsigned args[5]={0,1,0x600,0,1};
    unsigned step=m->poweron_commands;
    if(step>=5 || (unsigned)cmd!=step || argument!=args[step] ||
       !wifi_poweron_owned(d) || !m->configured || m->pending ||
       !m->response.ring || !m->buffer || !d->firmware.buffer)return false;
    uint8_t packet[16];if(mt76_poweron_packet(step,packet,sizeof packet)!=16)return false;
    unsigned op=m->poweron_ops++;m->poweron_commands++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    uint64_t start=timer_monotonic_us();
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    /* No second transfer after ownership loss; cleanup still uses its own gate. */
    uint64_t after_out=timer_monotonic_us();
    if(sent==16 && after_out>=start && after_out-start<500000 && wifi_poweron_owned(d))received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint64_t end=timer_monotonic_us();uint32_t words[4]={0};
    if(received==16)for(unsigned i=0;i<16;i++)words[i/4]|=(uint32_t)m->buffer[i]<<(8*(i%4));
    bool ok=sent==16 && received==16 && end>=start && end-start<=1000000 &&
        (m->completion==1 || m->completion==13) && m->remaining==MT76_MCU_RESPONSE_MAX-16 &&
        mt76_poweron_reply(step,m->buffer,16) && wifi_poweron_owned(d);
    if(ok)m->poweron_ack_mask|=1u<<step;
    console_printf("wifi-poweron-command: op=%u step=%u sequence=%u argument=%x sent=%d received=%d cc=%u residue=%u elapsed_us=%u h=%x p0=%x p1=%x trailer=%x validated=%u\n",(uint64_t)op,(uint64_t)step,(uint64_t)(step+1),(uint64_t)argument,(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,end>=start?end-start:0,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3],(uint64_t)ok);
    return ok;
}
static bool wifi_poweron_inspect(void *context,uint32_t tx,uint32_t ibi) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_calibration *s=&m->poweron;
    if(!m->poweron_mode || s->attempted || m->poweron_active || tx!=0x2f2f1b1b || ibi!=UINT32_MAX)return false;
    if(!wifi_cal_probe_inspect(d,tx,ibi) || !m->cal_probe.ibi_write_matched || !m->cal_probe.ibi_changed)return false;
    m->poweron_active=true;
    console_write("wifi-poweron-progress: R,VCO,wait10us,suppress power,wait500us,IBI,FULL,LC,wait15ms,restore,RXDCOC; no packet delivery\n");
    const struct mt76_poweron_io io={d,wifi_poweron_owned,wifi_poweron_read,wifi_poweron_write,wifi_poweron_command,wifi_poweron_delay,wifi_poweron_restore};
    enum mt76_calibration_status status=mt76_poweron_calibration_run(s,&io,&d->wifi_identity,&m->ant_fields,1,20);
    m->poweron_active=false;
    console_printf("wifi-poweron: status=%u attempted=%u complete=%u operations=%u reads=%u writes=%u waits=%u commands=%u ack_mask=%u saved_mask=%u modified_mask=%u restore_attempted=%u restored_mask=%u cleanup_complete=%u radio_acceptance=0\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)m->poweron_ops,(uint64_t)m->poweron_reads,(uint64_t)m->poweron_writes,(uint64_t)m->poweron_waits,(uint64_t)m->poweron_commands,(uint64_t)m->poweron_ack_mask,(uint64_t)s->saved_mask,(uint64_t)s->modified_mask,(uint64_t)s->restore_attempted,(uint64_t)s->restored_mask,(uint64_t)s->cleanup_complete);
    console_printf("wifi-poweron-values: saved_mask=%u tx_alc=%x ibi_saved=%x restored_mask=%u tx_alc_restored=%x ibi_restored=%x\n",(uint64_t)s->saved_mask,(uint64_t)s->saved[0],(uint64_t)s->saved[1],(uint64_t)s->restored_mask,(uint64_t)s->restored[0],(uint64_t)s->restored[1]);
    return status==MT76_CAL_OK && s->complete && m->poweron_ack_mask==31;
}
