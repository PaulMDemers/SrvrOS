static unsigned passive_trial,passive_ticks;
static bool passive_injected;
static void passive_put16(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void passive_put32(uint8_t *p,uint32_t v){passive_put16(p,v);passive_put16(p+2,v>>16);}
static unsigned passive_wire(uint8_t *p) {
    memset(p,0,4096);uint8_t *wi=p+4,*h=p+36,*b=p+60;
    passive_put16(h,0x80);memset(h+4,255,6);h[10]=h[16]=2;h[15]=h[21]=9;
    passive_put16(h+22,0x120);passive_put16(b+8,100);passive_put16(b+10,0x11);
    b[12]=0;b[13]=3;b[14]='t';b[15]='s';b[16]='t';
    b[17]=1;b[18]=1;b[19]=0x82;b[20]=3;b[21]=1;b[22]=1;
    passive_put32(wi+4,47u<<16);passive_put16(p,80);
    return 84;
}
static void passive_test_tick(void) {
    struct xhci_device *d=passive_test_device;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!m->passive.waiting || passive_injected)return;
    if(++passive_ticks<20)return;
    if(passive_trial==2 || (passive_trial>=3 && passive_trial<=5) || passive_trial==22)return;
    passive_injected=true;
    if(passive_trial==19){m->agc_state.complete=false;return;}
    if(passive_trial==20){test_time=0;return;}
    if(passive_trial==21){test_time_step=0;return;}
    if(passive_trial==52){test_time+=2000000;return;}
    struct xhci_wifi_rx *r=&m->rx;
    unsigned actual=passive_wire(r->buffer),cc=13;
    if(passive_trial==1){actual=4096;cc=1;}
    if(passive_trial==6)actual=0;
    if(passive_trial==7)actual=8;
    if(passive_trial==8)passive_put16(r->buffer,0xfffc);
    if(passive_trial==9)passive_put16(r->buffer+36,0x88);
    if(passive_trial==10)passive_put16(r->buffer+36,0x40);
    if(passive_trial==11)r->buffer[82]=6;
    if(passive_trial==12)r->buffer[80]=9;
    if(passive_trial==13)passive_put32(r->buffer+4,1u<<8);
    unsigned residue=4096-actual;
    if(passive_trial==14)residue=4097;
    if(passive_trial==15)cc=6;
    struct xhci_trb event={.parameter_low=(uint32_t)r->trb,.parameter_high=r->trb>>32,
        .status=(cc<<24)|residue,.control=(32u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)|(xhci.event_cycle?1u:0u)};
    if(passive_trial==16)event.control^=3u<<24;
    if(passive_trial==17)event.control^=2u<<16;
    if(passive_trial==18)event.parameter_low+=16;
    xhci.event_ring[xhci.event_dequeue]=event;
}
static void test_wifi_passive(void) {
    for(unsigned trial=0;trial<53;trial++) {
        reset_test();test_time=0;test_time_step=100;passive_trial=trial;passive_ticks=0;passive_injected=false;
        struct xhci_device *d=&xhci.devices[0];rx_lifecycle_fixture(d);
        struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_rx *r=&m->rx;
        wifi_mcu_select_mode(m,71);assert(m->passive_mode && m->poweron_mode && m->cal_probe_mode && m->post_vco_mode);
        struct xhci_trb ep0[64]={0},events[64]={0},out[64]={0},in[64]={0},main_ring[64]={0},commands[64]={0};
        uint8_t buffer[4096]={0},tx[4096]={0},rx[4096]={0},output[4096]={0};
        _Alignas(4096) uint8_t main_data[4096]={0};
        d->ep0_ring=ep0;d->ep0_ring_phys=0x9000;d->ep0_cycle=true;
        d->descriptor_buffer=buffer;d->descriptor_buffer_phys=(uintptr_t)buffer;d->output_context=output;
        d->firmware.buffer=tx;d->firmware.buffer_phys=(uintptr_t)tx;
        d->firmware.ep=(struct xhci_bulk_endpoint){.ring=out,.ring_phys=(uintptr_t)out,.cycle=true,.dci=16};
        m->buffer=rx;m->buffer_phys=(uintptr_t)rx;
        m->response=(struct xhci_bulk_endpoint){.ring=in,.ring_phys=(uintptr_t)in,.cycle=true,.dci=11};
        m->configured=true;r->configured=true;r->posts=1;r->pending=true;r->requested=4096;r->reads=3;
        r->guards[0][1]=0xf000f200;r->guards[0][2]=0x40000050;
        r->buffer=main_data;r->buffer_phys=(uintptr_t)main_data;r->ep.ring=main_ring;r->ep.ring_phys=r->trb=0x7000;r->ep.dci=9;
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=0x583f;
        cal_model_alc=0x2f2f1b1b;cal_model_ibi=UINT32_MAX;cal_probe_readonly=false;cal_probe_quarantine=0;
        mac_model_calls=mac_model_cancel=poweron_fault=poweron_bulk_calls=0;
        if(trial==22){r->done=true;r->pending=false;r->events=1;r->completion=13;r->remaining=4096-passive_wire(main_data);}
        poweron_test_device=passive_test_device=d;mac_cycle_model=cal_probe_model=true;
        bool ok=wifi_mcu_mac_cycle(d);passive_test_device=poweron_test_device=NULL;
        bool failed=(trial>=14 && trial<=15) || (trial>=19 && trial<=21) || trial==52;
        assert(ok==!failed && !freed && m->poweron.complete && m->poweron_ack_mask==31);
        if(trial==14 || trial==15){assert(d->ep0_quarantined && r->failed);mac_cycle_model=cal_probe_model=false;continue;}
        assert(!mac_model_mac && cal_model_alc==0x2f2f1b1b && cal_model_ibi==UINT32_MAX);
        if(failed){assert(!m->passive.wait_complete && !wifi_passive_capture(d) && !m->passive.inspected);mac_cycle_model=cal_probe_model=false;continue;}
        assert(m->passive.wait_complete && !m->passive.capture_attempted);
        if(trial==51){assert(!wifi_passive_capture(d) && !m->passive.inspected);mac_cycle_model=cal_probe_model=false;continue;}
        xhci.command_ring=commands;xhci.command_ring_phys=0xa000;xhci.command_cycle=true;
        context_write32(output,9,0,3);context_write32(output,9,2,0x7011);
        unsigned e=xhci.event_dequeue;
        if(trial>=3 && trial<=5)events[e++]=(struct xhci_trb){.parameter_low=0x7000,.status=((23+trial)<<24)|(trial==3?0:trial==4?0xffffff:4090),.control=(32u<<10)|(9u<<16)|(1u<<24)|(xhci.event_cycle?1u:0u)};
        events[e]=(struct xhci_trb){.parameter_low=0xa000,.status=1u<<24,.control=(33u<<10)|(1u<<24)|(xhci.event_cycle?1u:0u)};
        assert(wifi_rx_stop(d) && !r->pending && r->stopped && !freed);
        assert(wifi_rx_guard(d,3) && wifi_rx_guard(d,4) && wifi_rx_guard(d,5));
        if((trial>=2 && trial<=6) || (trial>=16 && trial<=18))r->buffer=(uint8_t *)1; /* no usable bytes: MUST NOT dereference */
        switch(trial) {
        case 24:m->passive.wait_complete=false;break;case 25:m->poweron.complete=false;break;
        case 26:m->poweron_ack_mask=0;break;case 27:m->mac_cycle.complete=false;break;case 28:m->mac_cycle.mac_disabled=false;break;
        case 29:r->stopped=false;break;case 30:r->stop_attempted=false;break;case 31:r->stop_state=1;break;
        case 32:r->stop_dequeue=0xbeef;break;case 33:r->stop_dequeue&=~1ull;break;case 34:r->pending=true;break;
        case 35:r->failed=true;break;case 36:r->posts=0;break;case 37:r->requested=4097;break;
        case 38:r->ep.dci=11;break;case 39:r->buffer=NULL;break;case 40:r->buffer_phys=0;break;
        case 41:r->buffer_phys++;break;case 42:r->trb++;break;case 43:m->agc_state.complete=false;break;
        case 44:r->events=2;break;case 45:r->remaining=4097;break;case 46:r->done=false;break;
        case 47:r->completion=6;break;case 48:r->done=false;r->events=0;break;case 49:m->passive_mode=false;break;
        case 50:r->active=false;break;
        }
        assert(wifi_passive_capture(d)==(trial<24));
        if(trial<24) {
            assert(m->passive.complete);
            bool empty=(trial>=2 && trial<=6) || (trial>=16 && trial<=18);
            assert(m->passive.inspected==!empty);
            if(empty)assert(!m->passive.actual && m->passive.view_status==255 && !m->passive.beacon);
            else {
                assert(m->passive.actual==(trial==1?4096:trial==7?8:84));
                bool beacon=trial==0 || trial==1 || trial==12 || trial==22 || trial==23;
                assert(m->passive.beacon==beacon);
                if(beacon)assert(!m->passive.beacon_status && m->passive.ssid_size==3 && m->passive.channel==1 && m->passive.interval==100);
                if(trial==22)assert(!r->passive_during_wait);
                else assert(r->passive_during_wait);
            }
        }else assert(!m->passive.complete && !m->passive.inspected);
        struct xhci_wifi_passive saved=m->passive;assert(!wifi_passive_capture(d) && !memcmp(&saved,&m->passive,sizeof saved));
        assert(!wifi_rx_post(d) && !freed);mac_cycle_model=cal_probe_model=false;
    }
    test_time_step=1000;
    puts("xhci passive: PASS actual mode71/v92 hook,one IN event polling,normal/empty/short/malformed/channel cases,ignored foreign events,clock/ownership failures,actual stop before decode,CC26/27/28 poison pointers,28 stop/length/ownership guards,no rearm/replay/free; synthetic only");
}
