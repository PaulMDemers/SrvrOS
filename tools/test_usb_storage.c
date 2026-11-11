#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned depth;
static uint64_t checkpoint_delay;
static uint64_t clock_us;
static uint64_t clock_reads;
static bool reference=true,regress_clock;
uint64_t timer_monotonic_us(void) { clock_reads++;return clock_us; }
bool timer_reference_ready(void) { return reference; }
static bool irq_enabled, foreign, boot;
static uint64_t usb_test_irq_save(void) { bool f=irq_enabled; irq_enabled=false; return f; }
static void usb_test_irq_restore(uint64_t f) { irq_enabled=f; }
#define SRVROS_USB_STORAGE_HOST_TEST
#define SRVROS_COMMIT_WAIT_HOST_TEST
#include "../kernel/src/drivers/usb_storage.c"
static uint64_t status_hooks,successful_status_hooks;
static bool owner_observe;
bool sync_commit_wait_status_enabled(void) {return owner_observe;}
void sync_commit_wait_status(uint64_t tag,uint64_t opcode,uint64_t bytes,uint64_t start,uint64_t end,uint64_t requested,uint64_t actual,bool ok,const struct usb_bulk_timing *sample) {
    (void)tag;(void)opcode;(void)bytes;
    assert(sample && requested==13);
    status_hooks++;
    if(ok && actual==13 && end>=start)successful_status_hooks++;
}
static struct block_device block, alias, other;
static struct usb_storage storage;
static uint8_t disk[65536], cdb[16];
static uint32_t tag, length, done;
static unsigned data_calls, fault_chunk, checkpoints;
static unsigned phase, calls, writes, syncs, senses, tur_failures, fault;
static bool empty, claim_busy,retire_during_attach;
static uint32_t sector_bytes=512;
static unsigned diagnosed;
static void diagnose(void *ctx) { assert(ctx==&storage);diagnosed++; }
void console_printf(const char *f,...) {(void)f;}
bool scheduler_io_begin(void) { if(foreign || boot) return false; depth++; return true; }
bool scheduler_io_active(void) { return depth || foreign; }
void scheduler_io_end(void) { assert(depth); depth--; }
struct block_device *block_parent(struct block_device *d) { return d == &alias ? &block : d; }
struct block_device *block_device_at(uint64_t i) { return i == 0 ? &block : i == 1 ? &alias : i == 2 ? &other : NULL; }
bool block_enable_partition_writes(struct block_device *d) { return d->write != NULL; }
struct block_device *block_find(const char *name) { (void)name; return NULL; }
struct block_device *block_register(const char *name,uint64_t size,uint64_t count,block_read_fn r,block_write_fn w,void *p) {
    block=(struct block_device){.used=true,.name=name,.block_size=size,.block_count=count,.read=r,.write=w,.private_data=p}; return &block;
}
static unsigned gpt_scans, mbr_scans, gpt_found;
uint64_t block_register_gpt_partitions(struct block_device *d) { assert(d==&block); gpt_scans++; return gpt_found; }
uint64_t block_register_mbr_partitions(struct block_device *d) { assert(d==&block); mbr_scans++; return 0; }
static bool bulk(void *ctx,bool in,void *data,uint32_t n,uint32_t *actual) {
    if(retire_during_attach) {retire_during_attach=false;assert(storage.initializing && !usb_storage_retire(&storage));}
    assert(ctx==&storage); if(regress_clock) clock_us--; else clock_us+=(phase+1)*10; calls++; *actual=n;
    if(fault==1) return false;
    if((fault==10 && phase==2) || (fault==11 && phase==1)) { *actual=0;return false; }
    if(fault==12 && phase==2) { *actual=0;phase=0;return true; }
    if(phase==0) {
        assert(!in && n==31 && le32(data)==0x43425355);
        uint8_t *b=data; tag=le32(b+4); length=le32(b+8);
        assert(b[13]==0 && b[14]>0 && b[14]<=16);
        memcpy(cdb,b+15,16); phase=length?1:2;done=0;data_calls=0;
        if(fault==2) *actual=30;
        return true;
    }
    if(phase==1) {
        data_calls++;
        assert(n==(length-done>USB_STORAGE_BULK_BYTES?USB_STORAGE_BULK_BYTES:length-done));
        if(fault==13 && data_calls==fault_chunk){*actual=0;return false;}
        if(cdb[0]==0x12) { assert(in); memset(data,0,n); }
        else if(cdb[0]==3) { assert(in); memset(data,0,n); ((uint8_t *)data)[0]=0x70; ((uint8_t *)data)[2]=empty?2:6; senses++; }
        else if(cdb[0]==0x25) { assert(in); put_be32(data,sizeof(disk)/sector_bytes-1); put_be32((uint8_t *)data+4,sector_bytes); }
        else {
            assert((cdb[0]==0x28 && in)||(cdb[0]==0x2a && !in));
            uint32_t count=((uint32_t)cdb[7]<<8)|cdb[8];
            assert(count && length==count*sector_bytes && n<=USB_STORAGE_BULK_BYTES);
            uint32_t off=be32(cdb+2)*sector_bytes+done; assert(off+n<=sizeof(disk));
            if(in) memcpy(data,disk+off,n); else { memcpy(disk+off,data,n); writes++; }
        }
        done+=n;phase=done==length?2:1;
        if(fault==3 || (fault==14 && data_calls==fault_chunk)){*actual=n-1;phase=2;}
        if(fault==15 && data_calls==fault_chunk)*actual=n+1;
        if(fault==16 && data_calls==fault_chunk){*actual=0;phase=2;}
        return true;
    }
    assert(in && n==13); memset(data,0,n);
    put_le32(data,fault==4?0:0x53425355); put_le32((uint8_t *)data+4,tag+(fault==5));
    if(fault==6) put_le32((uint8_t *)data+8,length+1);
    if(fault==7) ((uint8_t *)data)[12]=2;
    if(fault==8) *actual=12;
    if(fault==9) put_le32((uint8_t *)data+8,1);
    if(cdb[0]==0 && tur_failures) { tur_failures--; ((uint8_t *)data)[12]=1; }
    if(fault==17) ((uint8_t *)data)[12]=1;
    if(cdb[0]==0x35) syncs++;
    phase=0; return true;
}
static void reset(void) {
    memset(&storage,0,sizeof(storage)); memset(&block,0,sizeof(block));
    memset(disk,0x69,sizeof(disk)); phase=calls=writes=syncs=senses=tur_failures=fault=depth=0;
    gpt_scans=mbr_scans=gpt_found=0;
    empty=foreign=false; irq_enabled=true;done=data_calls=fault_chunk=0;
    sector_bytes=512;
}
static void test_batches(void) {
    static uint8_t expected[sizeof(disk)], input[sizeof(disk)], output[sizeof(disk)];
    const unsigned offsets[]={0,1,510,512,4095,4096,8191,16383,16384,32767};
    const unsigned lengths[]={0,1,2,511,512,513,4095,4096,4097,8192,16383,16384,16385,32768,65536};
    for (unsigned sector=512;sector<=4096;sector*=2) {
        for (unsigned a=0;a<sizeof(offsets)/sizeof(*offsets);a++) {
            for (unsigned b=0;b<sizeof(lengths)/sizeof(*lengths);b++) {
                unsigned off=offsets[a], n=lengths[b];
                if (n>sizeof(disk)-off) continue;
                reset(); sector_bytes=sector;
                assert(usb_storage_attach(&storage,&storage,bulk,true));
                assert(block.max_transfer_bytes==USB_STORAGE_COMMAND_BYTES);
                memset(expected,0x69,sizeof(expected));
                for (unsigned i=0;i<n;i++) input[i]=(uint8_t)(i*37+11);
                memcpy(expected+off,input,n);
                assert(block.write(&block,off,input,n));
                assert(!memcmp(disk,expected,sizeof(disk)));
                memset(output,0,sizeof(output));
                assert(block.read(&block,off,output,n) && !memcmp(output,input,n));
                assert(irq_enabled && !depth && !storage.busy);
            }
        }
    }
    reset(); assert(usb_storage_attach(&storage,&storage,bulk,true));
    unsigned before=calls, service_before=checkpoints;
    assert(block.write(&block,0,input,sizeof(input)));
    assert(checkpoints-service_before==16);
    assert(writes==16 && calls-before==24); /* Four commands, each four data chunks, no prereads. */
    before=calls;service_before=checkpoints;
    assert(block.read(&block,0,output,sizeof(output)) && !memcmp(input,output,sizeof(input)));
    assert(calls-before==24 && checkpoints-service_before==16);
    fault=5; before=calls;
    assert(!block.write(&block,0,input,sizeof(input)) && storage.failed);
    assert(calls-before==6); /* Ambiguous first command: no next command/retry. */
}
static void test_segment_faults(void) {
    static uint8_t buffer[USB_STORAGE_COMMAND_BYTES*2];
    for(unsigned in=0;in<2;in++)for(unsigned bad=13;bad<=16;bad++)for(unsigned chunk=1;chunk<=4;chunk++) {
        reset();assert(usb_storage_attach(&storage,&storage,bulk,true));
        fault=bad;fault_chunk=chunk;unsigned before=calls;
        uint64_t commands=storage.commands;
        bool ok=in?block.read(&block,0,buffer,sizeof(buffer)):block.write(&block,0,buffer,sizeof(buffer));
        assert(!ok && storage.failed && storage.commands==commands+1 && data_calls==chunk);
        /* Short data proceeds only to CSW; failed/overrun chunks stop at once. */
        assert(calls-before==1+chunk+(bad==14 || bad==16));
        before=calls;
        assert(!block.read(&block,0,buffer,512) && calls==before && !depth && !storage.busy);
    }
}
static struct usb_bulk_timing fake_timing;
static unsigned sample_reads,raw_timing_calls;
static void get_timing(void *ctx,struct usb_bulk_timing *out) { assert(ctx==&storage);sample_reads++;*out=fake_timing; }
static bool timing_bulk(void *ctx,bool in,void *data,uint32_t n,uint32_t *actual) {
    (void)ctx;(void)in;(void)data;(void)n;*actual=0;raw_timing_calls++;
    clock_us+=1200000;return true;
}
static bool owner_timing_bulk(void *ctx,bool in,void *data,uint32_t n,uint32_t *actual) {
    assert(usb_storage_timing_enabled()==owner_observe);
    bool ok=timing_bulk(ctx,in,data,n,actual);*actual=n;return ok;
}
static void test_owner_status_window(void) {
    reset();storage.context=&storage;storage.bulk=owner_timing_bulk;storage.transfer_timing=get_timing;
    uint32_t actual=0;clock_us=100;
    fake_timing=(struct usb_bulk_timing){.submitted=110,.observed=1000110,.resumed=1100110,
        .seen=true,.service_valid=true,.service_checks=512,.service_samples=2,
        .service_gap_max_us=500000,.completion_gap_us=500000};
    uint64_t reads=clock_reads,queries=sample_reads,callbacks=raw_timing_calls,hooks=status_hooks;
    assert(!owner_observe && !usb_storage_timing_enabled());
    assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,512));
    assert(clock_reads==reads && sample_reads==queries && status_hooks==hooks && raw_timing_calls==callbacks+1);
    owner_observe=true;clock_us=100;
    assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,512));
    assert(clock_reads==reads+2 && sample_reads==queries+1 && status_hooks==hooks+1 && raw_timing_calls==callbacks+2);
    assert(!owner_status_active && !usb_storage_timing_enabled());
    /* An existing trace shares the same two clocks and single cached sample. */
    clock_us=100;assert(usb_storage_timing_begin());
    assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,16384));
    struct usb_storage_timing out;usb_storage_timing_end(&out);
    assert(out.valid && clock_reads==reads+4 && sample_reads==queries+2 && status_hooks==hooks+2 && raw_timing_calls==callbacks+3);
    owner_observe=false;assert(!owner_status_active && !usb_storage_timing_enabled());
    puts("usb-owner-status: PASS disabled clock count, status-only observation, unchanged callback count, one cached sample shared with existing traces");
}
static void test_transfer_timing(void) {
    reset();storage.context=&storage;storage.bulk=timing_bulk;storage.transfer_timing=get_timing;
    uint32_t actual;struct usb_storage_timing out;
    clock_us=100;fake_timing=(struct usb_bulk_timing){.submitted=110,.observed=1000110,.resumed=1100110,.seen=true,.service_valid=true,.service_checks=512,.service_samples=2,.service_gap_max_us=500000,.completion_gap_us=500000};
    assert(usb_storage_timing_begin() && usb_storage_timing_enabled());
    assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,16384));usb_storage_timing_end(&out);
    assert(out.valid && out.phase[2].max_us==1200000 && out.phase[2].observed_calls==1);
    assert(out.phase[2].observe_us==1000000 && out.phase[2].resume_us==100000);
    assert(out.phase[2].over20ms==1 && out.phase[2].over100ms==1 && out.phase[2].over1s==1);
    assert(out.phase[2].service_checks==512 && out.phase[2].service_samples==2);
    assert(out.phase[2].service_gap_max_us==500000 && out.phase[2].completion_gap_max_us==500000);
    assert(!usb_storage_timing_enabled());
    for(unsigned bad=0;bad<11;bad++) {
        clock_us=100;fake_timing=(struct usb_bulk_timing){.submitted=110,.observed=1000110,.resumed=1100110,.seen=true,.service_valid=true,.service_checks=512,.service_samples=2,.service_gap_max_us=500000,.completion_gap_us=500000};
        if(bad==0)fake_timing.submitted=99;
        if(bad==1)fake_timing.observed=109;
        if(bad==2)fake_timing.resumed=1000109;
        if(bad==3)fake_timing.resumed=1200101;
        if(bad==4)fake_timing.seen=false;
        if(bad==6)fake_timing.service_valid=false;
        if(bad==7)fake_timing.service_samples=3;
        if(bad==8)fake_timing.service_gap_max_us=1000001;
        if(bad==9)fake_timing.completion_gap_us=500001;
        if(bad==10)fake_timing.service_gap_max_us=100000;
        assert(usb_storage_timing_begin());
        if(bad==5)timing.phase[2].observe_us=UINT64_MAX;
        assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,16384));usb_storage_timing_end(&out);assert(!out.valid);
    }
}
static void test_timing_overlap(void) {
    reset();storage.context=&storage;storage.bulk=timing_bulk;storage.transfer_timing=get_timing;
    fake_timing=(struct usb_bulk_timing){.submitted=110,.observed=1000110,.resumed=1100110,.seen=true,.service_valid=true,.service_checks=512,.service_samples=2,.service_gap_max_us=500000,.completion_gap_us=500000};
    struct usb_storage_timing legacy,aux,none;uint32_t actual;
    assert(usb_storage_timing_begin() && usb_storage_timing_aux_begin());
    assert(!usb_storage_timing_begin() && !usb_storage_timing_aux_begin());
    unsigned raw_before=raw_timing_calls,samples_before=sample_reads;
    clock_us=100;assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,16384));
    assert(raw_timing_calls==raw_before+1 && sample_reads==samples_before+1);
    usb_storage_timing_end(&legacy);assert(usb_storage_timing_enabled());
    clock_us=100;assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,16384));
    usb_storage_timing_aux_end(&aux);assert(!usb_storage_timing_enabled());
    assert(legacy.valid && aux.valid && legacy.calls[2]==1 && aux.calls[2]==2);
    assert(aux.csw.opcode==0x2a && aux.csw.command_bytes==16384 && aux.csw.request_bytes==13);
    assert(aux.csw.start_us==100 && aux.csw.end_us==1200100 && aux.csw.sample.observed==1000110);
    assert(!legacy.csw.start_us); /* The legacy observer is unchanged. */
    assert(aux.us[2]==2*legacy.us[2] && aux.phase[2].observe_us==2*legacy.phase[2].observe_us);
    assert(usb_storage_timing_aux_begin());usb_storage_timing_end(&none);assert(!none.valid && usb_storage_timing_enabled());
    clock_us=100;assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,16384));
    assert(usb_storage_timing_begin());
    clock_us=100;assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,16384));
    usb_storage_timing_aux_end(&aux);assert(usb_storage_timing_enabled());
    usb_storage_timing_aux_end(&none);assert(!none.valid && usb_storage_timing_enabled());
    clock_us=100;assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,16384));usb_storage_timing_end(&legacy);
    assert(legacy.valid && aux.valid && legacy.calls[2]==2 && aux.calls[2]==2);
    assert(!usb_storage_timing_enabled());
    /* Overflow in one window cannot invalidate or overwrite its peer. */
    assert(usb_storage_timing_begin() && usb_storage_timing_aux_begin());timing.us[2]=UINT64_MAX;
    clock_us=100;assert(timed_bulk(&storage,2,true,NULL,13,&actual,0x2a,16384));usb_storage_timing_end(&legacy);usb_storage_timing_aux_end(&aux);
    assert(!legacy.valid && aux.valid && aux.calls[2]==1);
    puts("usb-timing-overlap: PASS independent starts/ends, legacy refusal, same observations, stopped/foreign scope preservation and isolated overflow");
}
static void test_command_context(void) {
    static uint8_t bytes[USB_STORAGE_COMMAND_BYTES];
    struct usb_storage_timing out;
    for(unsigned write=0;write<2;write++) {
        reset();assert(usb_storage_attach(&storage,&storage,bulk,true));
        clock_us=100;checkpoint_delay=37;
        unsigned before=checkpoints;
        assert(usb_storage_timing_aux_begin());
        assert(write?write_blocks(&block,0,bytes,sizeof(bytes)):read_blocks(&block,0,bytes,sizeof(bytes)));
        usb_storage_timing_aux_end(&out);
        assert(out.valid && out.csw.opcode==(write?0x2a:0x28));
        assert(out.csw.command_bytes==sizeof(bytes) && out.csw.request_bytes==13 && out.csw.actual==13 && out.csw.ok);
        assert(out.checkpoint_calls==4 && out.checkpoint_us==148 && checkpoints-before==4);
    }
    reset();assert(usb_storage_attach(&storage,&storage,bulk,true));
    assert(usb_storage_timing_aux_begin());assert(flush(&block));usb_storage_timing_aux_end(&out);
    assert(out.csw.opcode==0x35 && !out.csw.command_bytes && out.checkpoint_calls==1 && out.checkpoint_us==37);
    /* Ending legacy observation must not end or overwrite auxiliary metadata. */
    assert(usb_storage_timing_aux_begin() && usb_storage_timing_begin());
    assert(flush(&block));usb_storage_timing_end(&out);
    assert(!out.csw.start_us && !out.checkpoint_calls);
    assert(flush(&block));usb_storage_timing_aux_end(&out);assert(out.checkpoint_calls==2);
    assert(usb_storage_timing_aux_begin());aux_timing.checkpoint_us=UINT64_MAX;
    timed_checkpoint();usb_storage_timing_aux_end(&out);assert(!out.valid);
    assert(usb_storage_timing_aux_begin());aux_timing.checkpoint_calls=UINT64_MAX;
    timed_checkpoint();usb_storage_timing_aux_end(&out);assert(!out.valid);
    checkpoint_delay=0;
    puts("usb-command-timing: PASS read/write/flush context, exact existing checkpoint calls, independent peer preservation and overflow refusal");
}
static void test_timing(void) {
    reset();assert(usb_storage_attach(&storage,&storage,bulk,true));
    struct usb_storage_timing out;
    reference=false;assert(!usb_storage_timing_begin());reference=true;
    assert(usb_storage_timing_begin());assert(!usb_storage_timing_begin());
    uint8_t bytes[8192];assert(read_blocks(&block,0,bytes,sizeof(bytes)));
    usb_storage_timing_end(&out);
    assert(out.valid && out.calls[0]==1 && out.calls[1]==2 && out.calls[2]==1);
    assert(out.us[0]==10 && out.us[1]==40 && out.us[2]==30 && !out.failures);
    assert(usb_storage_timing_begin());assert(flush(&block));usb_storage_timing_end(&out);
    assert(out.valid && out.calls[0]==1 && out.calls[1]==0 && out.calls[2]==1);
    assert(usb_storage_timing_begin());fault=10;
    assert(!read_blocks(&block,0,bytes,512));usb_storage_timing_end(&out);
    assert(out.valid && out.failures==1 && out.calls[2]==1 && storage.failed);
    usb_storage_timing_end(&out);assert(!out.valid);
    reset();assert(usb_storage_attach(&storage,&storage,bulk,true));
    assert(usb_storage_timing_begin());timing.us[0]=UINT64_MAX;
    assert(flush(&block));usb_storage_timing_end(&out);assert(!out.valid);
    assert(usb_storage_timing_begin());regress_clock=true;
    assert(flush(&block));usb_storage_timing_end(&out);assert(!out.valid);regress_clock=false;
}
static void test_runtime_status(void) {
    for(unsigned operation=0;operation<3;operation++) {
        reset();assert(usb_storage_attach(&storage,&storage,bulk,true));
        uint8_t bytes[512]={0};fault=17;
        bool ok=operation==0?block.read(&block,0,bytes,sizeof(bytes)):
            operation==1?block.write(&block,0,bytes,sizeof(bytes)):block.flush(&block);
        assert(!ok);fault=0;unsigned before=calls;
        assert(!block.write(&block,0,bytes,sizeof(bytes)) && calls==before);
        assert(!block.read(&block,0,bytes,sizeof(bytes)) && calls==before);
        assert(!block.flush(&block) && calls==before && storage.failed && block.offline && block.used);
    }
}
static unsigned retired_diagnostics;
static void retiring_diagnostic(void *context) {
    assert(context==&storage && storage.diagnostic_users==1);
    assert(!usb_storage_retire(&storage) && block.private_data==&storage);
    retired_diagnostics++;
    if(retired_diagnostics==1)usb_storage_print_status(&block); /* New diagnostic admission is closed. */
}
static void test_retire(void) {
    reset();retire_during_attach=true;
    assert(!usb_storage_attach(&storage,&storage,bulk,true));
    assert(!retire_during_attach && !storage.initializing && storage.retiring && !storage.block);
    assert(usb_storage_retire(&storage));
    reset();assert(usb_storage_attach(&storage,&storage,bulk,true));
    block_read_fn saved_read=block.read;block_write_fn saved_write=block.write;
    block_flush_fn saved_flush=block.flush;uint64_t sectors=block.block_count;
    storage.busy=true;unsigned before=calls;uint8_t data[512];memset(data,0xcc,sizeof(data));
    assert(!usb_storage_retire(&storage) && storage.failed && block.offline && block.private_data==&storage);
    assert(!saved_read(&block,0,data,sizeof(data)) && calls==before);
    storage.busy=false;storage.diagnostic_users=1;
    assert(!usb_storage_retire(&storage) && block.read==saved_read);
    storage.diagnostic_users=0;
    assert(usb_storage_retire(&storage) && storage.retired && block.used && block.block_count==sectors);
    assert(!block.private_data && block.read==retired_read && block.write==retired_write && block.flush==retired_flush && !storage.context && !storage.bulk);
    for(unsigned i=0;i<4;i++) {
        assert(usb_storage_retire(&storage));
        assert(!saved_read(&block,0,data,sizeof(data)) && !saved_write(&block,0,data,sizeof(data)) && !saved_flush(&block));
        for(unsigned j=0;j<sizeof(data);j++)assert(data[j]==0xcc);
        assert(calls==before && irq_enabled);
    }
    assert(!usb_storage_enable_volume_writes(&block) && !usb_storage_disable_volume_writes(&block));
    reset();assert(usb_storage_attach(&storage,&storage,bulk,true));
    retired_diagnostics=0;storage.diagnose=retiring_diagnostic;
    usb_storage_print_status(&block);
    assert(retired_diagnostics==1 && !storage.diagnostic_users && !storage.retired && irq_enabled);
    assert(usb_storage_retire(&storage));usb_storage_print_status(&block);
    assert(retired_diagnostics==1 && irq_enabled);
    puts("usb-storage-retire: PASS busy/diagnostic references, offline tombstones and stale raw callbacks");
}
#include "test_usb_content.inc"
int main(void) {
    test_content_commands();
    test_retire();
    reset();assert(usb_storage_attach(&storage,&storage,bulk,true));
    unsigned attached_calls=calls;
    assert(!usb_storage_attach(&storage,&storage,bulk,true) && calls==attached_calls);
    test_runtime_status();
    test_transfer_timing();
    test_timing();
    test_timing_overlap();
    test_command_context();
    test_segment_faults();
    test_batches();
    reset(); gpt_found=1; assert(usb_storage_attach(&storage,&storage,bulk,false));
    assert(gpt_scans==1 && mbr_scans==0);
    reset(); tur_failures=2; assert(usb_storage_attach(&storage,&storage,bulk,false));
    assert(gpt_scans==1 && mbr_scans==1);
    assert(senses==2 && block.block_count==sizeof(disk)/512 && block.block_size==512 && !block.write && !block.flush);
    assert(storage.errors==2 && storage.error_command==4 && storage.error_opcode==0);
    assert(storage.error_phase==3 && storage.error_status==1 && !storage.failed);
    uint8_t buf[1024]; assert(block.read(&block,510,buf,1024));
    for(unsigned i=0;i<sizeof(buf);i++) assert(buf[i]==0x69);
    assert(irq_enabled && depth==0);
    boot=true; assert(block.read(&block,0,buf,512)); assert(!depth); boot=false;
    assert(usb_storage_is_device(&block) && !usb_storage_is_device(NULL));
    assert(usb_storage_enable_volume_writes(&block) && block.write && block.flush);
    alias.write=other.write=block.write;
    claim_busy=true;
    assert(!usb_storage_disable_volume_writes(&block) && block.write && alias.write);
    claim_busy=false;
    storage.busy=true; assert(!usb_storage_disable_volume_writes(&block) && block.write && alias.write);
    storage.busy=false; assert(usb_storage_disable_volume_writes(&block) && !block.write && !block.flush);
    assert(!alias.write && other.write);
    claim_busy=true;
    assert(!usb_storage_enable_volume_writes(&block) && !block.write && !block.flush);
    claim_busy=false;
    assert(usb_storage_enable_volume_writes(&block));
    struct block_device unrelated={0}; assert(!usb_storage_enable_volume_writes(&unrelated));
    reset(); assert(usb_storage_attach(&storage,&storage,bulk,true));
    memset(buf,0xab,sizeof(buf)); assert(block.write(&block,510,buf,sizeof(buf)));
    assert(writes==3 && disk[509]==0x69 && disk[1534]==0x69);
    for(unsigned i=510;i<1534;i++) assert(disk[i]==0xab);
    assert(block.flush(&block) && syncs==1);
    unsigned before=calls;
    assert(!block.read(&block,UINT64_MAX,buf,1)); assert(!block.write(&block,sizeof(disk)-1,buf,2));
    foreign=true; assert(!block.read(&block,0,buf,1)); foreign=false;
    storage.busy=true; assert(!block.read(&block,0,buf,1)); storage.busy=false;
    assert(calls==before && depth==0 && irq_enabled);
    reset(); empty=true; tur_failures=1;
    assert(!usb_storage_attach(&storage,&storage,bulk,false) && !block.used && !storage.failed && senses==1);
    const char *reasons[]={"", "cbw-transfer", "cbw-length", "data-length", "csw-signature",
        "csw-tag", "csw-residue", "csw-status", "csw-length", "data-length",
        "csw-transfer", "data-transfer", "csw-length"};
    for(unsigned f=1;f<=12;f++) {
        reset(); assert(usb_storage_attach(&storage,&storage,bulk,true)); fault=f;
        assert(!block.read(&block,0,buf,512) && storage.failed);
        assert(storage.error_command==storage.commands && storage.error_opcode==0x28 && storage.error_status==255);
        assert(storage.error_phase==(f<=2?1:(f==3||f==9)?4:f==11?2:3));
        assert(!strcmp(storage.error_reason,reasons[f]) && storage.error_expected_tag==storage.tag);
        if(f==5) assert(storage.error_tag==storage.tag+1);
        if(f==8) assert(storage.error_actual==12);
        if(f==12) assert(storage.error_actual==0);
        before=calls;
        storage.diagnose=diagnose;diagnosed=0;usb_storage_print_status(&block);
        assert(diagnosed==1 && calls==before);
        assert(!block.read(&block,512,buf,512) && !block.write(&block,0,buf,512) && !block.flush(&block));
        assert(calls==before && !depth && irq_enabled);
    }
    test_owner_status_window();
    assert(status_hooks && successful_status_hooks);
    puts("usb-storage-host: PASS (16 KiB commands / 4 KiB segments, 512-4096 byte sectors, BOT wrappers, short/malformed responses, quarantine, Unit Attention, empty media, unaligned I/O, bounds, ownership, flush, read-only)");
}

void latency_usb(uint64_t bytes,uint8_t opcode) { (void)bytes; (void)opcode; }

void scheduler_io_checkpoint(void) { assert(!depth || storage.busy);checkpoints++;clock_us+=checkpoint_delay; }

bool block_claim_available(struct block_device *d,struct block_device *except) { (void)d;(void)except;return !claim_busy; }

void block_invalidate(struct block_device *d) { assert(d==&block);d->offline=true; }
bool block_device_online(struct block_device *d) { return d && !block_parent(d)->offline; }
