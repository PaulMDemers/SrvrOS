static bool window_discovery,receive_test_only,scan_test_only;
static unsigned window_trial,window_ticks,window_last_post,window_injected;
static void window_test_tick(void) {
    struct xhci_device *d=window_test_device;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct xhci_wifi_window *w=&m->window;struct xhci_wifi_rx *r=&m->rx;
    if(!w->polling || !w->state.attempted || !r->pending)return;
    if(window_last_post!=r->posts){window_last_post=r->posts;window_ticks=window_injected=0;}
    if(++window_ticks<20)return;
    if(window_trial==1 || window_trial==50 || ((window_trial==2 || window_trial==53) && r->posts==4))return;
    if(window_trial==3){test_time=0;return;}
    if(window_trial==4){test_time_step=0;return;}
    if(window_trial==5 || window_trial==52){test_time+=(window_trial==52 || window_discovery)?20000000:2000000;return;}
    if(window_trial==6 && r->posts==4){m->agc_state.complete=false;return;}
    if(window_injected>=2)return;
    uint64_t pointer=r->trb;unsigned cc=13,actual=passive_wire(r->buffer);
    if(window_trial==7 && r->posts==3)actual=0;
    if(window_trial==8 && r->posts==3){cc=1;actual=4096;}
    if(window_trial==9 && r->posts==3)passive_put16(r->buffer,0xfffc);
    if(window_trial==10 && r->posts==3)passive_put16(r->buffer+36,0x88);
    if(window_trial==11 && r->posts==3)r->buffer[82]=6;
    unsigned residue=4096-actual;
    if(window_trial==12 && r->posts==3)residue=4097;
    if(window_trial==13 && r->posts==3)cc=26;
    if(window_trial==14 && r->posts==3)cc=27;
    if(window_trial==15 && r->posts==3)cc=28;
    if(window_trial==16 && r->posts==3 && !window_injected)pointer-=16;
    struct xhci_trb e={.parameter_low=(uint32_t)pointer,.parameter_high=pointer>>32,
        .status=(cc<<24)|residue,.control=(32u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)|(xhci.event_cycle?1u:0u)};
    if(window_trial==17 && r->posts==3 && !window_injected)e.control^=2u<<24;
    if(window_trial==18 && r->posts==3 && !window_injected)e.control^=2u<<16;
    if(window_trial==19 && r->posts==3)e.control|=4;
    if(window_trial==49 && r->posts==3 && !window_injected)
        e=(struct xhci_trb){.parameter_low=0xbea0,.status=(13u<<24)|56,
            .control=(32u<<10)|(5u<<16)|(2u<<24)|(xhci.event_cycle?1u:0u)};
    xhci.event_ring[xhci.event_dequeue]=e;window_injected++;
}
static unsigned discovery_fault_at,discovery_fault_calls;
static bool discovery_fault_owned(void *context,const struct mt76_rx_window *window) {
    struct xhci_device *d=context;
    if(++discovery_fault_calls==discovery_fault_at)context_write32(d->output_context,9,0,1);
    return wifi_discovery_owned(context,window);
}
static void test_wifi_window(void) {
    for(unsigned discovery=0;discovery<3;discovery++)for(unsigned trial=0;trial<55;trial++) {
        if(receive_test_only && (discovery!=2 || trial!=0))continue;
        window_discovery=discovery;
        reset_test();test_time=0;test_time_step=100;
        if(trial>=50 || discovery)test_time_step=10000;
        window_trial=trial;window_ticks=window_last_post=window_injected=0;
        struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);
        struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_rx *r=&m->rx;
        struct xhci_wifi_window *w=&m->window;
        wifi_mcu_select_mode(m,discovery==2?75:discovery?74:trial>=50?73:72);assert(m->channel_mode==(discovery==2));assert(m->discovery_mode==(bool)discovery);assert(m->window_long_mode==(trial>=50 || discovery));assert(m->window_mode && !m->passive_mode && m->poweron_mode && m->post_vco_mode);
        struct xhci_trb ep0[64]={0},events[256]={0},out[64]={0},in[64]={0},commands[64]={0};
        _Alignas(4096) struct xhci_trb main_ring[256]={0};
        _Alignas(4096) uint8_t pages[8][4096]={{0}};
        uint8_t buffer[4096]={0},tx[4096]={0},rx[4096]={0},output[4096]={0};
        uint8_t key_report[64]={0,0,4};injected=0;
        if(trial==49)xhci.devices[1]=(struct xhci_device){.present=true,.slot_id=2,
            .hid_keyboard=true,.interrupt_pending=true,.interrupt_trb=0xbea0,
            .interrupt_dci=5,.interrupt_max_packet=64,.interrupt_buffer=key_report};
        d->ep0_ring=ep0;d->ep0_ring_phys=0x9000;d->ep0_cycle=true;
        d->descriptor_buffer=buffer;d->descriptor_buffer_phys=(uintptr_t)buffer;d->output_context=output;
        d->firmware.buffer=tx;d->firmware.buffer_phys=(uintptr_t)tx;
        d->firmware.ep=(struct xhci_bulk_endpoint){.ring=out,.ring_phys=(uintptr_t)out,.cycle=true,.dci=16};
        m->buffer=rx;m->buffer_phys=(uintptr_t)rx;
        m->response=(struct xhci_bulk_endpoint){.ring=in,.ring_phys=(uintptr_t)in,.cycle=true,.dci=11};
        if(discovery==2){d->vendor_id=0x7392;d->product_id=0xa711;d->device_bcd=0x100;d->firmware.ep.packet=512;m->response.packet=512;}
        m->configured=true;r->configured=true;r->posts=1;r->pending=true;r->requested=4096;r->reads=3;
        r->guards[0][1]=0xf000f200;r->guards[0][2]=0x40000050;
        r->buffer=pages[0];r->buffer_phys=(uintptr_t)pages[0];r->trb=(uintptr_t)main_ring;
        r->ep=(struct xhci_bulk_endpoint){.ring=main_ring,.ring_phys=(uintptr_t)main_ring,.dci=9,.packet=512,.enqueue=1,.cycle=true};
        main_ring[0]=(struct xhci_trb){.parameter_low=(uint32_t)r->buffer_phys,.parameter_high=r->buffer_phys>>32,
            .status=4096,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP|1};
        w->prepared=true;w->plan=(struct mt76_rx_window_plan){.budget_us=m->window_long_mode?MT76_RX_WINDOW_LONG_US:0,.ring=(uintptr_t)main_ring,.slot=d->slot_id,.dci=9};
        for(unsigned i=0;i<8;i++){w->plan.buffer[i]=pages[i];w->plan.physical[i]=(uintptr_t)pages[i];}
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=0x583f;
        cal_model_alc=0x2f2f1b1b;cal_model_ibi=UINT32_MAX;cal_probe_readonly=false;cal_probe_quarantine=0;
        mac_model_calls=mac_model_cancel=poweron_fault=poweron_bulk_calls=0;
        if(trial==20){r->done=true;r->pending=false;r->events=1;r->completion=13;r->remaining=4096-passive_wire(pages[0]);}
        poweron_test_device=window_test_device=d;mac_cycle_model=cal_probe_model=true;
        bool ok=wifi_mcu_mac_cycle(d);window_test_device=poweron_test_device=NULL;
        bool failed=(trial>=3 && trial<=6) || (trial>=12 && trial<=15) || trial==19 || trial==52;
        assert(ok==!failed && !freed && m->poweron.complete && m->poweron_ack_mask==31);
        if((trial>=12 && trial<=15) || trial==19) {
            assert(r->failed && !w->state.complete);
            mac_cycle_model=cal_probe_model=false;continue;
        }
        assert(!mac_model_mac && cal_model_alc==0x2f2f1b1b && cal_model_ibi==UINT32_MAX);
        assert(!w->state.finish_attempted);
        if(failed)assert(w->state.failed && !w->state.complete);
        assert(w->state.polled && w->state.poll_ok==!failed);
        for(unsigned i=0;i<8;i++)assert(!w->state.item[i].inspected);
        if(!failed)assert(r->posts==((trial==1 || trial==50)?1:(trial==2 || trial==53)?4:8));
        xhci.command_ring=commands;xhci.command_ring_phys=0xa000;xhci.command_cycle=true;
        context_write32(output,9,0,3);context_write32(output,9,2,(uint32_t)(r->trb+16)|1);context_write32(output,9,3,r->trb>>32);
        unsigned e=xhci.event_dequeue;
        if(trial==1 || trial==2 || trial==50 || trial==53)
            events[e++]=(struct xhci_trb){.parameter_low=(uint32_t)r->trb,.parameter_high=r->trb>>32,.status=(26u<<24)|4096,.control=(32u<<10)|(9u<<16)|(1u<<24)|(xhci.event_cycle?1u:0u)};
        if(trial==21)context_write32(output,9,0,1);
        if(trial==22)context_write32(output,9,2,(uint32_t)(r->trb+32)|1);
        if(trial==23)context_write32(output,9,2,(uint32_t)(r->trb+16)|3);
        events[e]=(struct xhci_trb){.parameter_low=0xa000,.status=1u<<24,.control=(33u<<10)|(1u<<24)|(xhci.event_cycle?1u:0u)};
        bool stopped=wifi_rx_stop(d);
        if(trial>=21 && trial<=23){assert(!stopped && !wifi_window_finish(d));mac_cycle_model=cal_probe_model=false;continue;}
        assert(stopped && !r->pending && r->stopped && !freed);
        if(failed) {
            assert(!wifi_window_finish(d) && !w->state.complete);
            for(unsigned i=0;i<8;i++)assert(!w->state.item[i].inspected);
            mac_cycle_model=cal_probe_model=false;continue;
        }
        assert(wifi_rx_guard(d,3) && wifi_rx_guard(d,4) && wifi_rx_guard(d,5));
        switch(trial) {
        case 24:m->mac_cycle.complete=false;break;case 25:m->mac_cycle.mac_disabled=false;break;
        case 26:r->stopped=false;break;case 27:r->pending=true;break;
        case 28:r->stop_state=1;break;case 29:r->stop_dequeue+=16;break;
        case 30:r->stop_dequeue&=~1ull;break;case 31:r->reads=5;break;
        case 32:r->guards[1][2]^=1;break;case 33:m->poweron.complete=false;break;
        case 34:m->poweron_ack_mask=0;break;case 35:m->agc_state.complete=false;break;
        case 36:r->ep.enqueue++;break;case 37:r->ep.cycle=false;break;
        case 38:r->trb-=16;break;case 39:r->buffer=pages[0];break;
        case 40:w->plan.physical[0]+=4096;break;case 41:w->plan.buffer[0]=pages[1];break;
        case 42:main_ring[0].status++;break;case 43:main_ring[0].control^=1;break;
        case 44:main_ring[0].parameter_low^=16;break;case 45:m->mac_cycle.filter_restored=false;break;
        case 46:m->mac_cycle.txop_restored=false;break;case 47:m->mac_cycle.rx_drained=false;break;
        case 48:w->state.plan.physical[7]^=4096;break;
        case 54:w->state.plan.budget_us=0;break;
        }
        bool captured=wifi_window_finish(d);assert(captured==(trial<24 || trial==49 || trial==50 || trial==51 || trial==53));
        if(captured) {
            assert(w->state.complete && w->state.stopped && !w->state.failed);
            assert(w->state.beacons==((trial==1 || trial==50)?0:(trial==2 || trial==53)?3:(trial==7 || trial==9 || trial==10 || trial==11)?7:8));
            if(trial==50 || trial==53)assert(w->state.elapsed_us>=10000000 && w->state.elapsed_us<=10100000);
            if(trial==8)assert(w->state.item[2].actual==4096);
            assert(w->initial_early==(trial==20));
            if(trial==49)assert(injected==1 && xhci.devices[1].hid_reports==1);
            if(discovery) {
                if(trial==0) {
                    struct xhci_wifi_mcu before=*m;
                    uint8_t output_saved[4096];memcpy(output_saved,output,sizeof output);
                    struct xhci_trb td=main_ring[0];
                    for(unsigned fault=0;fault<24;fault++) {
                        *m=before;memcpy(output,output_saved,sizeof output);main_ring[0]=td;
                        switch(fault) {
                        case 0:m->discovery_mode=false;break;case 1:m->window_long_mode=false;break;
                        case 2:context_write32(output,9,0,1);break;
                        case 3:context_write32(output,9,2,(uint32_t)(r->trb+32)|1);break;
                        case 4:r->stop_state=1;break;case 5:r->stop_dequeue+=16;break;
                        case 6:w->state.complete=false;break;case 7:w->state.item[3].actual++;break;
                        case 8:w->state.item[3].view_status=255;break;
                        case 9:w->state.plan.physical[7]^=4096;break;
                        case 10:m->mac_cycle.complete=false;break;case 11:m->mac_cycle.filter_restored=false;break;
                        case 12:m->mac_cycle.txop_restored=false;break;case 13:m->poweron_ack_mask=0;break;
                        case 14:r->pending=true;break;case 15:m->agc_state.complete=false;break;
                        case 16:r->reads=5;break;case 17:main_ring[0].status--;break;
                        case 18:w->state.item[5].probe_response=true;break;
                        case 19:context_write32(output,9,2,(uint32_t)(r->trb+16)|3);break;
                        case 20:r->ep.cycle=false;break;case 21:r->trb-=16;break;
                        case 22:r->active=false;break;case 23:w->state.stopped=false;break;
                        }
                        assert(!wifi_discovery_capture(d) && !m->discovery.complete && !m->discovery.count && !freed);
                        for(unsigned i=0;i<sizeof m->discovery.row;i++)assert(!((uint8_t *)m->discovery.row)[i]);
                    }
                    for(unsigned failure=1;failure<=18;failure++) {
                        *m=before;memcpy(output,output_saved,sizeof output);main_ring[0]=td;
                        discovery_fault_at=failure;discovery_fault_calls=0;
                        const struct mt76_discovery_io io={.context=d,.owned=discovery_fault_owned};
                        assert(!mt76_discovery_collect(&m->discovery,&w->state,&io));
                        assert(discovery_fault_calls==failure && !m->discovery.count && m->discovery.failed && !freed);
                        for(unsigned i=0;i<sizeof m->discovery.row;i++)assert(!((uint8_t *)m->discovery.row)[i]);
                        unsigned checks=m->discovery_checks;assert(!wifi_discovery_capture(d) && checks==m->discovery_checks);
                    }
                    *m=before;memcpy(output,output_saved,sizeof output);main_ring[0]=td;
                }
                assert(wifi_discovery_capture(d) && m->discovery.complete && !m->discovery.failed);
                assert(m->discovery.inputs==w->state.completions && m->discovery.parsed==w->state.beacons);
                assert(m->discovery.count==(w->state.beacons?1u:0u));
                if(m->discovery.count)assert(m->discovery.row[0].observations==w->state.beacons &&
                    m->discovery.row[0].ssid_size==3 && !memcmp(m->discovery.row[0].ssid,"tst",3));
                assert(m->discovery_checks==2+2*w->state.completions-(trial==7?1:0));
                struct mt76_discovery copy=m->discovery;unsigned checks=m->discovery_checks;
                assert(!wifi_discovery_capture(d) && checks==m->discovery_checks && !memcmp(&copy,&m->discovery,sizeof copy));
                wifi_discovery_log(d);
                if(discovery==2 && trial==0){
                    if(receive_test_only){if(scan_test_only)test_wifi_scan(d);else test_wifi_receive(d);mac_cycle_model=cal_probe_model=false;return;}
                    test_wifi_channel(d);test_wifi_tune(d);test_wifi_receive(d);test_wifi_scan(d);
                }
            } else assert(!wifi_discovery_capture(d) && !m->discovery.attempted);

        } else assert(!w->state.complete);
        assert(!wifi_window_finish(d) && !freed);
        mac_cycle_model=cal_probe_model=false;
    }
    for(unsigned mode=72;mode<=77;mode++)for(unsigned limit=0;limit<=9;limit++) {
        reset_test();struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);
        struct xhci_wifi_mcu *m=&d->firmware.mcu;wifi_mcu_select_mode(m,mode);
        struct xhci_wifi_rx *r=&m->rx;r->reads=3;r->guards[0][1]=0xf000f200;r->guards[0][2]=0x40000050;
        uint8_t input[4096]={0},output[4096]={0};struct xhci_trb commands[64]={0},events[64]={0};
        d->input_context=input;d->input_context_phys=(uintptr_t)input;d->output_context=output;
        xhci.command_ring=commands;xhci.command_ring_phys=0xa000;xhci.command_cycle=true;
        xhci.event_ring=events;xhci.event_cycle=true;xhci.rt_base=0x2000;xhci.db_base=0x1000;
        events[0]=(struct xhci_trb){.parameter_low=0xa000,.status=1u<<24,.control=(33u<<10)|(1u<<24)|1};
        fail_after=limit;bool ok=configure_wifi_rx(d);assert(ok==(limit==9) && !freed);
        if(ok) {
            assert(pool_used==9 && m->window.prepared && wifi_rx_post(d) && wifi_window_layout(d));
            assert(m->window.plan.budget_us==(mode>=73?MT76_RX_WINDOW_LONG_US:0));
            assert(!configure_wifi_rx(d) && !wifi_rx_post(d));
        } else assert(!r->posts && !xhci.command_enqueue);
    }
    puts("wifi-window-native: PASS289 short/long/discovery/channel-budget,live-context/atomic-loss/ledger/allocation/shared-HID/whole-poweron/parent-cleanup/unique-TRB/event/clock/exact-stop scenarios; no DMA freed");
}
