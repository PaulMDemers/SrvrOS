static bool cal_probe_model,cal_probe_readonly;
static unsigned cal_probe_quarantine;
static uint32_t cal_model_alc,cal_model_ibi;
static uint32_t mac_model_mac,mac_model_filter,mac_model_txop;
static unsigned mac_model_calls,mac_model_cancel;
static void xhci_test_mac_publish(struct xhci_device *d,uint64_t phys) {
    unsigned i=(phys-d->ep0_ring_phys)/16;
    struct xhci_trb *setup=&d->ep0_ring[i];
    bool in=(setup->parameter_low&0x80)!=0;
    uint32_t a=setup->parameter_high&65535,v=0;
    assert((setup->parameter_low>>8&255)==(in?7:6));
    if(in){
        switch(a){case 0x1004:v=mac_model_mac;break;case 0x1400:v=mac_model_filter;break;
        case 0x1340:v=mac_model_txop;break;case 0x2000:v=0xf000f200;break;
        case 0x208:v=0x40000050;break;case 0x13b0:v=cal_probe_model?cal_model_alc:0x2f2f;break;
        case 0x2124:v=cal_probe_model?cal_model_ibi:UINT32_MAX;break;default:v=0;break;}
        for(unsigned n=0;n<4;n++)d->descriptor_buffer[n]=(uint8_t)(v>>(8*n));
    }else{
        for(unsigned n=0;n<4;n++)v|=(uint32_t)d->descriptor_buffer[n]<<(8*n);
        if(cal_probe_model && a==0x13b0){assert(v==0 || v==0x2f2f1b1b);cal_model_alc=v;}
        else if(cal_probe_model && a==0x2124){assert(v==0xffffff7e || v==UINT32_MAX);if(!cal_probe_readonly)cal_model_ibi=v;}
        else if(a==0x1004){assert(v==0 || v==4 || v==12);mac_model_mac=v;}
        else if(a==0x1400){assert(v==0x17f97);mac_model_filter=v;}
        else {assert(a==0x1340 && (v==0x10583f || v==0x583f));mac_model_txop=v;}
    }
    unsigned status=i+2;if(status>=63)status-=63;
    xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)(d->ep0_ring_phys+16*status),
        .status=1u<<24,.control=(32u<<10)|(1u<<16)|((uint32_t)d->slot_id<<24)|(xhci.event_cycle?1u:0u)};
    if(++mac_model_calls==mac_model_cancel)d->firmware.mcu.agc_state.complete=false;
    if(cal_probe_model && mac_model_calls==cal_probe_quarantine)d->ep0_quarantined=true;
}
static bool xhci_test_mac_delay(uint64_t us){test_time+=us;return true;}
static void test_wifi_mac_cycle(void) {
    for(unsigned cancel=0;cancel<=53;cancel++) {
        reset_test();test_time=0;test_time_step=100;
        struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);
        struct xhci_wifi_mcu *m=&d->firmware.mcu;wifi_mcu_select_mode(m,68);
        assert(m->post_vco_mode && m->agc_state_mode && m->rx_lifecycle_mode && m->mac_cycle_mode);
        struct xhci_trb ep0[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        d->ep0_ring=ep0;d->ep0_ring_phys=0x9000;d->ep0_cycle=true;
        d->descriptor_buffer=buffer;d->descriptor_buffer_phys=(uintptr_t)buffer;
        m->rx.configured=true;m->rx.posts=1;m->rx.pending=true;
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=0x10583f;
        mac_model_calls=0;mac_model_cancel=cancel;mac_cycle_model=true;
        bool ok=wifi_mcu_mac_cycle(d);mac_cycle_model=false;
        assert(ok==!cancel);
        assert(!mac_model_mac && mac_model_filter==0x17f97 && mac_model_txop==0x10583f && !freed);
        if(!cancel)assert(m->mac_cycle.complete && m->mac_cycle.reads==46 && m->mac_cycle.writes==7 && m->mac_cycle.ibi9==UINT32_MAX);
        unsigned calls=mac_model_calls;assert(!wifi_mcu_mac_cycle(d) && calls==mac_model_calls);
        assert(m->rx.pending && !m->rx.stopped && !m->rx.stop_attempted);
    }
    for(unsigned fault=0;fault<11;fault++) {
        reset_test();struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);
        struct xhci_wifi_mcu *m=&d->firmware.mcu;wifi_mcu_select_mode(m,68);
        struct xhci_trb ring[64]={0};uint8_t buffer[4096]={0};
        d->ep0_ring=ring;d->descriptor_buffer=buffer;d->descriptor_buffer_phys=(uintptr_t)buffer;
        m->rx.configured=true;m->rx.posts=1;
        if(fault==0)m->mac_cycle_mode=false;
        if(fault==1)m->rx.active=false;
        if(fault==2)m->rx.configured=false;
        if(fault==3)m->rx.posts=0;
        if(fault==4)m->rx.stopped=true;
        if(fault==5)m->rx.stop_attempted=true;
        if(fault==6)d->present=false;
        if(fault==7)d->slot_id=0;
        if(fault==8)d->ep0_quarantined=true;
        if(fault==9)firmware_command_failed=true;
        if(fault==10)strict_control.device=d;
        assert(!wifi_mcu_mac_cycle(d) && !d->ep0_enqueue && !m->mac_cycle.mac_touched && !freed);
        strict_control.device=NULL;
    }
    test_time_step=1000;
    puts("xhci MAC cycle: PASS actual mode68,USB setup/read/write mapping,forward cancellation with independent cleanup,retained RX,no replay");
}
