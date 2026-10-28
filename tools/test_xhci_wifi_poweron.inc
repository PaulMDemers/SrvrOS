static unsigned poweron_fault,poweron_bulk_calls;
static void poweron_test_bulk(unsigned dci) {
    struct xhci_device *d=poweron_test_device;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    unsigned step=m->poweron_commands-1;assert(step<5 && dci==m->dci);poweron_bulk_calls++;
    if(poweron_fault==200+poweron_bulk_calls)return; /* bounded transfer timeout */
    if(dci==16) {
        uint8_t expected[16];assert(mt76_poweron_packet(step,expected,16)==16);
        assert(!memcmp(expected,d->firmware.buffer,16));
    }else{
        static const uint32_t ids[5]={1,12,255,3,2},args[5]={0,1,0x600,0,1};
        uint32_t words[4]={0x4a000008u|((step+1)<<16),ids[step],args[step],0x4a000008u|((step+1)<<16)};
        for(unsigned i=0;i<16;i++)m->buffer[i]=(uint8_t)(words[i/4]>>(8*(i%4)));
        if(poweron_fault==100+step)m->buffer[4]^=1;
    }
    if(poweron_fault==300+poweron_bulk_calls)m->agc_state.complete=false;
    xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)m->trb,.parameter_high=m->trb>>32,
        .status=dci==16?1u<<24:(13u<<24)|(MT76_MCU_RESPONSE_MAX-16),
        .control=(32u<<10)|(dci<<16)|((uint32_t)d->slot_id<<24)|(xhci.event_cycle?1u:0u)};
}
static void test_wifi_poweron(void) {
    for(unsigned trial=0;trial<123;trial++) {
        reset_test();test_time=0;test_time_step=100;
        struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);
        struct xhci_wifi_mcu *m=&d->firmware.mcu;wifi_mcu_select_mode(m,70);
        assert(m->post_vco_mode && m->cal_probe_mode && m->poweron_mode && m->mac_cycle_mode);
        struct xhci_trb ep0[64]={0},events[64]={0},out[64]={0},in[64]={0};
        uint8_t buffer[4096]={0},tx[4096]={0},rx[4096]={0};
        d->ep0_ring=ep0;d->ep0_ring_phys=0x9000;d->ep0_cycle=true;
        d->descriptor_buffer=buffer;d->descriptor_buffer_phys=(uintptr_t)buffer;
        d->firmware.buffer=tx;d->firmware.buffer_phys=(uintptr_t)tx;
        d->firmware.ep=(struct xhci_bulk_endpoint){.ring=out,.ring_phys=(uintptr_t)out,.cycle=true,.dci=16};
        m->buffer=rx;m->buffer_phys=(uintptr_t)rx;
        m->response=(struct xhci_bulk_endpoint){.ring=in,.ring_phys=(uintptr_t)in,.cycle=true,.dci=11};
        m->configured=true;m->rx.configured=true;m->rx.posts=1;m->rx.pending=true;
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=0x583f;
        cal_model_alc=0x2f2f1b1b;cal_model_ibi=UINT32_MAX;cal_probe_readonly=false;cal_probe_quarantine=trial>=119?(unsigned[]){40,43,45,47}[trial-119]:0;
        mac_model_calls=0;mac_model_cancel=trial<=91?trial:0;
        poweron_fault=trial>=92 && trial<97?100+trial-92:trial>=97 && trial<107?201+trial-97:trial>=107 && trial<117?301+trial-107:0;
        if(trial==117)m->ant_fields.nic1|=1u<<13;
        if(trial==118)m->trim_fields.valid=false,m->trim_fields.nic0=0;
        poweron_bulk_calls=0;poweron_test_device=d;mac_cycle_model=cal_probe_model=true;
        bool ok=wifi_mcu_mac_cycle(d);
        poweron_test_device=NULL;mac_cycle_model=cal_probe_model=false;
        assert(ok==(trial==0));
        if((trial>=97 && trial<107) || trial>=119) {
            assert(d->ep0_quarantined && (trial>=119 || m->failed) && !m->poweron.complete && !m->mac_cycle.complete && !m->mac_cycle.mac_disabled);
        } else assert(!mac_model_mac && cal_model_alc==0x2f2f1b1b && cal_model_ibi==UINT32_MAX);
        assert(!freed);
        assert(m->rx.pending && !m->rx.stop_attempted);
        if(!trial) {
            assert(m->poweron.complete && m->poweron_ops==30 && m->poweron_reads==18 && m->poweron_writes==4 && m->poweron_waits==3);
            assert(m->poweron_commands==5 && m->poweron_ack_mask==31 && m->poweron.restored_mask==3 && poweron_bulk_calls==10);
        }
        unsigned calls=mac_model_calls,bulk=poweron_bulk_calls;
        assert(!wifi_poweron_inspect(d,0x2f2f1b1b,UINT32_MAX) && mac_model_calls==calls && bulk==poweron_bulk_calls);
    }
    cal_probe_quarantine=0;test_time_step=1000;
    puts("xhci power-on: PASS mode70 and full parent/probe hook, actual bulk framing/replies,91 USB cancellation boundaries,each command mismatch/OUT+IN timeout/ownership loss,board gates,temporary/restore-write EP0 quarantine,independent restore/teardown,retained RX,no replay; synthetic only");
}
