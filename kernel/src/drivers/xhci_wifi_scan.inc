/* Pilot only: each channel is a separate excursion from/restoration to channel1.
 * Prior completed RX resources stay pinned and stopped for the entire scan. */
static bool wifi_scan_history_owned(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_scan *s=&m->scan;
    if(!m->scan_mode || !s->attempted || !s->active || s->failed || s->completed>3 || s->index>=3 ||
       !wifi_mcu_owned(d) || firmware_command_failed || strict_control.device || !wifi_window_owned(d,true))return false;
    uint64_t identity[18];wifi_channel_identity(d,identity);
    for(unsigned i=0;i<s->completed;i++){
        struct xhci_wifi_tune *t=&s->visit[i].tune;struct xhci_wifi_receive *r=&s->visit[i].receive;
        if(t->active || !t->pinned || t->state.channel!=mt76_scan_channel(i) || !t->state.complete || t->state.failed ||
           !t->state.cleanup_complete || t->state.restored_mask!=8191 || t->forward_failed || t->pos!=t->count ||
           !r->complete || r->failed || !r->stopped || !r->stop_attempted || r->active || r->mac_active || r->pending ||
           !r->mac_quiescent || !r->mac.complete || !r->mac.mac_disabled || !r->mac.tx_drained || !r->mac.rx_drained ||
           !r->mac.filter_restored || !r->mac.txop_restored || !wifi_receive_layout_for(d,r) ||
           !wifi_receive_dequeue_valid(r,r->dequeue) ||
           r->guards[0]!=0 || r->guards[1]!=0xf000f200 || r->guards[2]!=0x40000050 || r->guards[3]!=0)return false;
        for(unsigned k=0;k<18;k++)if(!identity[k] || identity[k]!=t->identity[k])return false;
    }
    return true;
}
static bool wifi_scan_retained(struct xhci_device *d){
    struct xhci_wifi_scan *s=&d->firmware.mcu.scan;
    if(!wifi_scan_history_owned(d))return false;
    if(s->completed)return wifi_receive_context_for(d,&s->visit[s->completed-1].receive,true,true);
    return wifi_channel_stopped(d);
}
static bool wifi_scan_collect_owned(void *context,const struct mt76_scan_input *v,unsigned n){
    struct xhci_device *d=context;struct xhci_wifi_scan *s=&d->firmware.mcu.scan;
    if(v!=s->input || n!=3 || s->completed!=3 || !wifi_scan_retained(d))return false;
    for(unsigned i=0;i<3;i++){
        const struct xhci_wifi_receive *r=&s->visit[i].receive;
        if(v[i].buffer!=wifi_receive_data(r) || v[i].ring!=r->ep.ring_phys || v[i].physical!=wifi_receive_data_phys(r) ||
           v[i].channel!=mt76_scan_channel(i) || v[i].elapsed_us!=r->elapsed_us ||
           v[i].events!=r->events || v[i].completion!=r->completion || v[i].remaining!=r->remaining ||
           v[i].actual!=r->actual || v[i].during_window!=r->during_window)return false;
    }
    return true;
}
static bool wifi_burst_window_equal(const struct mt76_rx_window *a,const struct mt76_rx_window *b){
#define EQ(k) if(a->k!=b->k)return false
    EQ(attempted);EQ(polled);EQ(poll_ok);EQ(finish_attempted);EQ(complete);EQ(failed);EQ(stop_attempted);EQ(stopped);
    EQ(attempts);EQ(completions);EQ(ignored);EQ(polls);EQ(beacons);EQ(elapsed_us);
    EQ(plan.ring);EQ(plan.slot);EQ(plan.dci);EQ(plan.budget_us);EQ(plan.country[0]);EQ(plan.country[1]);EQ(plan.channel);EQ(plan.first_index);
    for(unsigned i=0;i<8;i++){
        EQ(plan.physical[i]);EQ(plan.buffer[i]);EQ(item[i].submitted);EQ(item[i].completed);EQ(item[i].inspected);EQ(item[i].beacon);
        EQ(item[i].completion);EQ(item[i].remaining);EQ(item[i].actual);EQ(item[i].consumed);EQ(item[i].mpdu);
        EQ(item[i].view_status);EQ(item[i].beacon_status);EQ(item[i].frame_control);EQ(item[i].interval);EQ(item[i].ssid_size);
        EQ(item[i].privacy);EQ(item[i].rsn);EQ(item[i].wpa);EQ(item[i].ds_present);EQ(item[i].probe_response);
    }
#undef EQ
    return true;
}
static bool wifi_scan_batch_owned(void *context,const struct mt76_scan_batch_visit *v,unsigned n){
    struct xhci_device *d=context;struct xhci_wifi_scan *s=&d->firmware.mcu.scan;
    if(!d->firmware.mcu.scan_batch_mode || v!=s->batch_visit || n!=3 || s->completed!=3 || !wifi_scan_retained(d))return false;
    for(unsigned i=0;i<3;i++){
        const struct xhci_wifi_receive *r=&s->visit[i].receive;
        if(!r->burst.enabled || !r->burst.state.complete || r->burst.state.failed ||
           r->burst.plan.channel!=mt76_scan_channel(i) || !mt76_scan_tune_valid(&v[i].tune,mt76_scan_channel(i)) ||
           !wifi_burst_window_equal(&v[i].window,&r->burst.state))return false;
        const uint8_t *a=(const uint8_t *)&v[i].tune,*b=(const uint8_t *)&s->visit[i].tune.state;
        for(unsigned j=0;j<sizeof v[i].tune;j++)if(a[j]!=b[j])return false;
    }
    return true;
}
static void wifi_scan_batch_log(struct xhci_device *d){
    struct xhci_wifi_scan *s=&d->firmware.mcu.scan;struct mt76_scan_batch *r=&s->batch;
    console_printf("wifi-scan-batch: attempted=%u complete=%u failed=%u visits=%u inputs=%u pending=%u empty=%u parsed=%u unrelated=%u rejected=%u rows=%u late=%u retained=1 reused=0 data_out=0 identifiers_logged=0\n",(uint64_t)r->attempted,(uint64_t)r->complete,(uint64_t)r->failed,(uint64_t)s->completed,(uint64_t)r->inputs,(uint64_t)r->pending,(uint64_t)r->empty,(uint64_t)r->parsed,(uint64_t)r->unrelated,(uint64_t)r->rejected,(uint64_t)r->count,(uint64_t)s->late);
    if(r->complete)for(unsigned i=0;i<r->count && i<24;i++){
        const struct mt76_scan_row *row=&r->row[i];const struct mt76_discovery_row *v=&row->advertisement;
        console_printf("wifi-scan-batch-row: index=%u channel=%u channels=%u observations=%u hidden=%u ssid_size=%u privacy=%u rsn=%u wpa=%u ds=%u beacon=%u probe=%u ssid_conflict=%u security_conflict=%u channel_conflict=%u identifiers_logged=0\n",(uint64_t)i,(uint64_t)v->channel,(uint64_t)row->channels,(uint64_t)v->observations,(uint64_t)v->hidden,(uint64_t)v->ssid_size,(uint64_t)v->privacy,(uint64_t)v->rsn,(uint64_t)v->wpa,(uint64_t)v->ds_present,(uint64_t)v->beacon_seen,(uint64_t)v->probe_response_seen,(uint64_t)v->ssid_conflict,(uint64_t)v->security_conflict,(uint64_t)row->channel_conflict);
    }
}
static void wifi_scan_log(struct xhci_device *d){
    if(d->firmware.mcu.scan_batch_mode){wifi_scan_batch_log(d);return;}
    struct xhci_wifi_scan *s=&d->firmware.mcu.scan;struct mt76_scan *r=&s->result;
    console_printf("wifi-scan: attempted=%u complete=%u failed=%u visits=%u inputs=%u unobserved=%u empty=%u parsed=%u unrelated=%u rejected=%u rows=%u late=%u retained=1 reused=0 data_out=0 identifiers_logged=0\n",(uint64_t)s->attempted,(uint64_t)r->complete,(uint64_t)(s->failed || r->failed),(uint64_t)s->completed,(uint64_t)r->inputs,(uint64_t)r->unobserved,(uint64_t)r->empty,(uint64_t)r->parsed,(uint64_t)r->unrelated,(uint64_t)r->rejected,(uint64_t)r->count,(uint64_t)s->late);
    if(r->complete)for(unsigned i=0;i<r->count && i<3;i++){
        const struct mt76_scan_row *v=&r->row[i];const struct mt76_discovery_row *a=&v->advertisement;
        console_printf("wifi-scan-row: index=%u channel=%u channels=%u observations=%u hidden=%u ssid_size=%u privacy=%u rsn=%u wpa=%u ds_present=%u beacon_seen=%u probe_response_seen=%u ssid_conflict=%u security_conflict=%u channel_conflict=%u identifiers_logged=0\n",(uint64_t)i,(uint64_t)a->channel,(uint64_t)v->channels,(uint64_t)a->observations,(uint64_t)a->hidden,(uint64_t)a->ssid_size,(uint64_t)a->privacy,(uint64_t)a->rsn,(uint64_t)a->wpa,(uint64_t)a->ds_present,(uint64_t)a->beacon_seen,(uint64_t)a->probe_response_seen,(uint64_t)a->ssid_conflict,(uint64_t)a->security_conflict,(uint64_t)v->channel_conflict);
    }
}
static void wifi_scan_service_publish_device(struct xhci_device *);
static bool wifi_scan_capture(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_scan *s=&m->scan;
    if(!m->scan_mode || !m->receive_mode || !m->tune_mode || s->attempted || m->tune.state.attempted || m->receive.attempted)return false;
    *s=(struct xhci_wifi_scan){.attempted=true,.active=true};bool ok=wifi_scan_retained(d);
    if(m->scan_batch_mode)console_write("wifi-scan-progress: fixed US1/6/11 20MHz; up to8 fresh enabled-window INs per visit; retained early TD and one original10s deadline; stop and restore channel1 between visits; no network data OUT\n");
    else if(m->scan_retry_mode)console_write("wifi-scan-progress: fixed US1/6/11 20MHz; fresh IN with one pre-enable completion retry per visit; stop and restore channel1 between visits; no network data OUT\n");
    else console_write("wifi-scan-progress: fixed US1/6/11 20MHz; fresh single-IN per visit; stop and restore channel1 between visits; no network data OUT\n");
    for(unsigned i=0;i<3 && ok;i++){
        if(!wifi_scan_retained(d)){ok=false;break;}
        s->index=i;m->tune=(struct xhci_wifi_tune){0};m->receive=(struct xhci_wifi_receive){0};
        console_printf("wifi-scan-visit: index=%u channel=%u begin=1\n",(uint64_t)i,(uint64_t)mt76_scan_channel(i));
        ok=wifi_tune_capture(d);wifi_tune_log(d);
        if(m->receive.complete && m->receive.inspected && m->receive.beacon_status==MT76_RX_OK)
            console_printf("wifi-scan-observation: index=%u hidden=%u identifiers_logged=0\n",(uint64_t)i,(uint64_t)m->receive.beacon.hidden);
        s->visit[i].tune=m->tune;s->visit[i].receive=m->receive;
        if(ok){
            s->completed++;
            struct xhci_wifi_receive *r=&m->receive;
            if(m->scan_batch_mode)s->batch_visit[i]=(struct mt76_scan_batch_visit){.tune=m->tune.state,.window=r->burst.state};
            s->input[i]=(struct mt76_scan_input){.channel=mt76_scan_channel(i),.tune=m->tune.state,
                .ring=r->ep.ring_phys,.physical=wifi_receive_data_phys(r),.buffer=wifi_receive_data(r),.elapsed_us=r->elapsed_us,
                .events=r->events,.completion=r->completion,.remaining=r->remaining,.actual=r->actual,
                .complete=r->complete,.stopped=r->stopped,.quiescent=r->mac_quiescent,.retained=true,.during_window=r->during_window};
            ok=wifi_scan_retained(d);
        }
        console_printf("wifi-scan-visit: index=%u channel=%u complete=%u restored=%u stopped=%u\n",(uint64_t)i,(uint64_t)mt76_scan_channel(i),(uint64_t)ok,(uint64_t)m->tune.state.cleanup_complete,(uint64_t)m->receive.stopped);
    }
    const uint8_t us[2]={'U','S'};const struct mt76_scan_io io={d,wifi_scan_collect_owned};
    if(m->scan_batch_mode){const struct mt76_scan_batch_io batch_io={d,wifi_scan_batch_owned};ok=ok && s->completed==3 && mt76_scan_batch_collect(&s->batch,s->batch_visit,3,&batch_io);}
    else ok=ok && s->completed==3 && mt76_scan_collect(&s->result,us,s->input,3,&io);
    if(ok && m->scan_batch_mode)wifi_scan_service_publish_device(d);
    s->failed=!ok;s->active=false;
    if(!ok){
        /* Raw DMA must remain retained. Erase only the copied advertisements. */
        s->result=(struct mt76_scan){.attempted=true,.failed=true};
        if(m->scan_batch_mode)s->batch=(struct mt76_scan_batch){.attempted=true,.failed=true};
        m->receive.beacon=(struct mt76_rx_beacon){0};
        for(unsigned i=0;i<3;i++)s->visit[i].receive.beacon=(struct mt76_rx_beacon){0};
    }
    return ok;
}

