static void test_wifi_cal_probe(void) {
    for(unsigned readonly=0;readonly<2;readonly++)for(unsigned fault=0;fault<72;fault++) {
        reset_test();test_time=0;test_time_step=100;
        struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);
        struct xhci_wifi_mcu *m=&d->firmware.mcu;wifi_mcu_select_mode(m,69);
        assert(m->post_vco_mode && m->agc_state_mode && m->rx_lifecycle_mode && m->mac_cycle_mode && m->cal_probe_mode);
        struct xhci_trb ep0[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        d->ep0_ring=ep0;d->ep0_ring_phys=0x9000;d->ep0_cycle=true;
        d->descriptor_buffer=buffer;d->descriptor_buffer_phys=(uintptr_t)buffer;
        m->rx.configured=true;m->rx.posts=1;m->rx.pending=true;
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=0x583f;
        cal_model_alc=0x2f2f1b1b;cal_model_ibi=UINT32_MAX;
        mac_model_calls=0;mac_model_cancel=fault<=69?fault:0;
        cal_probe_quarantine=fault==70?21:fault==71?24:0;
        mac_cycle_model=cal_probe_model=true;cal_probe_readonly=readonly;
        bool ok=wifi_mcu_mac_cycle(d);mac_cycle_model=cal_probe_model=false;
        assert(ok==!fault && !freed && m->rx.pending && !m->rx.stop_attempted);
        if(fault<=69)assert(!mac_model_mac && cal_model_alc==0x2f2f1b1b && cal_model_ibi==UINT32_MAX);
        else assert(d->ep0_quarantined && !m->mac_cycle.complete && !m->mac_cycle.mac_disabled);
        if(!fault){
            assert(m->cal_probe.complete && m->cal_probe_ops==17 && m->cal_probe_reads==12 && m->cal_probe_writes==4 && m->cal_probe_waits==1);
            assert(m->cal_probe.ibi_write_matched==!readonly && m->cal_probe.ibi_changed==!readonly && m->cal_probe.raw_restore_mask==3);
            assert(m->mac_cycle.operations==54 && m->mac_cycle.reads==46 && m->mac_cycle.writes==7);
        }
        unsigned count=mac_model_calls;assert(!wifi_cal_probe_inspect(d,0x2f2f1b1b,UINT32_MAX));assert(count==mac_model_calls);
    }
    cal_probe_quarantine=0;test_time_step=1000;
    puts("xhci calibration probe: PASS actual mode69/parent hook,USB writes/readbacks,readonly effect flags,69 cancellation boundaries,mid-write EP0 quarantine,retained DMA,no replay");
}
