/* Explicit channel6 diagnostic. The old main RX ledger is immutable; both new
 * pages remain allocated after completion, stop and every failure. */
static bool wifi_receive_layout_for(struct xhci_device *d,struct xhci_wifi_receive *r) {
    if(!r->ep.ring || !r->buffer || !r->ep.ring_phys || !r->buffer_phys ||
       (r->ep.ring_phys&4095) || (r->buffer_phys&4095) || r->ep.ring_phys==r->buffer_phys ||
       r->ep.ring_phys>UINT64_MAX-4095 || r->buffer_phys>UINT64_MAX-4095 ||
       r->ep.ring!=pmm_phys_to_virt(r->ep.ring_phys) || r->buffer!=pmm_phys_to_virt(r->buffer_phys) ||
       r->identity[0]!=(uintptr_t)r->ep.ring || r->identity[1]!=r->ep.ring_phys ||
       r->identity[2]!=(uintptr_t)r->buffer || r->identity[3]!=r->buffer_phys ||
       !r->identity[4] || !r->identity[5] || r->identity[4]!=(uintptr_t)xhci.command_ring || r->identity[5]!=xhci.command_ring_phys ||
       r->ep.dci!=9 || r->ep.packet!=512 || !r->ep.cycle || r->ep.enqueue!=(r->posted?1u:0u))return false;
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    const uint64_t used[]={m->rx.ep.ring_phys,d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,
        d->firmware.buffer_phys,d->firmware.ep.ring_phys,m->buffer_phys,m->response.ring_phys};
    for(unsigned i=0;i<sizeof used/sizeof used[0];i++)if(r->ep.ring_phys==used[i] || r->buffer_phys==used[i])return false;
    for(unsigned i=0;i<8;i++)if(r->ep.ring_phys==m->window.plan.physical[i] || r->buffer_phys==m->window.plan.physical[i])return false;
    if(m->scan_mode)for(unsigned i=0;i<m->scan.completed && i<3;i++) {
        if(r==&m->scan.visit[i].receive || (r==&m->receive && m->scan.index==i && m->scan.completed==i+1))continue;
        const struct xhci_wifi_receive *prior=&m->scan.visit[i].receive;
        if(r->ep.ring_phys==prior->ep.ring_phys || r->ep.ring_phys==prior->buffer_phys ||
           r->buffer_phys==prior->ep.ring_phys || r->buffer_phys==prior->buffer_phys)return false;
    }
    if(r->ep.ring_phys==xhci.command_ring_phys || r->buffer_phys==xhci.command_ring_phys)return false;
    if(r->posted) {
        const struct xhci_trb *v=&r->ep.ring[0];
        if(r->trb!=r->ep.ring_phys || (v->parameter_low|((uint64_t)v->parameter_high<<32))!=r->buffer_phys ||
           v->status!=4096 || v->control!=((XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP|1))return false;
    }
    return true;
}
static bool wifi_receive_layout(struct xhci_device *d){return wifi_receive_layout_for(d,&d->firmware.mcu.receive);}
static bool wifi_receive_context_for(struct xhci_device *d,struct xhci_wifi_receive *r,bool stopped,bool cleanup) {
    if(!d->output_context || !r->retargeted || !wifi_receive_layout_for(d,r))return false;
    __asm__ volatile("" ::: "memory");
    unsigned state=context_read32(d->output_context,9,0)&7;
    uint64_t deq=context_read32(d->output_context,9,2)|((uint64_t)context_read32(d->output_context,9,3)<<32);
    if((deq&15)!=1 || ((deq&~15ull)!=r->ep.ring_phys && (deq&~15ull)!=r->ep.ring_phys+16))return false;
    return stopped?state==3:(state==1 || state==3 || (cleanup && state==2));
}
static bool wifi_receive_context(struct xhci_device *d,bool stopped,bool cleanup){return wifi_receive_context_for(d,&d->firmware.mcu.receive,stopped,cleanup);}
static bool wifi_receive_base_stopped(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!m->scan_mode || !m->scan.index)return wifi_channel_stopped(d);
    if(!m->scan.active || m->scan.index>=3 || m->scan.completed!=m->scan.index)return false;
    struct xhci_wifi_receive *r=&m->scan.visit[m->scan.index-1].receive;
    return r->complete && !r->failed && r->mac_quiescent && r->stopped && r->stop_attempted && !r->pending &&
        wifi_receive_context_for(d,r,true,true);
}
static bool wifi_receive_quiesced(struct xhci_device *d) {
    struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    if(!r->retarget_attempted)return wifi_receive_base_stopped(d);
    return r->retargeted && r->mac_quiescent && !r->pending &&
        (!r->posted || (r->stop_attempted && r->stopped)) && wifi_receive_context(d,true,true);
}
static bool wifi_receive_owned(struct xhci_device *d,bool cleanup) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_tune *t=&m->tune;struct xhci_wifi_receive *r=&m->receive;
    if(!m->receive_mode || !r->attempted || !r->active || !t->active || !t->pinned || !t->observing ||
       !t->state.observe_attempted || !t->state.target_complete || !t->state.target_vco_clear || t->state.channel!=wifi_tune_channel(d) ||
       !wifi_mcu_owned(d) || !d->present || d->vendor_id!=0x7392 || d->product_id!=0xa711 || d->device_bcd!=0x100 ||
       !d->slot_id || !d->ep0_ring || !d->descriptor_buffer || !d->output_context || !d->input_context ||
       firmware_command_failed || strict_control.device || m->pending || !m->configured ||
       d->firmware.ep.dci!=16 || m->response.dci!=11 || d->firmware.ep.packet!=512 || m->response.packet!=512 ||
       r->reads>20000 || r->writes>100 || r->waits>10000 || r->polls>20000000)return false;
    if(m->scan_mode && !wifi_scan_history_owned(d))return false;
    uint64_t identity[18];wifi_channel_identity(d,identity);
    for(unsigned i=0;i<18;i++)if(!identity[i] || identity[i]!=t->identity[i])return false;
    return cleanup || (!r->failed && !t->forward_failed && !t->state.failed &&
        wifi_window_owned(d,true) && m->window.state.complete && !m->window.state.failed &&
        m->discovery.complete && !m->discovery.failed && mt76_ant_fields_valid(&m->ant_fields) && mt76_trim_fields_valid(&m->trim_fields));
}
static bool wifi_receive_mac_owned(void *context,bool clean) {
    struct xhci_device *d=context;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    return r->mac_active && r->posted && wifi_receive_owned(d,clean) && wifi_receive_context(d,false,clean);
}
static bool wifi_receive_read(void *context,uint32_t a,uint32_t *v,bool clean) {
    struct xhci_device *d=context;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    if(!v || !wifi_receive_mac_owned(d,clean))return false;
    switch(a){case 0x1004:case 0x2000:case 0x208:case 0x1400:case 0x1340:case 0x13b0:case 0x2124:case 0x438:case 0xa30:case 0xa34:case 0x1200:case 0x430:break;default:return false;}
    r->reads++;bool ok=wifi_mac_private_usb(d,false,a,v);
    if(ok && !clean && a==0x1340)ok=*v==d->firmware.mcu.mac_cycle.txop;
    console_printf("wifi-channel-rx-usb: op=%u cleanup=%u write=0 address=%x value=%x ok=%u\n",(uint64_t)(r->reads+r->writes+r->waits-1),(uint64_t)clean,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);
    if(!ok)r->failed=true;
    return ok;
}
static bool wifi_receive_write(void *context,uint32_t a,uint32_t v,bool clean) {
    struct xhci_device *d=context;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    if(!wifi_receive_mac_owned(d,clean) ||
       !((a==0x1004 && (clean?v==0:(v==4 || v==12))) || (a==0x1400 && v==0x17f97) ||
         (a==0x1340 && clean && (v==r->mac.txop || v==(r->mac.txop&~(1u<<20))))))return false;
    r->writes++;bool ok=wifi_mac_private_usb(d,true,a,&v);
    console_printf("wifi-channel-rx-usb: op=%u cleanup=%u write=1 address=%x value=%x ok=%u\n",(uint64_t)(r->reads+r->writes+r->waits-1),(uint64_t)clean,(uint64_t)a,(uint64_t)v,(uint64_t)ok);
    if(!ok)r->failed=true;
    return ok;
}
static bool wifi_receive_delay(void *context,unsigned us,bool clean) {
    struct xhci_device *d=context;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    if((us!=50 && us!=1000 && us!=10000) || !wifi_receive_mac_owned(d,clean))return false;
    r->waits++;uint64_t start=timer_monotonic_us();bool ok=timer_delay_us(us);uint64_t end=timer_monotonic_us();
    ok=ok && end>=start && end-start>=us && wifi_receive_mac_owned(d,clean);
    console_printf("wifi-channel-rx-delay: op=%u cleanup=%u requested_us=%u elapsed_us=%u ok=%u\n",(uint64_t)(r->reads+r->writes+r->waits-1),(uint64_t)clean,(uint64_t)us,end>=start?end-start:0,(uint64_t)ok);
    if(!ok)r->failed=true;
    return ok;
}
static bool wifi_receive_event(struct xhci_device *d,const struct xhci_trb *e) {
    struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    uint64_t ptr=e->parameter_low|((uint64_t)e->parameter_high<<32);
    struct xhci_wifi_scan *scan=&d->firmware.mcu.scan;
    if(d->firmware.mcu.scan_mode && ((e->control>>10)&63)==32 &&
       (e->control>>24)==d->slot_id && ((e->control>>16)&31)==9)
        for(unsigned i=0;i<scan->completed && i<3;i++)if(ptr==scan->visit[i].receive.trb && ptr!=r->trb){scan->late++;return true;}
    if(!d->firmware.mcu.receive_mode || !r->retarget_attempted || !r->posted ||
       ((e->control>>10)&63)!=32 || (e->control>>24)!=d->slot_id || ((e->control>>16)&31)!=9 || ptr!=r->trb)return false;
    if(!r->pending || r->stopped || !r->active){r->late++;return true;}
    if(!r->retargeted || !wifi_receive_layout(d)){r->failed=true;return true;}
    r->pending=false;r->done=true;r->events++;r->completion=e->status>>24;r->remaining=e->status&0xffffff;
    bool normal=!(e->control&4) && (r->completion==1 || r->completion==13) && r->remaining<=4096;
    bool cancel=!(e->control&4) && r->stopping && r->completion>=26 && r->completion<=28 && (r->completion==27 || r->remaining<=4096);
    r->during_window=normal && r->polling && r->mac.started && r->mac.snapshot_complete && !r->stopping;
    if(!normal && !cancel)r->failed=true;
    return true;
}
static bool wifi_receive_poll(void *context,uint32_t tx,uint32_t ibi) {
    struct xhci_device *d=context;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    if(tx!=0x2f2f1b1b || ibi!=UINT32_MAX || !wifi_receive_mac_owned(d,false))return false;
    r->polling=true;uint64_t start=timer_monotonic_us(),previous=start;unsigned frozen=0;bool ok=true;
    while(!r->done && r->polls<20000000) {
        if(!wifi_receive_mac_owned(d,false)){ok=false;break;}
        uint64_t flags=event_irq_save();struct xhci_trb event;
        if(poll_event(&event))dispatch_transfer_event(&event);
        event_irq_restore(flags);r->polls++;
        uint64_t now=timer_monotonic_us();
        if(now<previous || (now==previous && ++frozen>=1024)){ok=false;break;}
        if(now!=previous)frozen=0;
        previous=now;if(now-start>=10000000)break;
    }
    r->elapsed_us=previous>=start?previous-start:0;r->polling=false;
    ok=ok && !r->failed && r->polls<20000000 && r->elapsed_us<=10100000 && (r->done || r->elapsed_us>=10000000);
    if(!ok)r->failed=true;
    console_printf("wifi-channel-rx-poll: ok=%u polls=%u elapsed_us=%u done=%u during_window=%u\n",(uint64_t)ok,(uint64_t)r->polls,r->elapsed_us,(uint64_t)r->done,(uint64_t)r->during_window);
    return ok;
}
static bool wifi_receive_stop(struct xhci_device *d) {
    struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    /* EP0/MCU failure must not suppress an independently owned command-ring
     * stop. Never issue it after command-ring loss or changed DMA identities. */
    if(!d->firmware.mcu.receive_mode || !r->attempted || !r->active ||
       !d->firmware.mcu.tune.active || !d->firmware.mcu.tune.pinned || !d->firmware.mcu.tune.observing ||
       r->stop_attempted || !r->retargeted || !d->present ||
       d->slot_id!=d->firmware.mcu.tune.identity[0] || (uintptr_t)d->output_context!=d->firmware.mcu.tune.identity[5] ||
       d->vendor_id!=0x7392 || d->product_id!=0xa711 || d->device_bcd!=0x100 ||
       firmware_command_failed || strict_control.device || !wifi_receive_layout(d))return false;
    r->stop_attempted=r->stopping=true;uint64_t flags=event_irq_save(),slot=0;
    struct xhci_trb cmd={.control=(15u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
    firmware_command_exact=true;bool ok=submit_command_trb(&cmd,15,&slot) && slot==d->slot_id;firmware_command_exact=false;r->stopping=false;
    if(!ok)firmware_command_failed=true;
    if(ok)ok=wifi_receive_context(d,true,true);
    if(ok){r->dequeue=context_read32(d->output_context,9,2)|((uint64_t)context_read32(d->output_context,9,3)<<32);r->pending=false;r->stopped=true;}
    event_irq_restore(flags);
    console_printf("wifi-channel-rx-stop: attempted=1 ok=%u state=%u dequeue=%x trb=%x retained=1\n",(uint64_t)ok,(uint64_t)(context_read32(d->output_context,9,0)&7),r->dequeue,r->trb);
    return ok;
}
static bool wifi_receive_capture(struct xhci_device *d,unsigned channel) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_receive *r=&m->receive;
    if(channel!=wifi_tune_channel(d) || !channel || !m->receive_mode || r->attempted || !wifi_receive_base_stopped(d))return false;
    *r=(struct xhci_wifi_receive){.attempted=true,.active=true,.view_status=255,.beacon_status=255};
    if(m->scan_mode)console_printf("wifi-channel-rx-progress: fresh retained channel%u IN; bounded MAC/receive window then exact stop before channel1 restoration\n",(uint64_t)channel);
    else console_write("wifi-channel-rx-progress: fresh retained channel6 IN; bounded MAC/receive window then exact stop before channel1 restoration\n");
    bool ok=wifi_receive_owned(d,false);void *p=0;
    if(ok)ok=alloc_frame_zero(&r->ep.ring_phys,&p);
    if(ok){r->ep.ring=p;r->ep.dci=9;r->ep.packet=512;r->ep.cycle=true;write_link_trb(r->ep.ring,r->ep.ring_phys,true);ok=alloc_frame_zero(&r->buffer_phys,&p);}
    if(ok){r->buffer=p;r->identity[0]=(uintptr_t)r->ep.ring;r->identity[1]=r->ep.ring_phys;r->identity[2]=(uintptr_t)r->buffer;r->identity[3]=r->buffer_phys;r->identity[4]=(uintptr_t)xhci.command_ring;r->identity[5]=xhci.command_ring_phys;ok=wifi_receive_layout(d);}
    if(ok)ok=wifi_receive_owned(d,false) && wifi_receive_base_stopped(d) && wifi_receive_layout(d);
    if(ok) {
        uint64_t flags=event_irq_save(),slot=0;r->retarget_attempted=true;
        struct xhci_trb cmd={.parameter_low=(uint32_t)(r->ep.ring_phys|1),.parameter_high=r->ep.ring_phys>>32,
            .control=(16u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
        firmware_command_exact=true;ok=submit_command_trb(&cmd,16,&slot) && slot==d->slot_id;firmware_command_exact=false;
        if(!ok)firmware_command_failed=true;
        if(ok){r->retargeted=true;ok=wifi_receive_context(d,true,false);}
        event_irq_restore(flags);
        console_printf("wifi-channel-rx-retarget: attempted=1 ok=%u fresh_ring=%x old_ring=%x retained=1\n",(uint64_t)ok,r->ep.ring_phys,m->rx.ep.ring_phys);
    }
    if(ok)ok=wifi_receive_owned(d,false) && wifi_receive_context(d,true,false);
    if(ok) {
        uint64_t flags=event_irq_save();struct xhci_trb td={.parameter_low=(uint32_t)r->buffer_phys,.parameter_high=r->buffer_phys>>32,
            .status=4096,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
        r->pending=r->posted=true;r->trb=ring_push(r->ep.ring,r->ep.ring_phys,&r->ep.enqueue,&r->ep.cycle,&td);
        ok=wifi_receive_layout(d);
        if(ok)db_write32(d->slot_id,9);
        event_irq_restore(flags);
        console_printf("wifi-channel-rx-post: count=1 requested=4096 submitted=%u ring=%x buffer=%x trb=%x old_ring=%x old_pages=8 distinct=1 retained=1 reused=0\n",(uint64_t)ok,r->ep.ring_phys,r->buffer_phys,r->trb,m->rx.ep.ring_phys);
    }
    if(ok) {
        r->mac_active=true;
        const struct mt76_mac_cycle_io io={d,wifi_receive_mac_owned,wifi_receive_read,wifi_receive_write,wifi_receive_delay,wifi_mac_cycle_now,wifi_receive_poll};
        ok=mt76_mac_cycle_run(&r->mac,&io);
    }
    if(r->retargeted && (r->posted || !wifi_receive_context(d,true,true))) {bool stop=wifi_receive_stop(d);ok=stop && ok;}
    /* Fresh independent disabled/idle observations are required even if MAC
     * teardown reported success. Failed/unknown cleanup never permits retuning. */
    if(r->retargeted && wifi_receive_owned(d,true)) {
        const uint32_t addr[4]={0x1004,0x2000,0x208,0x1200},want[4]={0,0xf000f200,0x40000050,0};
        bool quiet=true;
        for(unsigned i=0;i<4;i++) {
            bool read=wifi_receive_owned(d,true) && wifi_mac_private_usb(d,false,addr[i],&r->guards[i]);
            quiet=read && r->guards[i]==want[i] && quiet;
            console_printf("wifi-channel-rx-guard: index=%u address=%x value=%x ok=%u\n",(uint64_t)i,(uint64_t)addr[i],(uint64_t)r->guards[i],(uint64_t)read);
        }
        r->mac_quiescent=quiet && (!r->mac.mac_touched || (r->mac.mac_disabled && r->mac.tx_drained && r->mac.rx_drained));
    }
    ok=ok && wifi_receive_quiesced(d) && !r->failed && r->mac.complete;
    if(ok && r->done && (r->completion==1 || r->completion==13) && r->remaining<=4096) {
        r->actual=4096-r->remaining;
        if(r->during_window && r->actual) {
            r->inspected=true;r->view_status=mt76_rx_usb_view(r->buffer,r->actual,&r->view);
            if(r->view_status==MT76_RX_OK)r->beacon_status=mt76_rx_beacon(r->buffer,r->actual,channel,&r->beacon);
        }
    }
    r->complete=ok;r->failed|=!ok;r->active=false;r->mac_active=false;return ok;
}
static void wifi_receive_log(struct xhci_device *d) {
    struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    struct mt76_mac_cycle *s=&r->mac;
    console_printf("wifi-channel-rx-mac: attempted=%u complete=%u failed=%u started=%u snapshot=%u operations=%u reads=%u writes=%u waits=%u tx_rounds=%u rx_rounds=%u mac_disabled=%u tx_drained=%u rx_drained=%u filter_restored=%u txop_restored=%u hold_us=%u filter=%x txop=%x tx_alc=%x ibi9=%x\n",(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->failed,(uint64_t)s->started,(uint64_t)s->snapshot_complete,(uint64_t)s->operations,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->waits,(uint64_t)s->tx_rounds,(uint64_t)s->rx_rounds,(uint64_t)s->mac_disabled,(uint64_t)s->tx_drained,(uint64_t)s->rx_drained,(uint64_t)s->filter_restored,(uint64_t)s->txop_restored,s->hold_us,(uint64_t)s->filter,(uint64_t)s->txop,(uint64_t)s->tx_alc,(uint64_t)s->ibi9);
    console_printf("wifi-channel-rx: attempted=%u complete=%u failed=%u retargeted=%u posted=%u stop_attempted=%u stopped=%u pending=%u mac_complete=%u mac_quiescent=%u reads=%u writes=%u waits=%u polls=%u elapsed_us=%u events=%u cc=%u remaining=%u actual=%u during_window=%u inspected=%u view_status=%u beacon_status=%u consumed=%u mpdu=%u frame_control=%x retained=1 reused=0 channel=%u data_out=0 identifiers_logged=0\n",(uint64_t)r->attempted,(uint64_t)r->complete,(uint64_t)r->failed,(uint64_t)r->retargeted,(uint64_t)r->posted,(uint64_t)r->stop_attempted,(uint64_t)r->stopped,(uint64_t)r->pending,(uint64_t)r->mac.complete,(uint64_t)r->mac_quiescent,(uint64_t)r->reads,(uint64_t)r->writes,(uint64_t)r->waits,(uint64_t)r->polls,r->elapsed_us,(uint64_t)r->events,(uint64_t)r->completion,(uint64_t)r->remaining,(uint64_t)r->actual,(uint64_t)r->during_window,(uint64_t)r->inspected,(uint64_t)r->view_status,(uint64_t)r->beacon_status,(uint64_t)r->view.consumed,(uint64_t)r->view.mpdu_size,(uint64_t)r->view.frame_control,(uint64_t)wifi_tune_channel(d));
    if(r->complete && r->inspected && r->beacon_status==MT76_RX_OK)
        console_printf("wifi-channel-rx-beacon: channel=%u interval=%u ssid_size=%u privacy=%u rsn=%u wpa=%u ds_present=%u identifiers_logged=0\n",(uint64_t)r->beacon.channel,(uint64_t)r->beacon.interval,(uint64_t)r->beacon.ssid_size,(uint64_t)r->beacon.privacy,(uint64_t)r->beacon.rsn_present,(uint64_t)r->beacon.wpa_present,(uint64_t)r->beacon.ds_present);
}
