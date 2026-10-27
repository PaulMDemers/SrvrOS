/* v94 only. Polling fits inside the accepted MAC hook; final inspection is
 * deferred until the parent MAC cleanup, endpoint stop and six guards finish. */
static bool wifi_window_owned(void *context,bool cleanup) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct xhci_wifi_window *w=&m->window;struct xhci_wifi_rx *r=&m->rx;
    if(!m->window_mode || !w->state.attempted || !wifi_window_layout(d) ||
       !wifi_rx_owned(d) || !m->poweron.complete || m->poweron_ack_mask!=31 ||
       !m->mac_cycle.started || !m->mac_cycle.snapshot_complete)return false;
    const struct mt76_rx_window_plan *p=&w->state.plan;
    if(p->ring!=w->plan.ring || p->slot!=w->plan.slot || p->dci!=9)return false;
    for(unsigned i=0;i<8;i++)if(p->buffer[i]!=w->plan.buffer[i] || p->physical[i]!=w->plan.physical[i])return false;
    if(cleanup)return m->mac_cycle.complete && m->mac_cycle.mac_disabled &&
        m->mac_cycle.tx_drained && m->mac_cycle.rx_drained &&
        m->mac_cycle.filter_restored && m->mac_cycle.txop_restored &&
        r->stopped && r->stop_attempted && !r->pending && !r->failed &&
        wifi_rx_guards_valid(r,6);
    return w->polling && !w->state.polled && !w->state.failed &&
        !m->mac_cycle.complete && !r->stop_attempted && !r->stopped &&
        wifi_rx_guards_valid(r,3) && wifi_mac_cycle_owned(d,false);
}
static bool wifi_window_post(void *context,unsigned index,uint64_t trb,uint64_t physical) {
    struct xhci_device *d=context;struct xhci_wifi_window *w=&d->firmware.mcu.window;
    struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
    if(!wifi_window_owned(d,false) || index>=8 || w->state.attempts!=index+1 ||
       trb!=w->plan.ring+16*index || physical!=w->plan.physical[index])return false;
    if(index==0)return r->posts==1 && r->trb==trb &&
        ((!r->done && r->pending && !r->events) ||
         (r->done && !r->pending && r->events==1 &&
          (r->completion==1 || r->completion==13) && r->remaining<=4096));
    if(r->posts!=index || !r->done || r->pending || r->events!=1 ||
       (r->completion!=1 && r->completion!=13) || r->remaining>4096 ||
       !w->delivered || !w->state.item[index-1].completed)return false;
    uint64_t flags=event_irq_save();
    r->buffer=(uint8_t *)w->plan.buffer[index];r->buffer_phys=physical;
    r->done=false;r->pending=true;r->events=0;r->completion=0;r->remaining=0;
    w->delivered=false;r->posts++;
    struct xhci_trb td={.parameter_low=(uint32_t)physical,.parameter_high=physical>>32,
        .status=4096,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    r->trb=ring_push(r->ep.ring,r->ep.ring_phys,&r->ep.enqueue,&r->ep.cycle,&td);
    bool ok=r->trb==trb;
    if(ok)db_write32(d->slot_id,9);
    event_irq_restore(flags);
    console_printf("wifi-window-post: index=%u requested=4096 submitted=%u retained=1 reused=0\n",(uint64_t)index,(uint64_t)ok);
    return ok;
}
static bool wifi_window_poll(void *context,struct mt76_rx_window_event *out,bool *present) {
    struct xhci_device *d=context;struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
    struct xhci_wifi_window *w=&d->firmware.mcu.window;
    if(!out || !present || !wifi_window_owned(d,false))return false;
    uint64_t flags=event_irq_save();
    if(!r->done) {
        struct xhci_trb event;
        if(poll_event(&event))dispatch_transfer_event(&event);
    }
    *present=r->done && !w->delivered;
    if(*present) {
        if(r->events!=1 || r->pending || r->failed){event_irq_restore(flags);return false;}
        *out=(struct mt76_rx_window_event){.trb=r->trb,.slot=d->slot_id,.dci=9,
            .completion=r->completion,.remaining=r->remaining};
        w->delivered=true;
    }
    event_irq_restore(flags);return !r->failed;
}
static bool wifi_window_inspect(void *context,uint32_t tx,uint32_t ibi) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct xhci_wifi_window *w=&m->window;
    if(!m->window_mode || w->state.attempted || !w->prepared ||
       !wifi_poweron_inspect(d,tx,ibi) || !wifi_window_layout(d))return false;
    w->initial_early=m->rx.done;w->polling=true;
    console_write("wifi-window-progress: up to8 distinct retained IN pages for1s; decode after parent MAC cleanup and exact stop\n");
    const struct mt76_rx_window_io io={.context=d,.owned=wifi_window_owned,
        .post=wifi_window_post,.poll=wifi_window_poll,.now=wifi_mac_cycle_now};
    bool ok=mt76_rx_window_poll(&w->state,&w->plan,&io);w->polling=false;
    console_printf("wifi-window-poll: ok=%u attempts=%u completions=%u polls=%u elapsed_us=%u initial_early=%u retained=1\n",(uint64_t)ok,(uint64_t)w->state.attempts,(uint64_t)w->state.completions,(uint64_t)w->state.polls,w->state.elapsed_us,(uint64_t)w->initial_early);
    return ok;
}
static bool wifi_window_finish(struct xhci_device *d) {
    struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
    const struct mt76_rx_window_stop stop={.slot=d->slot_id,.dci=r->ep.dci,
        .state=r->stop_state,.dequeue=r->stop_dequeue};
    const struct mt76_rx_window_io io={.context=d,.owned=wifi_window_owned};
    return mt76_rx_window_finish(&d->firmware.mcu.window.state,&io,&stop);
}
static void wifi_window_log(struct xhci_device *d) {
    struct xhci_wifi_window *w=&d->firmware.mcu.window;
    struct mt76_rx_window *s=&w->state;
    console_printf("wifi-window: attempted=%u polled=%u poll_ok=%u finish_attempted=%u complete=%u failed=%u stop_attempted=%u stopped=%u attempts=%u completions=%u beacons=%u ignored=%u polls=%u elapsed_us=%u posts=%u initial_early=%u retained=1 reused=0 data_out=0 first_entry_only=1 identifiers_logged=0\n",(uint64_t)s->attempted,(uint64_t)s->polled,(uint64_t)s->poll_ok,(uint64_t)s->finish_attempted,(uint64_t)s->complete,(uint64_t)s->failed,(uint64_t)s->stop_attempted,(uint64_t)s->stopped,(uint64_t)s->attempts,(uint64_t)s->completions,(uint64_t)s->beacons,(uint64_t)s->ignored,(uint64_t)s->polls,s->elapsed_us,(uint64_t)d->firmware.mcu.rx.posts,(uint64_t)w->initial_early);
    for(unsigned i=0;i<s->attempts && i<8;i++) {
        struct mt76_rx_window_item *v=&s->item[i];
        console_printf("wifi-window-item: index=%u submitted=%u completed=%u cc=%u remaining=%u actual=%u inspected=%u view_status=%u beacon_status=%u consumed=%u mpdu=%u frame_control=%x beacon=%u during_window=%u\n",(uint64_t)i,(uint64_t)v->submitted,(uint64_t)v->completed,(uint64_t)v->completion,(uint64_t)v->remaining,(uint64_t)v->actual,(uint64_t)v->inspected,(uint64_t)v->view_status,(uint64_t)v->beacon_status,(uint64_t)v->consumed,(uint64_t)v->mpdu,(uint64_t)v->frame_control,(uint64_t)v->beacon,(uint64_t)(v->completed && (i || !w->initial_early)));
        if(v->beacon)console_printf("wifi-window-beacon: index=%u channel=1 interval=%u ssid_size=%u privacy=%u rsn=%u wpa=%u probe_response=%u ds_present=%u identifiers_logged=0\n",(uint64_t)i,(uint64_t)v->interval,(uint64_t)v->ssid_size,(uint64_t)v->privacy,(uint64_t)v->rsn,(uint64_t)v->wpa,(uint64_t)v->probe_response,(uint64_t)v->ds_present);
    }
}