/* Cache validity only: one root PORTSC read, no USB/RF command or poll loop.
 * Latched connection change retires the owner even if it is connected again.
 * Hub children need an independent live child-port observer before admission. */
static bool wifi_scan_service_present(void *context){
    const struct xhci_device *d=context;
    if(!xhci.operational || !d->present || !d->configured || !d->wifi_probe_ok || d->ep0_quarantined ||
       d->parent_slot_id || !d->port_id || d->port_id>xhci.port_count ||
       !d->firmware.mcu.scan_batch_mode || d->firmware.mcu.failed || d->firmware.mcu.scan.failed ||
       !d->firmware.mcu.scan.batch.complete)return false;
    uint32_t state=op_read32(port_offset(d->port_id-1));
    return state!=UINT32_MAX && (state&(XHCI_PORTSC_CCS|XHCI_PORTSC_PED))==(XHCI_PORTSC_CCS|XHCI_PORTSC_PED) &&
        !(state&(XHCI_PORTSC_CSC|XHCI_PORTSC_PR));
}
static bool wifi_scan_service_owned(void *context,const struct mt76_scan_batch *batch){
    struct xhci_device *d=context;
    return batch==&d->firmware.mcu.scan.batch && d->firmware.mcu.scan.active &&
        wifi_scan_batch_owned(d,d->firmware.mcu.scan.batch_visit,3);
}
static void wifi_scan_service_publish_device(struct xhci_device *d){
    if(!wifi_scan_service_present(d))return;
    const struct wifi_service_provider ops={d,"Edimax USB Wi-Fi",wifi_scan_service_present};
    const struct mt76_selection_io io={d,wifi_scan_service_owned};
    uint64_t flags=event_irq_save();
    (void)wifi_service_publish(&ops,&d->firmware.mcu.scan.batch,&io);
    event_irq_restore(flags);
}
