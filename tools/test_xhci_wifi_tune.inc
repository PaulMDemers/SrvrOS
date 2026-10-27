static uint8_t tune_regs[128];static uint32_t tune_reply[4];
static unsigned tune_fault,tune_at,tune_bulk_calls,tune_cases,tune_reads,tune_clear[2];
static unsigned tune_delay_fault,tune_delay_phase;
static bool tune_test_delay(uint64_t us) {
    struct xhci_wifi_tune *t=&tune_test_device->firmware.mcu.tune;
    if(tune_delay_fault && (unsigned)t->cleanup_gate==tune_delay_phase) {
        if(tune_delay_fault==1)return false;
        if(tune_delay_fault==2)return true;
        if(tune_delay_fault==3){test_time=0;return true;}
        if(tune_delay_fault==4)tune_test_device->firmware.mcu.agc_state.complete=false;
        if(tune_delay_fault==5)context_write32(tune_test_device->output_context,9,0,1);
    }
    test_time+=us;return true;
}
static void tune_test_bulk(unsigned dci) {
    struct xhci_device *d=tune_test_device;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_tune *t=&m->tune;tune_bulk_calls++;
    unsigned fault=m->scan_mode && m->scan.index!=scan_test_rf_visit?0:tune_fault;
    if(t->exchanges==tune_at && ((fault==5 && dci==16) || (fault==6 && dci==11)))return;
    if(dci==16) {
        uint32_t w[4]={0};for(unsigned i=0;i<16;i++)w[i/4]|=(uint32_t)d->firmware.buffer[i]<<(8*(i%4));
        unsigned command=(w[0]>>20)&127,seq=(w[0]>>16)&15,reg=w[1]&255;
        assert(command==10 || command==12);assert(seq==(t->exchanges-1)%15+1);
        assert(w[0]==(0x50000008u|(command<<20)|(seq<<16)) && !w[3] && (w[1]&~255u)==0x80000000u);
        assert((reg==4 || wifi_channel_index(reg)<13) && w[2]<=255 && (command==12 || !w[2]));
        if(command==12){tune_regs[reg]=(uint8_t)w[2];if(reg==4)tune_reads=0;}
        if(command==10 && reg==4 && tune_regs[4]&0x80) {
            tune_reads++;unsigned clear=tune_clear[t->cleanup_gate?1:0];
            if(clear && tune_reads>=clear)tune_regs[4]=0x30;
        }
        uint32_t h=0x4a000008u|(seq<<16);tune_reply[0]=tune_reply[3]=h;tune_reply[1]=w[1];tune_reply[2]=tune_regs[reg];
    } else {
        assert(dci==11);for(unsigned i=0;i<16;i++)m->buffer[i]=(uint8_t)(tune_reply[i/4]>>(8*(i%4)));
        if(t->exchanges==tune_at) {
            if(fault==1)m->buffer[2]^=1;
            if(fault==2)m->buffer[4]^=1;
            if(fault==3)m->buffer[9]=1;
            if(fault==7)m->agc_state.complete=false;
            if(fault==8)m->buffer[12]^=1;
            if(fault==9)m->buffer[8]^=1;
            if(fault==10)context_write32(d->output_context,9,0,1);
        }
    }
    unsigned actual=fault==4 && t->exchanges==tune_at?12:16;
    xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)m->trb,.parameter_high=m->trb>>32,
        .status=dci==16?1u<<24:(13u<<24)|(MT76_MCU_RESPONSE_MAX-actual),
        .control=(32u<<10)|(dci<<16)|((uint32_t)d->slot_id<<24)|(xhci.event_cycle?1u:0u)};
}
static void tune_model_init(void) {
    memset(tune_regs,0,sizeof tune_regs);tune_regs[4]=0x30;tune_reads=tune_bulk_calls=0;tune_clear[0]=tune_clear[1]=2;
    for(unsigned i=0;i<13;i++)tune_regs[mt76_channel_snapshot_reg(i)]=(uint8_t)mt76_channel_snapshot_initial(i);
}
static void tune_model_restored(void) {
    assert(tune_regs[4]==0x30);
    for(unsigned i=0;i<13;i++)assert(tune_regs[mt76_channel_snapshot_reg(i)]==mt76_channel_snapshot_initial(i));
}
static void test_wifi_tune(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_mcu before=*m;uint8_t context[4096];memcpy(context,d->output_context,sizeof context);
    unsigned normal=0;
    for(unsigned trial=0;trial<=normal;trial++) {
        *m=before;m->channel_mode=false;m->tune_mode=true;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        tune_model_init();tune_fault=trial?1:0;tune_at=trial;tune_test_device=d;
        bool ok=wifi_tune_capture(d);tune_test_device=NULL;tune_cases++;
        if(!trial) {
            fprintf(stderr,"tune normal: callbacks=%u exchanges=%u ownership=%u guards=%u reads=%u writes=%u polls=%u/%u delays=%u/%u\n",m->tune.callbacks,m->tune.exchanges,m->tune.owned_checks,m->tune.state.guard_reads,m->tune.state.rf_reads,m->tune.state.rf_writes,m->tune.state.target_polls,m->tune.state.cleanup_polls,m->tune.state.target_delay_us,m->tune.state.cleanup_delay_us);
            assert(ok && m->tune.state.complete && m->tune.state.cleanup_complete && m->tune.state.restored_mask==8191);
            assert(m->tune.state.steps==21 && m->tune.state.target_vco_clear && m->tune.state.cleanup_vco_clear);
            normal=m->tune.exchanges;assert(normal>100 && normal<160);wifi_tune_log(d);
        } else assert(!ok && m->tune.state.failed && !m->tune.state.complete);
        assert(!freed && !m->rx.pending && m->rx.stopped && !mac_model_mac);
        if(m->tune.state.cleanup_complete)tune_model_restored();
        unsigned calls=tune_bulk_calls;assert(!wifi_tune_capture(d) && calls==tune_bulk_calls);
    }
    for(unsigned fault=2;fault<=10;fault++)for(unsigned sample=0;sample<3;sample++) {
        *m=before;m->channel_mode=false;m->tune_mode=true;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        tune_model_init();tune_fault=fault;tune_at=(unsigned[]){1,30,normal-1}[sample];tune_test_device=d;
        bool ok=wifi_tune_capture(d);tune_test_device=NULL;tune_cases++;
        assert(!ok && m->tune.state.failed && !m->tune.state.complete && !freed);
        if(fault==5 || fault==6)assert(d->ep0_quarantined && m->failed);
        if(m->tune.state.cleanup_complete)tune_model_restored();
    }
    for(unsigned phase=0;phase<2;phase++)for(unsigned clear=0;clear<=7;clear++) {
        *m=before;m->channel_mode=false;m->tune_mode=true;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        tune_model_init();tune_fault=0;tune_at=0;tune_clear[phase]=clear;tune_test_device=d;
        bool ok=wifi_tune_capture(d);tune_test_device=NULL;tune_cases++;
        assert(ok==(clear>=1 && clear<=6));
        if(ok)tune_model_restored();else assert(m->tune.state.failed && !m->tune.state.complete);
        assert(m->tune.state.target_polls<=6 && m->tune.state.cleanup_polls<=6 && !freed);
    }
    for(unsigned fault=0;fault<21;fault++) {
        *m=before;m->channel_mode=false;m->tune_mode=true;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        tune_model_init();tune_fault=0;tune_at=0;
        switch(fault) {
        case 0:d->vendor_id^=1;break;case 1:d->product_id^=1;break;case 2:d->device_bcd^=1;break;
        case 3:m->discovery.complete=false;break;case 4:m->window.state.complete=false;break;
        case 5:m->rx.pending=true;break;case 6:m->rx.stopped=false;break;
        case 7:context_write32(d->output_context,9,0,1);break;case 8:m->rx.stop_dequeue^=1;break;
        case 9:m->rx.guards[1][0]=1;break;case 10:m->ant_fields.valid=false;break;case 11:m->trim_fields.valid=false;break;
        case 12:m->response.packet=64;break;case 13:d->firmware.ep.packet=64;break;
        case 14:m->buffer_phys^=16;break;case 15:d->ep0_quarantined=true;break;
        case 16:firmware_command_failed=true;break;case 17:m->poweron.complete=false;break;
        case 18:m->tune_mode=false;break;case 19:tune_regs[4]=0xb0;break;case 20:tune_regs[37]^=1;break;
        }
        tune_test_device=d;bool ok=wifi_tune_capture(d);tune_test_device=NULL;tune_cases++;
        assert(!ok && !freed);if(fault<19)assert(!tune_bulk_calls);else assert(!m->tune.state.touched_mask);
        d->vendor_id=0x7392;d->product_id=0xa711;d->device_bcd=0x100;
    }
    for(unsigned phase=0;phase<2;phase++)for(unsigned fault=1;fault<=5;fault++) {
        *m=before;m->channel_mode=false;m->tune_mode=true;memcpy(d->output_context,context,sizeof context);
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        tune_model_init();tune_fault=0;tune_at=0;tune_delay_fault=fault;tune_delay_phase=phase;
        uint64_t step=test_time_step;if(fault==2)test_time_step=0;
        tune_test_device=d;assert(!wifi_tune_capture(d) && m->tune.state.failed && !freed);tune_test_device=NULL;
        if(m->tune.state.cleanup_complete)tune_model_restored();
        test_time_step=step;tune_delay_fault=0;tune_cases++;
    }
    *m=before;m->channel_mode=false;m->tune_mode=true;memcpy(d->output_context,context,sizeof context);
    d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
    struct xhci_wifi_tune *t=&m->tune;*t=(struct xhci_wifi_tune){.active=true,.pinned=true};
    t->state.attempted=true;t->state.channel=6;t->state.saved_mask=8191;t->state.touched_mask=1;
    for(unsigned i=0;i<13;i++)t->state.saved[i]=(uint8_t)mt76_channel_snapshot_initial(i);
    wifi_channel_identity(d,t->identity);wifi_tune_forward(t,6);t->pos=23;
    tune_model_init();tune_fault=0;tune_at=0;tune_test_device=d;
    assert(!wifi_tune_write(d,38,2,false) && !tune_bulk_calls);
    assert(!wifi_tune_write(d,37,3,false) && !tune_bulk_calls);
    assert(!wifi_tune_write(d,37,2,true) && !tune_bulk_calls);
    assert(!wifi_tune_write(d,4,0xb0,false) && !tune_bulk_calls);
    assert(!wifi_tune_delay(d,10,false) && !tune_bulk_calls);
    assert(wifi_tune_write(d,37,2,false));uint8_t seen=0;assert(wifi_tune_read(d,37,&seen,false) && seen==2);
    unsigned calls=tune_bulk_calls;t->state.failed=true;
    assert(!wifi_tune_guard(d,0x1004,(uint32_t *)&tune_reply[0],false) && calls==tune_bulk_calls);
    tune_test_device=NULL;tune_cases+=8;
    *m=before;memcpy(d->output_context,context,sizeof context);d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
    printf("wifi-tune-native: PASS %u scenarios;all%u RF reply headers,wire/transport/live-stop/board/ownership gates,VCO waits/timeouts,independent cleanup,no DMA freed\n",tune_cases,normal);
}
