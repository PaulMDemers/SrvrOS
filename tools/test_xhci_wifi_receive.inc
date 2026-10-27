static unsigned receive_fault,receive_ticks,receive_commands,receive_cases,receive_mac_at;
static unsigned receive_controls,receive_control_fault,receive_control_at;
static void receive_test_allocate(void) {
    struct xhci_wifi_receive *r=&receive_test_device->firmware.mcu.receive;
    if(receive_test_device->firmware.mcu.scan_mode)receive_fault=receive_test_device->firmware.mcu.scan.index==scan_test_fault_visit?scan_test_fault_kind:0;
    if(scan_test_history && scan_test_history<=3 && receive_test_device->firmware.mcu.scan_mode && receive_test_device->firmware.mcu.scan.index==1){
        struct xhci_wifi_scan *s=&receive_test_device->firmware.mcu.scan;
        if(scan_test_history==1)s->visit[0].tune.identity[0]=0;
        if(scan_test_history==2)s->visit[0].receive.identity[1]=0;
        if(scan_test_history==3)s->visit[0].receive.mac_quiescent=false;
    }
    if(receive_fault==26 && r->ep.ring && !r->buffer_phys)receive_test_device->firmware.mcu.agc_state.complete=false;
}
static void receive_test_control(void) {
    struct xhci_device *d=receive_test_device;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    if(!r->mac_active || !r->active)return;
    if(scan_test_history>=4 && d->firmware.mcu.scan_mode && d->firmware.mcu.scan.index==1){
        struct xhci_wifi_scan *s=&d->firmware.mcu.scan;
        if(scan_test_history==4)s->visit[0].tune.identity[0]=0;
        if(scan_test_history==5)s->visit[0].receive.identity[1]=0;
        if(scan_test_history==6)s->visit[0].receive.mac_quiescent=false;
    }
    if(++receive_controls!=receive_control_at)return;
    if(receive_control_fault==1)d->firmware.mcu.agc_state.complete=false;
    if(receive_control_fault==2)d->ep0_quarantined=true;
    if(receive_control_fault==3)context_write32(d->output_context,9,2,(uint32_t)(r->ep.ring_phys+32)|1);
}
static void receive_test_command(void) {
    struct xhci_device *d=receive_test_device;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    unsigned index=xhci.command_enqueue?xhci.command_enqueue-1:62;
    struct xhci_trb *cmd=&xhci.command_ring[index];unsigned type=(cmd->control>>10)&63;
    assert(type==16 || type==15);receive_commands++;
    if(d->firmware.mcu.scan_mode && type==16){
        receive_ticks=0;receive_controls=0;
        receive_fault=d->firmware.mcu.scan.index==scan_test_fault_visit?scan_test_fault_kind:0;
    }
    assert((cmd->control>>24)==d->slot_id && ((cmd->control>>16)&31)==9);
    if((receive_fault==1 && type==16) || (receive_fault==2 && type==15))return;
    uint64_t deq=r->ep.ring_phys|1;
    if(type==16)assert((cmd->parameter_low|((uint64_t)cmd->parameter_high<<32))==deq && wifi_receive_base_stopped(d));
    if(type==15){assert(!cmd->parameter_low && !cmd->parameter_high);deq+=16;}
    if(receive_fault==17 && type==15)deq+=16;
    if(receive_fault==18 && type==16)deq&=~1ull;
    context_write32(d->output_context,9,0,((receive_fault==3 && type==15) || (receive_fault==27 && type==16))?1:3);
    context_write32(d->output_context,9,2,(uint32_t)deq);context_write32(d->output_context,9,3,deq>>32);
    if(receive_fault==25 && type==16)d->firmware.mcu.agc_state.complete=false;
    uint64_t ptr=xhci.command_ring_phys+16*index;
    if((receive_fault==14 && type==16) || (receive_fault==15 && type==15))ptr+=16;
    xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)ptr,.parameter_high=ptr>>32,
        .status=(receive_fault==4?19u:1u)<<24,.control=(33u<<10)|((uint32_t)(receive_fault==16?2:d->slot_id)<<24)|(xhci.event_cycle?1u:0u)};
}
static void receive_test_tick(void) {
    struct xhci_device *d=receive_test_device;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;
    if(receive_mac_at && mac_model_calls>=receive_mac_at)d->firmware.mcu.agc_state.complete=false;
    if((!r->polling && !(receive_fault==24 && r->posted)) || !r->pending)return;
    receive_ticks++;
    if(receive_fault==5)return;
    if(receive_fault==6){test_time=0;return;}
    if(receive_fault==7){test_time_step=0;return;}
    if(receive_ticks<5 && receive_fault!=24)return;
    unsigned actual=passive_wire(r->buffer);r->buffer[82]=receive_fault==8?1:(uint8_t)wifi_tune_channel(d);
    unsigned cc=receive_fault==9?27:13,residue=receive_fault==10?4097:4096-actual;
    if(receive_fault==11){actual=0;residue=4096;}
    if(receive_fault==21){cc=1;actual=4096;residue=0;}
    if(receive_fault==22)passive_put16(r->buffer,0xfffc);
    if(receive_fault==23)passive_put16(r->buffer+36,0x88);
    struct xhci_trb event={.parameter_low=(uint32_t)r->trb,.parameter_high=r->trb>>32,.status=(cc<<24)|residue,
        .control=(32u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
    if(receive_fault==12 && receive_ticks==5){event.parameter_low=(uint32_t)d->firmware.mcu.rx.trb;event.parameter_high=d->firmware.mcu.rx.trb>>32;}
    if(receive_fault==13 && receive_ticks==5)event.control|=4;
    if(receive_fault==19 && receive_ticks==5)event.control^=3u<<24;
    if(receive_fault==20 && receive_ticks==5)event.parameter_low+=32;
    dispatch_transfer_event(&event);
}
static void test_wifi_receive(struct xhci_device *d) {
    uint8_t input[4096]={0};void *old_input=d->input_context;uint64_t old_input_phys=d->input_context_phys;
    d->input_context=input;d->input_context_phys=(uintptr_t)input;
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_mcu before=*m;uint8_t context[4096];memcpy(context,d->output_context,sizeof context);
    for(unsigned trial=0;trial<28;trial++) {
        *m=before;m->channel_mode=false;m->tune_mode=m->receive_mode=true;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=before.mac_cycle.txop;
        tune_model_init();tune_fault=0;tune_at=0;tune_delay_fault=0;
        receive_fault=trial;receive_controls=receive_control_at=receive_control_fault=0;receive_ticks=receive_commands=receive_mac_at=0;uint64_t step=test_time_step;
        receive_test_device=tune_test_device=d;bool ok=wifi_tune_capture(d);receive_test_device=tune_test_device=NULL;test_time_step=step;receive_cases++;
        bool expected=trial==0 || trial==5 || trial==8 || trial==11 || trial==12 || (trial>=19 && trial<=24);
        if(ok!=expected)fprintf(stderr,"receive trial=%u ok=%u expected=%u retarget=%u posted=%u mac=%u stopped=%u quiet=%u failed=%u callbacks=%u pos=%u count=%u\n",trial,ok,expected,m->receive.retargeted,m->receive.posted,m->receive.mac.complete,m->receive.stopped,m->receive.mac_quiescent,m->receive.failed,m->tune.callbacks,m->tune.pos,m->tune.count);
        assert(ok==expected && !freed && m->rx.stopped && !m->rx.pending);
        if(ok) {
            assert(m->tune.state.observe_attempted && m->tune.state.observe_complete && m->receive.complete && m->receive.stopped && m->receive.mac_quiescent);
            assert(m->receive.ep.ring_phys!=m->rx.ep.ring_phys && m->receive.buffer_phys!=m->rx.buffer_phys && m->receive.ep.enqueue==1);
            assert(m->receive.mac.operations==54 && m->receive.mac.reads==46 && m->receive.mac.writes==7 && m->receive.mac.waits==1);
            if(trial==0 || trial==12 || trial==19 || trial==20 || trial==21)assert(m->receive.inspected && m->receive.beacon_status==MT76_RX_OK && m->receive.beacon.channel==6);
            if(trial==8)assert(m->receive.beacon_status==MT76_RX_CHANNEL_MISMATCH);
            if(trial==22)assert(m->receive.view_status==MT76_RX_INVALID && m->receive.beacon_status==255);
            if(trial==23)assert(m->receive.view_status==MT76_RX_UNSUPPORTED && m->receive.beacon_status!=MT76_RX_OK);
            if(trial==24)assert(!m->receive.during_window && !m->receive.inspected && m->receive.actual==84);
            if(trial==5)assert(!m->receive.inspected && m->receive.elapsed_us>=10000000);
            if(trial==0)fprintf(stderr,"receive normal: callbacks=%u exchanges=%u ownership=%u controls=%u mac_ops=%u polls=%u elapsed=%llu actual=%u view=%u beacon=%u\n",m->tune.callbacks,m->tune.exchanges,m->tune.owned_checks,receive_controls,m->receive.mac.operations,m->receive.polls,(unsigned long long)m->receive.elapsed_us,m->receive.actual,m->receive.view_status,m->receive.beacon_status);
            tune_model_restored();wifi_tune_log(d);
        } else assert(m->tune.state.failed && !m->tune.state.complete);
        if(m->tune.state.cleanup_complete)tune_model_restored();
        if(trial==1 || trial==2 || trial==4 || trial==14 || trial==15 || trial==16)assert(firmware_command_failed && !m->tune.state.cleanup_complete);
        if(trial==3 || trial==17)assert(!m->tune.state.cleanup_complete && !m->receive.stopped);
        if(m->receive.posted)assert(m->receive.stop_attempted);
        if(trial==25 || trial==26)assert(!m->receive.posted && m->tune.state.cleanup_complete);
        if(trial==26)assert(!m->receive.retarget_attempted);
        if(trial==18 || trial==27)assert(!m->receive.posted && m->receive.stopped && m->tune.state.cleanup_complete);
        unsigned calls=tune_bulk_calls;assert(!wifi_tune_capture(d) && calls==tune_bulk_calls);
    }
    for(unsigned fault=1;fault<=3;fault++)for(unsigned boundary=1;boundary<=57;boundary++) {
        *m=before;m->channel_mode=false;m->tune_mode=m->receive_mode=true;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=before.mac_cycle.txop;
        tune_model_init();tune_fault=0;tune_at=0;tune_delay_fault=0;receive_fault=0;receive_ticks=receive_commands=receive_mac_at=0;
        receive_controls=0;receive_control_at=boundary;receive_control_fault=fault;
        receive_test_device=tune_test_device=d;bool ok=wifi_tune_capture(d);receive_test_device=tune_test_device=NULL;receive_cases++;
        if(ok || !m->receive.stopped)fprintf(stderr,"control fault=%u boundary=%u ok=%u controls=%u posted=%u stop=%u stopped=%u quarantine=%u cmdfail=%u failed=%u\n",fault,boundary,ok,receive_controls,m->receive.posted,m->receive.stop_attempted,m->receive.stopped,d->ep0_quarantined,firmware_command_failed,m->tune.state.failed);
        assert(!ok && m->tune.state.failed && !freed && m->receive.stop_attempted && m->receive.stopped);
        if(fault==1){assert(m->tune.state.cleanup_complete);tune_model_restored();}
        if(fault==2)assert(d->ep0_quarantined && !m->tune.state.cleanup_complete);
        if(m->tune.state.cleanup_complete)tune_model_restored();
    }
    receive_controls=receive_control_at=receive_control_fault=0;
    for(unsigned header=1;header<=152;header++) {
        *m=before;m->channel_mode=false;m->tune_mode=m->receive_mode=true;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=before.mac_cycle.txop;
        tune_model_init();tune_fault=1;tune_at=header;receive_fault=0;receive_ticks=receive_commands=receive_mac_at=0;
        receive_controls=receive_control_at=receive_control_fault=0;
        receive_test_device=tune_test_device=d;assert(!wifi_tune_capture(d) && m->tune.state.failed && !freed);receive_test_device=tune_test_device=NULL;receive_cases++;
        if(m->tune.state.cleanup_complete)tune_model_restored();
    }
    for(unsigned admission=0;admission<3;admission++) {
        *m=before;m->channel_mode=false;m->tune_mode=m->receive_mode=true;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=before.mac_cycle.txop;
        tune_model_init();tune_fault=0;tune_at=0;receive_fault=0;receive_ticks=receive_commands=receive_mac_at=0;
        receive_controls=receive_control_at=receive_control_fault=0;
        struct xhci_trb *ring=xhci.command_ring;uint64_t phys=xhci.command_ring_phys;
        if(admission==0)d->input_context=NULL;
        if(admission==1)xhci.command_ring=NULL;
        if(admission==2)xhci.command_ring_phys=0;
        receive_test_device=tune_test_device=d;assert(!wifi_tune_capture(d) && !m->receive.retarget_attempted && !receive_commands && !freed);receive_test_device=tune_test_device=NULL;receive_cases++;
        assert(m->tune.state.cleanup_complete);tune_model_restored();
        d->input_context=input;xhci.command_ring=ring;xhci.command_ring_phys=phys;
    }
    for(unsigned allocation=0;allocation<2;allocation++) {
        *m=before;m->channel_mode=false;m->tune_mode=m->receive_mode=true;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=before.mac_cycle.txop;
        tune_model_init();tune_fault=0;tune_at=0;receive_fault=0;receive_ticks=receive_commands=receive_mac_at=0;fail_after=pool_used+allocation;
        receive_test_device=tune_test_device=d;assert(!wifi_tune_capture(d));receive_test_device=tune_test_device=NULL;fail_after=0;receive_cases++;
        assert(!m->receive.retarget_attempted && !m->receive.posted && !freed && m->tune.state.cleanup_complete);tune_model_restored();
    }
    *m=before;memcpy(d->output_context,context,sizeof context);d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
    d->input_context=old_input;d->input_context_phys=old_input_phys;
    printf("wifi-channel-receive-native: PASS %u fresh-ring/page,single-IN,channel6/beacon,quiet/mismatch/foreign/malformed,clock/command/stop/allocation/restore scenarios;no DMA freed\n",receive_cases);
}
