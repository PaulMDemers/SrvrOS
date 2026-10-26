/* Included after AGC fixtures: execute the actual retained-endpoint helpers. */
static void rx_lifecycle_fixture(struct xhci_device *d) {
    agc_state_fixture(d,7);struct xhci_wifi_mcu *m=&d->firmware.mcu;
    m->agc_state=(struct mt76_agc_state){.attempted=true,.complete=true,.init_done=true,.reads=8,
        .raw={0x16344ef0,0},.initial={78,0},.current={78,0},.low_gain=-1};
    for(unsigned j=0;j<2;j++){m->agc_state.guards[j][1]=0xf000f200;m->agc_state.guards[j][2]=0x40000050;}
    m->agc_state_submitted=8;m->rx_lifecycle_mode=true;m->rx.active=m->rx.attempted=true;
    assert(wifi_rx_owned(d));
}
static void test_wifi_rx_lifecycle(void) {
    for(unsigned fault=0;fault<244;fault++) {
        reset_test();struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);
        struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_rx *r=&m->rx;
        uint8_t input[4096]={0},output[4096]={0};struct xhci_trb commands[64]={0},events[64]={0};
        d->input_context=input;d->input_context_phys=(uintptr_t)input;d->output_context=output;
        xhci.command_ring=commands;xhci.command_ring_phys=0xa000;xhci.command_cycle=true;
        xhci.event_ring=events;xhci.event_cycle=true;xhci.rt_base=0x2000;xhci.db_base=0x1000;r->reads=3;r->guards[0][1]=0xf000f200;r->guards[0][2]=0x40000050;
        events[0]=(struct xhci_trb){.parameter_low=0xa000,.status=1u<<24,.control=(33u<<10)|(1u<<24)|1};
        if(fault<=229)post_vco_fault(m,fault);
        if(fault==226)m->post_vco.complete=false;
        if(fault==230)m->agc_state.complete=false;
        if(fault==231)m->agc_state.raw[1]=1;
        if(fault==232)m->agc_state_submitted++;
        if(fault==233)m->rx_lifecycle_mode=false;
        if(fault==234)r->active=false;
        if(fault==235)r->reads++;
        if(fault==236)firmware_command_failed=true;
        if(fault==237)events[0].status=6u<<24;
        if(fault==238)events[0].control=(33u<<10)|(2u<<24)|1;
        if(fault==239)events[0]=(struct xhci_trb){0};
        if(fault==240)fail_after=1;
        if(fault==241)r->attempted=false;
        if(fault==242)d->output_context=NULL;
        if(fault==243)r->posts=1;
        bool ok=configure_wifi_rx(d);assert(ok==!fault && !freed);
        if(fault && fault<237)assert(!pool_used && !xhci.command_enqueue);
        if(!fault) {
            assert(context_read32(input,0,1)==(1u|(1u<<9)));
            assert(context_read32(input,10,1)==((3u<<1)|(6u<<3)|(512u<<16)));
            assert((context_read32(input,1,0)>>27)==16 && r->ep.dci==9 && r->ep.packet==512);
            assert(wifi_rx_post(d) && r->pending && r->requested==4096 && r->posts==1);
            assert(r->trb==r->ep.ring_phys && r->ep.ring[0].status==4096);
            assert(!wifi_rx_post(d) && !configure_wifi_rx(d));
        }
    }
    for(unsigned fault=0;fault<16;fault++) {
        reset_test();struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);
        struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
        struct xhci_trb ring[64]={0},commands[64]={0},events[64]={0};uint8_t output[4096]={0};
        r->configured=true;r->posts=1;r->pending=true;r->requested=4096;
        r->ep.ring=ring;r->ep.ring_phys=r->trb=0x9000;r->ep.dci=9;d->output_context=output;
        xhci.command_ring=commands;xhci.command_ring_phys=0xa000;xhci.command_cycle=true;
        xhci.event_ring=events;xhci.event_cycle=true;xhci.rt_base=0x2000;
        context_write32(output,9,0,3);context_write32(output,9,2,0x9001);
        events[0]=(struct xhci_trb){.parameter_low=0xdead,.status=1u<<24,.control=(33u<<10)|(1u<<24)|1};
        events[1]=(struct xhci_trb){.parameter_low=0x9000,.status=26u<<24|4096,.control=(32u<<10)|(9u<<16)|(1u<<24)|1};
        events[2]=(struct xhci_trb){.parameter_low=0xa000,.status=1u<<24,.control=(33u<<10)|(1u<<24)|1};
        if(fault==1)events[2].status=6u<<24;
        if(fault==2)events[2].control=(33u<<10)|(2u<<24)|1;
        if(fault==3)events[2]=(struct xhci_trb){0};
        if(fault==4)context_write32(output,9,0,1);
        if(fault==5)context_write32(output,9,2,0xbeef);
        if(fault==6)r->stop_attempted=true;
        if(fault==7)firmware_command_failed=true;
        if(fault==8)strict_control.device=d;
        if(fault==9)r->ep.dci=11;
        /* Complete-before-stop and stop-without-transfer-event both quiesce. */
        if(fault==10){r->pending=false;r->done=true;context_write32(output,9,2,0x9011);events[1].parameter_low=0xbeef;}
        if(fault==11)events[1].status=27u<<24|0xffffff;
        if(fault==12)events[1].status=28u<<24|4090;
        if(fault==13)events[1].parameter_low=0xbeef;
        if(fault==14)context_write32(output,9,2,0x9000);
        if(fault==15)d->firmware.mcu.agc_state.complete=false; /* cleanup still needed */
        bool success=fault==0 || (fault>=10 && fault<=13) || fault==15;
        assert(wifi_rx_stop(d)==success && !freed);
        if(success){assert(r->stopped && !r->pending);assert(commands[0].control==((15u<<10)|(9u<<16)|(1u<<24)|1));}
        unsigned count=xhci.command_enqueue;assert(!wifi_rx_stop(d) && xhci.command_enqueue==count);
        if(success){unsigned late=r->late_events;assert(wifi_rx_event(d,&events[1])==(events[1].parameter_low==0x9000));if(events[1].parameter_low==0x9000)assert(r->late_events==late+1);}
        strict_control.device=NULL;
    }
    for(unsigned fault=0;fault<9;fault++) {
        reset_test();struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
        r->configured=true;r->posts=1;r->pending=true;r->requested=4096;r->trb=0x9000;
        struct xhci_trb e={.parameter_low=0x9000,.status=(13u<<24)|4000,.control=(32u<<10)|(9u<<16)|(1u<<24)|1};
        if(fault==1)e.control=(32u<<10)|(11u<<16)|(1u<<24)|1;
        if(fault==2)e.parameter_low++;
        if(fault==3)e.control=(32u<<10)|(9u<<16)|(2u<<24)|1;
        if(fault==4)e.status=(1u<<24)|4097;
        if(fault==5)e.status=26u<<24;
        if(fault==6)e.status=6u<<24;
        if(fault==7)e.control|=4;
        if(fault==8)r->stopping=true;
        bool matches=fault==0 || fault>=4;
        assert(wifi_rx_event(d,&e)==matches);
        assert(r->failed==(fault>=4 && fault<=7));
        if(!matches)assert(r->pending && !r->events);
        if(fault==0)assert(r->done && r->remaining==4000 && !r->pending);
    }
    for(unsigned op=0;op<6;op++)for(unsigned fault=0;fault<5;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        r->reads=op;r->configured=r->stopped=op>=3;r->posts=op>=3;
        for(unsigned n=0;n<op;n++)r->guards[n/3][n%3]=n%3==0?0:n%3==1?0xf000f200:0x40000050;
        d->ep0_ring=ring;d->ep0_ring_phys=0x9000;d->ep0_cycle=true;d->descriptor_buffer=buffer;d->descriptor_buffer_phys=(uintptr_t)buffer;
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==4?6:1,0);
        rx_guard_model=true;rx_guard_value=op%3==0?0:op%3==1?0xf000f200:0x40000050;
        if(fault==1)r->reads++;
        if(fault==2)r->active=false;
        if(fault==3)rx_guard_value^=1;
        assert(wifi_rx_guard(d,op)==!fault);
        if(fault==1 || fault==2)assert(!d->ep0_enqueue);
        if(!fault)assert(r->reads==op+1 && !wifi_rx_guard(d,op));
        assert(!freed);rx_guard_model=false;
    }
    puts("xhci RX lifecycle: PASS prior-proof/configuration faults,one post,MCU/stale event separation,exact stop/state/dequeue,no replay,retained DMA");
}
