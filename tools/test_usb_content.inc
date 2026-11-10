/* Real command/validation paths with identical controller callbacks. Host costs
 * exercise observer joins only; none are native controller-latency acceptance. */
static bool content_delay512,content_end_during_bulk,content_phase_overflow,content_checkpoint_overflow;
static struct usb_storage_content_timing premature_content;
static bool content_sampled_bulk(void *ctx,bool in,void *data,uint32_t n,uint32_t *actual) {
    uint64_t start=clock_us;unsigned prior_phase=phase;
    bool ok=bulk(ctx,in,data,n,actual);
    if(content_delay512 && prior_phase==2 && cdb[0]==0x2a && length==512)clock_us+=351000;
    uint64_t end=clock_us;
    fake_timing=(struct usb_bulk_timing){.submitted=start+1,.observed=end-1,
        .resumed=end,.seen=true,.service_valid=true,.service_checks=1,.service_samples=1,
        .service_gap_max_us=end-start-2,.completion_gap_us=end-start-2};
    if(prior_phase==0 && content_command_active) {
        if(content_phase_overflow) {content_phase_overflow=false;content_command.phase[0].us=UINT64_MAX;}
        if(content_checkpoint_overflow) {content_checkpoint_overflow=false;content_command.checkpoint_us=UINT64_MAX;}
    }
    if(content_end_during_bulk) {
        content_end_during_bulk=false;usb_storage_content_end(&premature_content);
    }
    return ok;
}
static void content_fixture(void) {
    reset();regress_clock=false;checkpoint_delay=0;checkpoints=0;
    content_delay512=content_end_during_bulk=false;
    content_phase_overflow=content_checkpoint_overflow=false;
    assert(usb_storage_attach(&storage,&storage,bulk,true));
    storage.bulk=content_sampled_bulk;storage.transfer_timing=get_timing;
    clock_us=100;
}
static void content_open(void) {
    assert(usb_storage_timing_aux_begin());assert(usb_storage_content_begin());
}
static void content_close(struct usb_storage_content_timing *c,struct usb_storage_timing *a) {
    usb_storage_content_end(c);usb_storage_timing_aux_end(a);
    assert(!content_active && !content_command_active && !aux_timing_active);
}
static void test_content_commands(void) {
    static uint8_t bytes[32768],image[sizeof(disk)];
    struct usb_storage_content_timing c,none,saved;
    struct usb_storage_timing a,l;
    assert(!usb_storage_content_begin());
    unsigned no=usb_storage_content_role_begin(2);assert(no==USB_STORAGE_CONTENT_NO_ROLE);
    usb_storage_content_role_end(no);usb_storage_content_end(&none);assert(!none.valid && !none.active && !none.invalid);
    content_fixture();content_delay512=true;
    assert(usb_storage_timing_begin());content_open();
    saved=content_timing;assert(!usb_storage_content_begin());assert(!memcmp(&saved,&content_timing,sizeof(saved)));
    unsigned old=usb_storage_content_role_begin(2);
    assert(block.write(&block,0,bytes,sizeof(bytes))); /* Two existing16KiB commands. */
    unsigned nested=usb_storage_content_role_begin(4);
    assert(block.write(&block,57344,bytes,512));usb_storage_content_role_end(nested);
    assert(block.write(&block,32768,bytes,512));usb_storage_content_role_end(old);
    old=usb_storage_content_role_begin(3);assert(block.read(&block,0,bytes,512));usb_storage_content_role_end(old);
    assert(block.flush(&block)); /* Deliberately unclassified/preflight role6. */
    uint8_t tur[6]={0};assert(command(&storage,tur,6,false,NULL,0)==0);
    usb_storage_content_end(&c);assert(c.valid && !c.active && !c.invalid);
    assert(c.roles[2].write_commands==3 && c.roles[2].write_bytes==32768+512);
    assert(c.roles[2].write512_commands==1 && c.roles[2].csw_calls==3);
    assert(c.roles[2].csw_us==351090 && c.roles[2].write_csw_us==351090);
    assert(c.roles[2].write512_csw_us==351030 && c.roles[2].over100ms==1);
    assert(c.roles[2].write_over100ms==1 && c.roles[2].write512_over100ms==1);
    assert(c.roles[2].worst.opcode==0x2a && c.roles[2].worst.command_bytes==512 && c.roles[2].worst.actual==13);
    assert(c.roles[2].worst.sample.seen && c.roles[2].worst.sample.service_valid);
    assert(c.roles[4].write_commands==1 && c.roles[4].write512_commands==1 && c.roles[4].write512_csw_us==351030);
    assert(c.roles[3].read_commands==1 && c.roles[3].read_bytes==512 && !c.roles[3].write_csw_us && !c.roles[3].write_over100ms);
    assert(c.roles[6].flush_commands==1 && c.roles[6].other_commands==1 && c.roles[6].csw_calls==2);
    /* Both slow512-byte CSWs tie: retain the first global maximum and the
     * complete original command, not the last command or a per-phase maximum. */
    assert(c.command.valid && c.command.complete && c.command.ok && c.command.role==4);
    assert(c.command.csw_start_us==c.roles[4].worst.start_us && c.command.csw_end_us==c.roles[4].worst.end_us);
    assert(c.command.phase[0].calls==1 && c.command.phase[0].requested==31 && c.command.phase[0].actual==31);
    assert(c.command.phase[1].calls==1 && c.command.phase[1].requested==512 && c.command.phase[1].actual==512);
    assert(c.command.phase[2].calls==1 && c.command.phase[2].requested==13 && c.command.phase[2].actual==13);
    assert(c.command.start_us==c.command.phase[0].first_us && c.command.end_us==c.command.checkpoint_last_us);
    assert(c.command.checkpoint_calls==1 && c.command.checkpoint_us==0);
    usb_storage_timing_end(&l);usb_storage_timing_aux_end(&a);
    assert(l.valid && a.valid && !usb_storage_timing_enabled());
    uint64_t commands=0,csws=0,us=0;
    for(unsigned i=0;i<USB_STORAGE_CONTENT_ROLES;i++) {
        const struct usb_storage_content_role *r=&c.roles[i];
        commands+=r->read_commands+r->write_commands+r->flush_commands+r->other_commands;
        csws+=r->csw_calls;us+=r->csw_us;assert(!r->failed_commands);
    }
    assert(commands==7 && csws==7 && commands==a.calls[0] && us==a.us[2]);
    assert(!memcmp(l.calls,a.calls,sizeof(l.calls)) && !memcmp(l.us,a.us,sizeof(l.us)));
    saved=c;content_open();assert(block.flush(&block));content_close(&c,&a);
    assert(c.roles[6].csw_calls==1 && !c.roles[2].write_commands && saved.roles[2].write_commands==3);

    /* Observation does not change segmentation, callback counts, checkpoint
     * eligibility, command tags, image bytes or callback-time costs. */
    unsigned baseline_calls=0,baseline_writes=0,baseline_syncs=0,baseline_checkpoints=0;
    uint64_t baseline_commands=0,baseline_clock=0,baseline_clock_reads=0,baseline_sample_reads=0;
    for(unsigned enabled=0;enabled<2;enabled++) {
        content_fixture();content_delay512=true;checkpoint_delay=37;
        /* Keep the existing auxiliary observer active in both cases. The new
         * command companion consumes exactly its existing clock/sample calls. */
        assert(usb_storage_timing_aux_begin());
        if(enabled)assert(usb_storage_content_begin());
        clock_reads=sample_reads=0;
        old=usb_storage_content_role_begin(2);
        for(unsigned i=0;i<sizeof(bytes);i++)bytes[i]=(uint8_t)(i*17+3);
        assert(block.write(&block,0,bytes,sizeof(bytes)));
        assert(block.write(&block,40001,bytes,513));
        assert(block.read(&block,0,bytes,sizeof(bytes)));assert(block.flush(&block));
        usb_storage_content_role_end(old);
        if(enabled) {content_close(&c,&a);assert(c.valid && !c.invalid);}
        else usb_storage_timing_aux_end(&a);
        if(!enabled) {
            memcpy(image,disk,sizeof(image));baseline_calls=calls;baseline_writes=writes;
            baseline_syncs=syncs;baseline_checkpoints=checkpoints;
            baseline_commands=storage.commands;baseline_clock=clock_us;
            baseline_clock_reads=clock_reads;baseline_sample_reads=sample_reads;
        } else assert(!memcmp(image,disk,sizeof(image)) && calls==baseline_calls && writes==baseline_writes &&
            syncs==baseline_syncs && checkpoints==baseline_checkpoints && storage.commands==baseline_commands && clock_us==baseline_clock &&
            clock_reads==baseline_clock_reads && sample_reads==baseline_sample_reads);
    }
    /* A16KiB command contains four unchanged4KiB callbacks and three existing
     * inter-chunk checkpoints plus the original post-validation checkpoint. */
    content_fixture();checkpoint_delay=37;content_open();old=usb_storage_content_role_begin(2);
    assert(block.write(&block,0,bytes,16384));usb_storage_content_role_end(old);content_close(&c,&a);
    assert(c.command.valid && c.command.complete && c.command.ok && c.command.role==2);
    assert(c.command.start_us==100 && c.command.end_us==368);
    assert(c.command.phase[0].calls==1 && c.command.phase[0].us==10 && c.command.phase[0].first_us==100 && c.command.phase[0].last_us==110);
    assert(c.command.phase[1].calls==4 && c.command.phase[1].requested==16384 && c.command.phase[1].actual==16384 && c.command.phase[1].us==80);
    assert(c.command.phase[1].first_us==110 && c.command.phase[1].last_us==301);
    assert(c.command.phase[2].calls==1 && c.command.phase[2].us==30 && c.command.phase[2].first_us==301 && c.command.phase[2].last_us==331);
    assert(c.command.csw_start_us==301 && c.command.csw_end_us==331 && c.command.checkpoint_calls==4 && c.command.checkpoint_us==148);
    assert(c.command.checkpoint_first_us==130 && c.command.checkpoint_last_us==368);
    saved=c;content_open();assert(block.flush(&block));content_close(&c,&a);
    assert(c.command.role==6 && c.command.phase[1].calls==0 && c.command.phase[1].requested==0 && c.command.phase[1].actual==0 &&
        c.command.phase[1].us==0 && c.command.phase[1].first_us==0 && c.command.phase[1].last_us==0);
    assert(c.command.checkpoint_calls==1 && saved.command.phase[1].calls==4 && saved.command.start_us==100);
    checkpoint_delay=0;
    /* A later failed command with a smaller CSW cannot replace the earlier
     * command companion; overall role failure still rejects positive evidence. */
    content_fixture();content_delay512=true;content_open();old=usb_storage_content_role_begin(2);
    assert(block.write(&block,0,bytes,512));content_delay512=false;fault=17;
    assert(!block.read(&block,0,bytes,512));usb_storage_content_role_end(old);content_close(&c,&a);
    assert(c.command.complete && c.command.ok && c.command.csw_end_us-c.command.csw_start_us==351030 && c.roles[2].failed_commands==1);
    /* Every original validation failure remains a failed/quarantined command,
     * including SCSI failure after a successful CSW callback. */
    for(unsigned f=1;f<=17;f++) {
        content_fixture();content_open();old=usb_storage_content_role_begin(4);
        fault=f;fault_chunk=1;
        assert(!block.read(&block,0,bytes,512));
        unsigned before=calls;assert(!block.read(&block,0,bytes,512) && calls==before);
        usb_storage_content_role_end(old);content_close(&c,&a);
        assert(c.valid && !c.active && !c.invalid && c.roles[4].read_commands==1 && c.roles[4].failed_commands==1);
        assert(storage.failed && block.offline && !depth && !storage.busy && irq_enabled);
        assert(c.roles[4].csw_calls<=1);
        if(c.roles[4].csw_calls)assert(c.command.complete && !c.command.ok && c.command.role==4 &&
            c.command.csw_start_us==c.roles[4].worst.start_us && c.command.csw_end_us==c.roles[4].worst.end_us);
        else assert(!c.command.complete);
        if(f==17)assert(c.roles[4].csw_calls==1 && c.roles[4].worst.ok==1);
    }
    /* Nested role order, overflow and clock faults invalidate only evidence. */
    content_fixture();content_open();old=usb_storage_content_role_begin(2);nested=usb_storage_content_role_begin(4);
    usb_storage_content_role_end(old);assert(content_timing.invalid && content_depth==2 && content_role==4);
    usb_storage_content_role_end(nested);usb_storage_content_role_end(old);content_close(&c,&a);assert(!c.valid && c.invalid && a.valid);
    content_fixture();content_open();assert(usb_storage_content_role_begin(7)==USB_STORAGE_CONTENT_NO_ROLE);
    content_close(&c,&a);assert(!c.valid && c.invalid && a.valid);
    content_fixture();content_open();old=usb_storage_content_role_begin(2);
    content_timing.roles[2].write_commands=UINT64_MAX;assert(block.write(&block,0,bytes,512));
    usb_storage_content_role_end(old);content_close(&c,&a);assert(!c.valid && c.invalid && a.valid && !storage.failed);
    content_fixture();content_open();old=usb_storage_content_role_begin(2);
    content_timing.roles[2].write512_csw_us=UINT64_MAX;assert(block.write(&block,0,bytes,512));
    usb_storage_content_role_end(old);content_close(&c,&a);assert(!c.valid && c.invalid && a.valid && !storage.failed);
    content_fixture();content_open();content_phase_overflow=true;assert(block.flush(&block));
    content_close(&c,&a);assert(!c.valid && c.invalid && !c.command.valid && c.command.complete && c.command.ok && a.valid && !storage.failed);
    content_fixture();content_open();checkpoint_delay=1;content_checkpoint_overflow=true;assert(block.flush(&block));
    content_close(&c,&a);assert(!c.valid && c.invalid && !c.command.valid && c.command.complete && c.command.ok && a.valid && !storage.failed);
    checkpoint_delay=0;
    content_fixture();content_open();regress_clock=true;assert(block.flush(&block));regress_clock=false;
    content_close(&c,&a);assert(!c.valid && c.invalid && !a.valid && !storage.failed);
    content_fixture();content_open();old=usb_storage_content_role_begin(2);content_end_during_bulk=true;
    assert(block.write(&block,0,bytes,512));assert(premature_content.active && premature_content.invalid);
    usb_storage_content_role_end(old);content_close(&c,&a);assert(!c.valid && !c.active && c.invalid && a.valid);
    content_fixture();assert(usb_storage_timing_begin());content_open();usb_storage_timing_aux_end(&a);
    assert(a.valid && content_timing.invalid && content_active && timing_active);
    assert(block.flush(&block));usb_storage_content_end(&c);usb_storage_timing_end(&l);
    assert(!c.valid && c.invalid && !c.active && l.valid && l.calls[0]==1 && !usb_storage_timing_enabled());
    puts("usb-content-commands: PASS exact role/count/byte/512vs16K/CSW joins, same-command phase/checkpoint/final-result companions, immutable first maxima, on-off media/callback/clock/sample equivalence, original validation/quarantine, nested/clock/overflow and peer-window preservation");
}
