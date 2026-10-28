static void burst_test_tick(void){
    struct xhci_device *d=receive_test_device;struct xhci_wifi_receive *r=&d->firmware.mcu.receive;struct xhci_wifi_burst *b=&r->burst;
    unsigned visit=d->firmware.mcu.scan.index;
    bool early=batch_case==1 || batch_case==3;
    if(!r->posted || !r->pending || (!r->polling && !(early && b->posts==1)))return;
    if(batch_last_trb!=r->trb){batch_last_trb=r->trb;batch_model_ticks=0;batch_injected=0;}
    if(++batch_model_ticks<5 && !early)return;
    if(r->polling && (batch_case==2 || batch_case==3))return;
    unsigned td=(unsigned)((r->trb-r->ep.ring_phys)/16);assert(td<9);
    unsigned index=td-b->first;
    unsigned fault=visit==batch_fault_visit && r->polling && index==2?batch_case:0;
    if(fault==13){test_time_step=0;return;}
    if(fault==14){test_time=0;return;}
    if(fault==18){r->ep.ring[td].status=0;receive_layout_faulted=true;return;}
    if(fault==19){b->pin_physical[8]^=4096;receive_layout_faulted=true;return;}
    if(fault>=20 && fault<=22){
        unsigned n=8;uint64_t alias=fault==20 && visit?d->firmware.mcu.scan.visit[0].receive.burst.physical[8]:fault==22?d->firmware.mcu.window.plan.physical[7]:b->physical[0];
        b->physical[n]=b->pin_physical[n]=alias;b->buffer[n]=pmm_phys_to_virt(alias);b->pin_pointer[n]=(uintptr_t)b->buffer[n];receive_layout_faulted=true;return;
    }
    if(fault==23){r->identity[5]^=4096;receive_layout_faulted=true;return;}
    if(fault==24 && visit){d->firmware.mcu.scan.visit[0].receive.mac_quiescent=false;receive_layout_faulted=true;return;}
    struct xhci_trb *v=&r->ep.ring[td];uint64_t physical=v->parameter_low|((uint64_t)v->parameter_high<<32);
    assert(physical==b->physical[td]);uint8_t *wire=pmm_phys_to_virt(physical);
    unsigned actual=passive_wire(wire);wire[82]=(uint8_t)wifi_tune_channel(d);
    if(fault==4)actual=0;
    if(fault==5)passive_put16(wire,0xfffc);
    if(fault==6)wire[82]=(uint8_t)(wifi_tune_channel(d)==1?6:1);
    if(fault==7)passive_put16(wire+36,0x88);
    struct xhci_trb e={.parameter_low=(uint32_t)r->trb,.parameter_high=r->trb>>32,
        .status=((fault==8?27u:13u)<<24)|(fault==9?4097:4096-actual),.control=(32u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
    if(fault==10)e.control|=4;
    if(fault==11 && !batch_injected)e.control^=2u<<24;
    if(fault==12 && !batch_injected){e.parameter_low=(uint32_t)(r->trb-16);e.parameter_high=(r->trb-16)>>32;}
    if(fault==25 && visit && !batch_injected){uint64_t prior=d->firmware.mcu.scan.visit[0].receive.ep.ring_phys;e.parameter_low=(uint32_t)prior;e.parameter_high=prior>>32;}
    batch_injected++;dispatch_transfer_event(&e);
}
static void test_wifi_service_adapter(struct xhci_device *d){
    bool operational=xhci.operational,configured=d->configured,port_active=port_model.active;
    unsigned port_count=xhci.port_count;uint8_t port=d->port_id,parent=d->parent_slot_id;
    uint32_t *sc=(uint32_t *)(registers+xhci.op_base+XHCI_OP_PORTS),saved=*sc;
    xhci.operational=true;xhci.port_count=1;d->configured=true;d->port_id=1;d->parent_slot_id=0;port_model.active=false;
    *sc=XHCI_PORTSC_CCS|XHCI_PORTSC_PED;
    assert(wifi_scan_service_present(d) && !wifi_scan_service_owned(d,&d->firmware.mcu.scan.batch));
    /* The real capture hook already published before retiring its owner. The
     * retired active-phase proof must refuse now; reads/selects use cache only. */
    struct srv_wifi_snapshot out;assert(wifi_service_snapshot(0,&out)==0 && out.owner && out.count);
    uint64_t owner=out.owner;
    unsigned commands=receive_commands,controls=receive_controls;
    for(unsigned i=0;i<out.count;i++){
        struct srv_wifi_select q={.version=1,.size=sizeof q,.owner=owner,.generation=out.generation,.index=i};
        int result=wifi_service_select(&q);assert(result==SRV_WIFI_OK || result==SRV_WIFI_CONFLICT);
    }
    /* Connected again with latched CSC must retire the old cache identity. */
    *sc|=XHCI_PORTSC_CSC;assert(wifi_service_snapshot(owner,&out)==SRV_WIFI_STALE && !out.count);
    *sc&=~XHCI_PORTSC_CSC;assert(wifi_service_snapshot(owner,&out)==SRV_WIFI_STALE);
    d->parent_slot_id=1;assert(!wifi_scan_service_present(d));d->parent_slot_id=0;
    d->ep0_quarantined=true;assert(!wifi_scan_service_present(d));d->ep0_quarantined=false;
    *sc=0;assert(!wifi_scan_service_present(d));*sc=UINT32_MAX;assert(!wifi_scan_service_present(d));
    assert(commands==receive_commands && controls==receive_controls);
    *sc=saved;xhci.operational=operational;xhci.port_count=port_count;d->configured=configured;d->port_id=port;d->parent_slot_id=parent;port_model.active=port_active;
}
static void test_wifi_burst_case(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;wifi_mcu_select_mode(m,80);
    assert(m->scan_batch_mode && m->scan_mode && !m->scan_retry_mode && m->receive_mode);
    uint8_t input[4096]={0};d->input_context=input;d->input_context_phys=(uintptr_t)input;
    m->tune=(struct xhci_wifi_tune){0};m->receive=(struct xhci_wifi_receive){0};m->scan=(struct xhci_wifi_scan){0};
    /* Use the captured parent TXOP with its temporary bit20 set; the older
     * window fixture also covers the cleared-bit form. */
    m->mac_cycle.txop|=1u<<20;
    mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=m->mac_cycle.txop;
    tune_model_init();tune_fault=tune_at=tune_delay_fault=0;
    receive_retry_fault=receive_fault=receive_ticks=receive_commands=receive_mac_at=receive_controls=receive_control_at=receive_control_fault=0;
    receive_last_trb=0;receive_layout_faulted=false;batch_last_trb=0;batch_model_ticks=batch_injected=0;
    scan_test_history=0;scan_test_rf_visit=3;scan_test_fault_visit=batch_fault_visit;
    scan_test_fault_kind=batch_case==15?2:batch_case==16?1:batch_case==17?3:0;
    if(batch_case>=26){scan_test_fault_visit=3;fail_after=pool_used+batch_case-26;}
    bool service_case=batch_case==0 && batch_fault_visit==0;
    bool old_operational=xhci.operational,old_configured=d->configured,old_port_active=port_model.active;
    unsigned old_port_count=xhci.port_count;uint8_t old_port=d->port_id,old_parent=d->parent_slot_id;
    uint32_t *root_sc=(uint32_t *)(registers+xhci.op_base+XHCI_OP_PORTS),old_sc=*root_sc;
    if(service_case){xhci.operational=true;xhci.port_count=1;d->configured=true;d->port_id=1;d->parent_slot_id=0;port_model.active=false;*root_sc=XHCI_PORTSC_CCS|XHCI_PORTSC_PED;}
    receive_test_device=tune_test_device=d;bool ok=wifi_scan_capture(d);receive_test_device=tune_test_device=NULL;
    bool expected=batch_case<8 || (batch_case>=11 && batch_case<=12) || (batch_case==24 && !batch_fault_visit) || batch_case==25;
    if(ok!=expected)fprintf(stderr,"batch case=%u visit=%u got=%u want=%u completed=%u first=%u posts=%u attempts=%u rxfailed=%u mac=%u stop=%u quiet=%u engine=%u tune=%u alloc=%llu\n",batch_case,batch_fault_visit,ok,expected,m->scan.completed,m->receive.burst.first,m->receive.burst.posts,m->receive.burst.state.attempts,m->receive.failed,m->receive.mac.complete,m->receive.stopped,m->receive.mac_quiescent,m->receive.burst.state.complete,m->tune.state.complete,(unsigned long long)pool_used);
    if(ok!=expected){
        struct xhci_wifi_receive *r=&m->receive;
        fprintf(stderr,"layout=%u retarget=%u/%u owned=%u firstbuf=%llx ring=%llx command=%llx oldrx=%llx/%llx\n",wifi_receive_layout(d),r->retarget_attempted,r->retargeted,wifi_receive_owned(d,true),(unsigned long long)r->buffer_phys,(unsigned long long)r->ep.ring_phys,(unsigned long long)xhci.command_ring_phys,(unsigned long long)m->rx.ep.ring_phys,(unsigned long long)m->rx.buffer_phys);
        for(unsigned n=0;n<9;n++)fprintf(stderr,"p%u=%llx pin=%llx ptr=%p pinptr=%llx\n",n,(unsigned long long)r->burst.physical[n],(unsigned long long)r->burst.pin_physical[n],(void *)r->burst.buffer[n],(unsigned long long)r->burst.pin_pointer[n]);
    }
    assert(ok==expected && !freed);
    if(ok){
        assert(m->scan.completed==3 && m->scan.batch.complete && !m->scan.batch.failed);
        unsigned quiet=batch_case==2 || batch_case==3;
        assert(m->scan.batch.inputs==(quiet?0:24) && m->scan.batch.pending==(quiet?3:0));
        assert(m->scan.batch.parsed==(quiet?0:(batch_case>=4 && batch_case<=7)?23:24));
        for(unsigned i=0;i<3;i++){
            struct xhci_wifi_receive *r=&m->scan.visit[i].receive;struct xhci_wifi_burst *b=&r->burst;
            assert(r->stopped && r->mac_quiescent && b->state.complete && b->posts==(quiet?1u:8u)+b->first);
            assert(b->first==((batch_case==1 || batch_case==3)?1u:0u));
            if(b->first)assert(b->early_events==1 && b->early_remaining==4096-84);
            assert(m->scan.visit[i].tune.state.restored_mask==8191);
            for(unsigned j=0;j<9;j++)assert(b->physical[j]==b->pin_physical[j]);
            if(quiet)assert(b->state.elapsed_us>=10000000 && !b->state.completions);
        }
        if(service_case){
            test_wifi_service_adapter(d);
            *root_sc=old_sc;xhci.operational=old_operational;xhci.port_count=old_port_count;d->configured=old_configured;d->port_id=old_port;d->parent_slot_id=old_parent;port_model.active=old_port_active;
        }
        if(batch_case==12)assert(m->scan.visit[batch_fault_visit].receive.burst.late==1);
        if(batch_case==25 && batch_fault_visit)assert(m->scan.late==1);
        tune_model_restored();unsigned cmds=receive_commands;assert(!wifi_scan_capture(d) && cmds==receive_commands);
        wifi_scan_log(d);
    }else{
        struct mt76_scan_batch empty={.attempted=true,.failed=true};assert(m->scan.failed && !memcmp(&m->scan.batch,&empty,sizeof empty));
        if(m->tune.state.cleanup_complete)tune_model_restored();
    }
    printf("wifi-scan-batch-native: PASS case=%u visit=%u result=%u retained DMA,no replay,independent cleanup\n",batch_case,batch_fault_visit,ok);
}

static void test_wifi_burst(struct xhci_device *d){
    if(!batch_test_group){test_wifi_burst_case(d);return;}
    unsigned group=batch_case;struct xhci_wifi_mcu before=d->firmware.mcu;
    uint8_t context[4096];memcpy(context,d->output_context,sizeof context);
    void *old_input=d->input_context;uint64_t old_phys=d->input_context_phys,limit=fail_after,step=test_time_step;
    for(unsigned n=group*6;n<(group+1)*6;n++)for(unsigned visit=0;visit<3;visit++){
        d->firmware.mcu=before;memcpy(d->output_context,context,sizeof context);d->input_context=old_input;d->input_context_phys=old_phys;
        d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        fail_after=limit;test_time_step=step;batch_case=n;batch_fault_visit=visit;
        test_wifi_burst_case(d);
    }
}
