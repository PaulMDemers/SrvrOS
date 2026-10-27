static unsigned scan_cases;
static void test_wifi_scan(struct xhci_device *d){
    uint8_t input[4096]={0};void *old_input=d->input_context;uint64_t old_input_phys=d->input_context_phys;
    d->input_context=input;d->input_context_phys=(uintptr_t)input;
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_mcu before=*m;uint8_t context[4096];memcpy(context,d->output_context,sizeof context);
    for(unsigned trial=0;trial<1+3*28+24+6;trial++){
        *m=before;m->channel_mode=false;m->tune_mode=m->receive_mode=m->scan_mode=true;
        m->tune=(struct xhci_wifi_tune){0};m->receive=(struct xhci_wifi_receive){0};m->scan=(struct xhci_wifi_scan){0};
        memcpy(d->output_context,context,sizeof context);d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
        mac_model_mac=0;mac_model_filter=0x17f97;mac_model_txop=before.mac_cycle.txop;
        tune_model_init();tune_fault=tune_at=tune_delay_fault=0;
        receive_fault=receive_ticks=receive_commands=receive_mac_at=receive_controls=receive_control_at=receive_control_fault=0;
        scan_test_history=0;scan_test_rf_visit=3;
        scan_test_fault_visit=trial && trial<=84?(trial-1)/28:3;scan_test_fault_kind=trial && trial<=84?(trial-1)%28:0;
        if(trial>=85 && trial<109){unsigned n=trial-85;scan_test_rf_visit=n/8;tune_fault=n%2?8:1;tune_at=(unsigned[]){1,70,105,152}[(n%8)/2];}
        if(trial>=109)scan_test_history=trial-108;
        receive_test_device=tune_test_device=d;uint64_t step=test_time_step;bool ok=wifi_scan_capture(d);test_time_step=step;receive_test_device=tune_test_device=NULL;scan_cases++;
        unsigned fault=scan_test_fault_kind;
        bool expected=trial<85 && (!trial || fault==0 || fault==5 || fault==8 || fault==11 || fault==12 || (fault>=19 && fault<=24));
        if(ok!=expected)fprintf(stderr,"scan trial=%u visit=%u fault=%u ok=%u expected=%u completed=%u tune=%u rx=%u stopped=%u quiet=%u command=%u\n",trial,scan_test_fault_visit,fault,ok,expected,m->scan.completed,m->tune.state.complete,m->receive.complete,m->receive.stopped,m->receive.mac_quiescent,firmware_command_failed);
        assert(ok==expected && !freed && !m->scan.active);
        if(ok){
            assert(m->scan.completed==3 && m->scan.result.complete && !m->scan.result.failed && m->scan.result.inputs==3);
            for(unsigned i=0;i<3;i++)assert(m->scan.visit[i].tune.state.channel==mt76_scan_channel(i) && m->scan.visit[i].receive.stopped && m->scan.visit[i].receive.mac_quiescent && m->scan.visit[i].tune.state.restored_mask==8191);
            assert(m->scan.result.parsed<=3);tune_model_restored();wifi_scan_log(d);
            if(!trial){
                assert(m->scan.result.parsed==3 && m->scan.result.count==1 && m->scan.result.row[0].channel_conflict && m->scan.result.row[0].channels==1057);
                /* Prior completions must never mutate the final receive ledger. */
                struct xhci_wifi_receive last=m->receive;unsigned late=m->scan.late;
                struct xhci_trb event={.parameter_low=(uint32_t)m->scan.visit[0].receive.trb,.parameter_high=m->scan.visit[0].receive.trb>>32,.status=13u<<24,.control=(32u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
                dispatch_transfer_event(&event);assert(m->scan.late==late+1 && !memcmp(&last,&m->receive,sizeof last));
                unsigned calls=tune_bulk_calls;assert(!wifi_scan_capture(d) && calls==tune_bulk_calls);
            }
        }else{
            struct mt76_scan empty={.attempted=true,.failed=true};assert(m->scan.failed && !memcmp(&empty,&m->scan.result,sizeof empty));
            unsigned bad=trial<85?scan_test_fault_visit:trial<109?scan_test_rf_visit:1;
            assert(m->scan.completed<=bad && m->scan.index==bad);
            if(scan_test_history>=4)assert(m->receive.stop_attempted && m->receive.stopped && !m->tune.state.cleanup_complete);
            if(m->tune.state.cleanup_complete)tune_model_restored();
        }
    }
    scan_test_fault_visit=scan_test_rf_visit=3;scan_test_fault_kind=scan_test_history=0;receive_fault=0;
    *m=before;memcpy(d->output_context,context,sizeof context);d->ep0_quarantined=d->firmware.failed=false;d->firmware.ep.packet=512;firmware_command_failed=false;
    d->input_context=old_input;d->input_context_phys=old_input_phys;
    printf("wifi-scan-native: PASS %u three-visit US1/6/11,fresh retained rings/pages,per-visit command/receive/clock failures,stop before restoration,private dedup/conflicts,late-event refusal,no replay/no DMA free\n",scan_cases);
}
