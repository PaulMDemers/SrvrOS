/* Opt-in repeated receive adapter. The first preposted TD/page is immutable;
 * an observed pre-enable completion can be skipped, never relabelled/parsed. */
static bool wifi_burst_owned(void *context,bool cleanup){
    struct xhci_device *d=context;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;struct xhci_wifi_burst *b=&r->burst;
    if(!b->enabled || !b->prepared || !b->state.attempted || !wifi_receive_layout(d) ||
       !wifi_receive_mac_owned(d,cleanup) || b->plan.channel!=wifi_tune_channel(d))return false;
    if(cleanup)return r->stopped && r->stop_attempted && !r->pending && r->mac_quiescent &&
        r->mac.complete && r->mac.mac_disabled && r->mac.tx_drained && r->mac.rx_drained &&
        r->mac.filter_restored && r->mac.txop_restored;
    return r->polling && !r->stop_attempted && !b->state.polled && !b->state.failed && r->mac.started && r->mac.snapshot_complete;
}
static bool wifi_burst_post(void *context,unsigned index,uint64_t trb,uint64_t physical){
    struct xhci_device *d=context;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;struct xhci_wifi_burst *b=&r->burst;
    unsigned td=b->first+index;
    if(!wifi_burst_owned(d,false) || index>=8 || b->state.attempts!=index+1 || td>=9 ||
       trb!=r->ep.ring_phys+16*td || physical!=b->physical[td])return false;
    if(!td){
        bool ok=b->posts==1 && r->trb==trb && !b->delivered &&
            ((!r->done && r->pending && !r->events) || (r->done && !r->pending && r->events==1 &&
             r->during_window && (r->completion==1 || r->completion==13) && r->remaining<=4096));
        console_printf("wifi-burst-post: index=0 td=0 adopted=1 submitted=%u ring=%x buffer=%x trb=%x retained=1 reused=0\n",(uint64_t)ok,r->ep.ring_phys,physical,trb);
        return ok;
    }
    if(b->posts!=td || !r->done || r->pending || r->events!=1 ||
       (r->completion!=1 && r->completion!=13) || r->remaining>4096 ||
       (index?(!b->delivered || !b->state.item[index-1].completed):(!b->first || r->during_window)))return false;
    uint64_t flags=event_irq_save();
    r->pending=true;r->done=false;r->events=r->completion=r->remaining=0;r->during_window=false;b->delivered=false;b->posts++;
    struct xhci_trb v={.parameter_low=(uint32_t)physical,.parameter_high=physical>>32,.status=4096,
        .control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    r->trb=ring_push(r->ep.ring,r->ep.ring_phys,&r->ep.enqueue,&r->ep.cycle,&v);
    bool ok=r->trb==trb && wifi_burst_owned(d,false);
    if(ok)db_write32(d->slot_id,9);
    event_irq_restore(flags);
    console_printf("wifi-burst-post: index=%u td=%u adopted=0 submitted=%u ring=%x buffer=%x trb=%x retained=1 reused=0\n",(uint64_t)index,(uint64_t)td,(uint64_t)ok,r->ep.ring_phys,physical,trb);
    if(!ok)r->failed=true;
    return ok;
}
static bool wifi_burst_event_poll(void *context,struct mt76_rx_window_event *out,bool *present){
    struct xhci_device *d=context;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;struct xhci_wifi_burst *b=&r->burst;
    if(!out || !present || !wifi_burst_owned(d,false))return false;
    uint64_t flags=event_irq_save();
    if(!r->done){struct xhci_trb e;if(poll_event(&e))dispatch_transfer_event(&e);}
    *present=r->done && !b->delivered;
    bool ok=!r->failed;
    if(*present){
        ok=ok && r->events==1 && !r->pending && r->during_window;
        if(ok){*out=(struct mt76_rx_window_event){.trb=r->trb,.slot=d->slot_id,.dci=9,.completion=r->completion,.remaining=r->remaining};b->delivered=true;}
    }
    event_irq_restore(flags);
    if(ok && *present)console_printf("wifi-burst-event: index=%u td=%u cc=%u remaining=%u actual=%u during_window=1 inspected=0\n",(uint64_t)(b->state.attempts-1),(uint64_t)(b->first+b->state.attempts-1),(uint64_t)out->completion,(uint64_t)out->remaining,(uint64_t)(4096-out->remaining));
    return ok;
}
static bool wifi_burst_poll(struct xhci_device *d){
    struct xhci_wifi_receive *r=&d->firmware.mcu.receive;struct xhci_wifi_burst *b=&r->burst;
    if(!b->enabled || b->prepared || b->state.attempted || !wifi_receive_layout(d) || !wifi_receive_mac_owned(d,false))return false;
    r->polling=true;
    if(r->done){
        if(r->pending || r->during_window || r->events!=1 || (r->completion!=1 && r->completion!=13) || r->remaining>4096){r->polling=false;return false;}
        b->first=1;b->early_events=r->events;b->early_completion=r->completion;b->early_remaining=r->remaining;
    }
    b->plan=(struct mt76_rx_window_plan){.ring=r->ep.ring_phys,.slot=d->slot_id,.dci=9,
        .country={'U','S'},.channel=(uint8_t)wifi_tune_channel(d),.budget_us=10000000,.first_index=b->first};
    for(unsigned i=0;i<8;i++){b->plan.physical[i]=b->physical[b->first+i];b->plan.buffer[i]=b->buffer[b->first+i];}
    b->prepared=true;
    console_printf("wifi-burst-begin: channel=%u first=%u early_events=%u early_cc=%u early_remaining=%u inspected=0 budget_us=10000000 pages=9 retained=1 reused=0 data_out=0\n",(uint64_t)b->plan.channel,(uint64_t)b->first,(uint64_t)b->early_events,(uint64_t)b->early_completion,(uint64_t)b->early_remaining);
    const struct mt76_rx_window_io io={.context=d,.owned=wifi_burst_owned,.post=wifi_burst_post,.poll=wifi_burst_event_poll,.now=wifi_mac_cycle_now};
    bool ok=mt76_rx_window_poll(&b->state,&b->plan,&io);
    r->polls=b->state.polls;r->elapsed_us=b->state.elapsed_us;r->polling=false;if(!ok)r->failed=true;
    console_printf("wifi-burst-poll: ok=%u attempts=%u completions=%u polls=%u elapsed_us=%u first=%u deadline_reset=0\n",(uint64_t)ok,(uint64_t)b->state.attempts,(uint64_t)b->state.completions,(uint64_t)b->state.polls,b->state.elapsed_us,(uint64_t)b->first);
    return ok;
}
static void wifi_burst_log(struct xhci_device *d){
    struct xhci_wifi_burst *b=&d->firmware.mcu.receive.burst;struct mt76_rx_window *s=&b->state;
    console_printf("wifi-burst: attempted=%u complete=%u failed=%u first=%u posts=%u attempts=%u completions=%u beacons=%u ignored=%u late=%u polls=%u elapsed_us=%u stopped=%u retained=1 reused=0 deadline_reset=0 identifiers_logged=0\n",(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->failed,(uint64_t)b->first,(uint64_t)b->posts,(uint64_t)s->attempts,(uint64_t)s->completions,(uint64_t)s->beacons,(uint64_t)s->ignored,(uint64_t)b->late,(uint64_t)s->polls,s->elapsed_us,(uint64_t)s->stopped);
    for(unsigned i=0;i<9;i++)console_printf("wifi-burst-page: index=%u physical=%x retained=1 reused=0\n",(uint64_t)i,b->physical[i]);
    for(unsigned i=0;i<s->attempts && i<8;i++){
        const struct mt76_rx_window_item *v=&s->item[i];
        console_printf("wifi-burst-item: index=%u td=%u submitted=%u completed=%u cc=%u remaining=%u actual=%u inspected=%u view=%u beacon_status=%u consumed=%u mpdu=%u frame_control=%x beacon=%u interval=%u ssid_size=%u privacy=%u rsn=%u wpa=%u ds=%u probe=%u identifiers_logged=0\n",(uint64_t)i,(uint64_t)(b->first+i),(uint64_t)v->submitted,(uint64_t)v->completed,(uint64_t)v->completion,(uint64_t)v->remaining,(uint64_t)v->actual,(uint64_t)v->inspected,(uint64_t)v->view_status,(uint64_t)v->beacon_status,(uint64_t)v->consumed,(uint64_t)v->mpdu,(uint64_t)v->frame_control,(uint64_t)v->beacon,(uint64_t)v->interval,(uint64_t)v->ssid_size,(uint64_t)v->privacy,(uint64_t)v->rsn,(uint64_t)v->wpa,(uint64_t)v->ds_present,(uint64_t)v->probe_response);
    }
}
