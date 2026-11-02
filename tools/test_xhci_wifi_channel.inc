static uint8_t channel_model_regs[128];
static uint32_t channel_reply[4];
static unsigned channel_fault,channel_fault_at,channel_bulk_calls,channel_native_cases;
static void channel_test_bulk(unsigned dci) {
    struct xhci_device *d=channel_test_device;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct xhci_wifi_channel *c=&m->channel;channel_bulk_calls++;
    if((channel_fault==5 && dci==16) || (channel_fault==6 && dci==11))if(c->exchanges==channel_fault_at)return;
    if(dci==16) {
        uint32_t w[4]={0};for(unsigned n=0;n<16;n++)w[n/4]|=(uint32_t)d->firmware.buffer[n]<<(8*(n%4));
        unsigned command=(w[0]>>20)&0x7f,seq=(w[0]>>16)&15,reg=w[1]&255;
        assert(command==10 || command==12);assert(w[0]==(0x50000008u|(command<<20)|(seq<<16)));
        assert(seq==(c->exchanges-1)%15+1 && !w[3] && (w[1]&~255u)==0x80000000u);
        assert(wifi_channel_index(reg)<13 && w[2]<=255 && (command==12 || !w[2]));
        if(command==12)channel_model_regs[reg]=(uint8_t)w[2];
        uint32_t h=0x4a000008u|(seq<<16);
        channel_reply[0]=channel_reply[3]=h;channel_reply[1]=w[1];channel_reply[2]=channel_model_regs[reg];
    } else {
        assert(dci==11);
        for(unsigned n=0;n<16;n++)m->buffer[n]=(uint8_t)(channel_reply[n/4]>>(8*(n%4)));
        if(c->exchanges==channel_fault_at) {
            if(channel_fault==1)m->buffer[2]^=1;
            if(channel_fault==2)m->buffer[4]^=1;
            if(channel_fault==3)m->buffer[9]=1;
            if(channel_fault==8)m->buffer[12]^=1;
            if(channel_fault==7)m->agc_state.complete=false;
            if(channel_fault==9)m->buffer[8]^=1;
            if(channel_fault==10)context_write32(d->output_context,9,0,1);
        }
    }
    unsigned actual=channel_fault==4 && c->exchanges==channel_fault_at?12:16;
    xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)m->trb,.parameter_high=m->trb>>32,
        .status=dci==16?1u<<24:(13u<<24)|(MT76_MCU_RESPONSE_MAX-actual),
        .control=(32u<<10)|(dci<<16)|((uint32_t)d->slot_id<<24)|(xhci.event_cycle?1u:0u)};
}
static void channel_model_init(void) {
    memset(channel_model_regs,0,sizeof channel_model_regs);
    for(unsigned i=0;i<13;i++)channel_model_regs[mt76_channel_snapshot_reg(i)]=(uint8_t)mt76_channel_snapshot_initial(i);
    channel_bulk_calls=0;
}
static void channel_model_restored(void) {
    for(unsigned i=0;i<13;i++)assert(channel_model_regs[mt76_channel_snapshot_reg(i)]==mt76_channel_snapshot_initial(i));
}
static void test_wifi_channel(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_mcu before=*m;
    uint8_t context[4096];memcpy(context,d->output_context,sizeof context);
    /* Every RF exchange header mutation, including all26 cleanup responses.
     * Full prior/window/discovery fixture is kept; the actual adapter executes
     * all guards, encoding, real dispatch, independent restore and ring wraps. */
    for(unsigned trial=0;trial<=732;trial++) {
        *m=before;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        channel_fault=trial?1:0;channel_fault_at=trial;channel_model_init();channel_test_device=d;
        bool ok=wifi_channel_capture(d);channel_test_device=NULL;channel_native_cases++;
        assert(ok==!trial && !freed && !m->rx.pending && m->rx.stopped && !mac_model_mac);
        if(trial){assert(m->channel.state.failed && !m->channel.state.complete);}
        else {
            assert(m->channel.state.complete && m->channel.state.cleanup_complete && m->channel.state.channels_completed==11);
            assert(m->channel.state.guard_reads==1860 && m->channel.state.rf_reads==488 && m->channel.state.rf_writes==244);
            assert(m->channel.callbacks==2592 && m->channel.owned_checks==5185 && m->channel.exchanges==732 && m->channel.state.restored_mask==8191);
            wifi_channel_log(d);
        }
        channel_model_restored();
        unsigned calls=channel_bulk_calls;assert(!wifi_channel_capture(d) && channel_bulk_calls==calls);
    }
    for(unsigned fault=2;fault<=10;fault++)for(unsigned n=0;n<3;n++) {
        *m=before;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        channel_fault=fault;channel_fault_at=(unsigned[]){1,30,710}[n];channel_model_init();channel_test_device=d;
        bool ok=wifi_channel_capture(d);channel_test_device=NULL;channel_native_cases++;
        if(ok)fprintf(stderr,"unexpected channel preview pass fault=%u at=%u\n",fault,channel_fault_at);
        assert(!ok && m->channel.state.failed && !m->channel.state.complete && !freed && m->rx.stopped);
        if(fault==5 || fault==6)assert(d->ep0_quarantined && m->failed);
        if(fault!=5 && fault!=6 && fault!=10)channel_model_restored();
    }
    for(unsigned fault=0;fault<20;fault++) {
        *m=before;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        channel_fault=0;channel_fault_at=0;channel_model_init();
        switch(fault) {
        case 0:d->vendor_id^=1;break;case 1:d->product_id^=1;break;case 2:d->device_bcd^=1;break;
        case 3:m->discovery.complete=false;break;case 4:m->window.state.complete=false;break;
        case 5:m->rx.pending=true;break;case 6:m->rx.stopped=false;break;
        case 7:context_write32(d->output_context,9,0,1);break;
        case 8:context_write32(d->output_context,9,2,(uint32_t)(m->rx.trb+32)|1);break;
        case 9:m->rx.stop_dequeue^=1;break;case 10:m->rx.guards[1][0]=1;break;
        case 11:m->ant_fields.valid=false;break;case 12:m->trim_fields.valid=false;break;
        case 13:m->response.packet=64;break;case 14:d->firmware.ep.packet=64;break;
        case 15:m->buffer_phys^=16;break;case 16:d->ep0_quarantined=true;break;
        case 17:firmware_command_failed=true;break;case 18:m->poweron.complete=false;break;
        case 19:m->channel_mode=false;break;
        }
        channel_test_device=d;bool gate_ok=wifi_channel_capture(d);
        if(gate_ok || channel_bulk_calls)fprintf(stderr,"bad channel admission fault=%u ok=%u bulk=%u\n",fault,gate_ok,channel_bulk_calls);
        assert(!gate_ok && !channel_bulk_calls && !freed);channel_test_device=NULL;channel_native_cases++;
        d->vendor_id=0x7392;d->product_id=0xa711;d->device_bcd=0x100;
    }
    *m=before;memcpy(d->output_context,context,sizeof context);
    d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
    /* Exercise callback refusal directly: the engine's first forward write is
     * exactly bank0/R37=2, after its snapshot and leading four guards. */
    struct xhci_wifi_channel *c=&m->channel;
    *c=(struct xhci_wifi_channel){.active=true,.pinned=true,.forward=22};
    c->state.attempted=true;c->state.plan=(struct mt76_channel_preview_plan){.country={'U','S'},.bandwidth=20,.count=11};
    for(unsigned i=0;i<11;i++)c->state.plan.channel[i]=(uint8_t)(i+1);
    c->state.saved_mask=8191;c->state.touched_mask=1;
    for(unsigned i=0;i<13;i++)c->state.saved[i]=(uint8_t)mt76_channel_snapshot_initial(i);
    wifi_channel_identity(d,c->identity);channel_fault=0;channel_model_init();channel_test_device=d;
    assert(!wifi_channel_write(d,38,2,false) && !channel_bulk_calls);
    assert(!wifi_channel_write(d,37,255,false) && !channel_bulk_calls);
    assert(!wifi_channel_write(d,37,2,true) && !channel_bulk_calls);
    assert(wifi_channel_write(d,37,2,false));uint8_t seen=0;assert(wifi_channel_read(d,37,&seen,false) && seen==2);
    unsigned bulk=channel_bulk_calls;c->state.failed=true;
    assert(!wifi_channel_read(d,37,&seen,false) && channel_bulk_calls==bulk);
    channel_test_device=NULL;channel_native_cases+=6;
    *m=before;memcpy(d->output_context,context,sizeof context);
    d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
    printf("wifi-channel-native: PASS %u scenarios;all732 RF reply mutations,transport/length/address/value/ownership/stop/board gates,independent cleanup,exact callbacks,no DMA freed\n",channel_native_cases);
}
