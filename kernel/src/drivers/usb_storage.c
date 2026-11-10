/* USB-IF Bulk-Only Transport 1.0, SCSI transparent subclass, LUN 0.
 * No transport retries: an ambiguous write or halted endpoint quarantines the
 * device for this boot. xHCI retains DMA memory even after timeout/disconnect. */
#include <srvros/usb_storage.h>
#include <srvros/console.h>
#include <srvros/latency.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>

static uint64_t save_irq(void);
static void restore_irq(uint64_t flags);
static bool timing_active,aux_timing_active;
static struct usb_storage_timing timing,aux_timing;
static struct usb_storage_content_timing content_timing;
static bool content_active,content_command_active;
static struct usb_storage_command_timing content_command;
static bool content_command_peak;
static unsigned content_role=USB_STORAGE_CONTENT_ROLES-1,content_depth;
static unsigned content_command_role;
static void content_bad(void) {
    content_timing.valid=0;content_timing.invalid=1;
    if(content_command_active)content_command.valid=0;
}
static void content_add(uint64_t *sum,uint64_t value) {
    if(value>UINT64_MAX-*sum)content_bad();else *sum+=value;
}
bool usb_storage_content_begin(void) {
    uint64_t f=save_irq();
    bool ok=aux_timing_active && !content_active && !content_command_active;
    if(ok) {
        content_timing=(struct usb_storage_content_timing){.valid=1,.active=1};
        content_role=USB_STORAGE_CONTENT_ROLES-1;content_depth=0;content_active=true;
    }
    restore_irq(f);return ok;
}
void usb_storage_content_end(struct usb_storage_content_timing *out) {
    uint64_t f=save_irq();
    if(!content_active) {if(out)*out=(struct usb_storage_content_timing){0};restore_irq(f);return;}
    if(content_command_active) {
        content_bad();if(out)*out=content_timing;restore_irq(f);return;
    }
    if(content_depth || content_role!=USB_STORAGE_CONTENT_ROLES-1 || !aux_timing_active)content_bad();
    content_timing.active=0;if(out)*out=content_timing;
    content_active=false;content_depth=0;content_role=USB_STORAGE_CONTENT_ROLES-1;
    restore_irq(f);
}
unsigned usb_storage_content_role_begin(unsigned role) {
    uint64_t f=save_irq();unsigned previous=USB_STORAGE_CONTENT_NO_ROLE;
    if(content_active) {
        if(role>=USB_STORAGE_CONTENT_ROLES || content_command_active ||
           content_depth>=(USB_STORAGE_CONTENT_NO_ROLE>>3))content_bad();
        else {previous=(content_depth<<3)|content_role;content_depth++;content_role=role;}
    }
    restore_irq(f);return previous;
}
void usb_storage_content_role_end(unsigned previous) {
    if(previous==USB_STORAGE_CONTENT_NO_ROLE)return;
    uint64_t f=save_irq();
    if(content_active) {
        if(content_command_active || !content_depth || (previous>>3)!=content_depth-1 ||
           (previous&7u)>=USB_STORAGE_CONTENT_ROLES)content_bad();
        else {content_depth--;content_role=previous&7u;}
    }
    restore_irq(f);
}
/* Called only where the existing command is about to attempt its CBW. The
 * captured role cannot move until its original validation path returns. */
static bool content_command_begin(uint8_t opcode,uint32_t bytes) {
    if(!content_active)return false;
    if(content_command_active || !aux_timing_active) {content_bad();return false;}
    content_command_active=true;content_command_role=content_role;
    content_command=(struct usb_storage_command_timing){.valid=content_timing.valid,.role=content_role};
    content_command_peak=false;
    struct usb_storage_content_role *p=&content_timing.roles[content_command_role];
    if(opcode==0x28) {content_add(&p->read_commands,1);content_add(&p->read_bytes,bytes);}
    else if(opcode==0x2a) {
        content_add(&p->write_commands,1);content_add(&p->write_bytes,bytes);
        if(bytes==512)content_add(&p->write512_commands,1);
    }
    else if(opcode==0x35)content_add(&p->flush_commands,1);
    else content_add(&p->other_commands,1);
    return true;
}
static void content_command_end(bool owned,bool ok) {
    if(!owned || !content_command_active)return;
    if(!content_active) {content_bad();content_command_active=false;return;}
    if(!ok)content_add(&content_timing.roles[content_command_role].failed_commands,1);
    content_command.complete=1;content_command.ok=ok;
    if(content_command_peak)content_timing.command=content_command;
    content_command_active=false;
}
static void content_command_bulk(unsigned phase,uint64_t start,uint64_t end,
    uint32_t requested,uint32_t actual) {
    if(!content_active || !content_command_active)return;
    if(phase>2 || end<start || (content_command.end_us && start<content_command.end_us)) {
        content_bad();return;
    }
    struct usb_storage_command_phase *p=&content_command.phase[phase];
    if(!p->calls)p->first_us=start;
    content_add(&p->calls,1);content_add(&p->requested,requested);
    content_add(&p->actual,actual);content_add(&p->us,end-start);p->last_us=end;
    if(!content_command.start_us)content_command.start_us=start;
    content_command.end_us=end;
    if(phase==2) {
        content_command.csw_start_us=start;content_command.csw_end_us=end;
        const struct usb_storage_command_timing *old=&content_timing.command;
        content_command_peak=!old->complete || end-start>old->csw_end_us-old->csw_start_us;
    }
}
bool usb_storage_timing_enabled(void) { return timing_active || aux_timing_active || content_active; }
bool usb_storage_timing_begin(void) {
    uint64_t flags=save_irq();
    bool ok=!timing_active && timer_reference_ready();
    if(ok) { timing=(struct usb_storage_timing){.valid=true};timing_active=true; }
    restore_irq(flags);return ok;
}
void usb_storage_timing_end(struct usb_storage_timing *out) {
    uint64_t flags=save_irq();
    if(out) *out=timing_active?timing:(struct usb_storage_timing){0};
    timing_active=false;restore_irq(flags);
}
/* Independent reserved observer: never resets or ends the legacy logger's
 * window. This is diagnostic storage, not a second hardware request/queue. */
bool usb_storage_timing_aux_begin(void) {
    uint64_t flags=save_irq();bool ok=!aux_timing_active && timer_reference_ready();
    if(ok){aux_timing=(struct usb_storage_timing){.valid=true};aux_timing_active=true;}
    restore_irq(flags);return ok;
}
void usb_storage_timing_aux_end(struct usb_storage_timing *out) {
    uint64_t flags=save_irq();
    if(content_active)content_bad();
    if(out)*out=aux_timing_active?aux_timing:(struct usb_storage_timing){0};
    aux_timing_active=false;restore_irq(flags);
}
static void timing_add(struct usb_storage_timing *target,uint64_t *sum,uint64_t value) {
    if(value>UINT64_MAX-*sum) target->valid=false;
    else *sum+=value;
}
static void timing_max(uint64_t *maximum,uint64_t value) {
    if(value>*maximum)*maximum=value;
}
static void record_bulk(struct usb_storage_timing *target,unsigned phase,uint64_t start,
    uint64_t end,bool ok,bool has_sample,const struct usb_bulk_timing *captured) {
    timing_add(target,&target->calls[phase],1);
    if(!ok) timing_add(target,&target->failures,1);
    if(end<start) { target->valid=false;return; }
    uint64_t elapsed=end-start;
    struct usb_storage_phase_timing *p=&target->phase[phase];
    timing_add(target,&target->us[phase],elapsed);timing_max(&p->max_us,elapsed);
    if(elapsed>20000)timing_add(target,&p->over20ms,1);
    if(elapsed>100000)timing_add(target,&p->over100ms,1);
    if(elapsed>1000000)timing_add(target,&p->over1s,1);
    if(has_sample) {
        struct usb_bulk_timing sample=*captured;
        if(sample.seen) {
            if(sample.submitted<start || sample.observed<sample.submitted ||
               sample.resumed<sample.observed || sample.resumed>end)target->valid=false;
            else {
                uint64_t observe=sample.observed-sample.submitted,resume=sample.resumed-sample.observed;
                timing_add(target,&p->observed_calls,1);
                timing_add(target,&p->observe_us,observe);timing_add(target,&p->resume_us,resume);
                if(!sample.service_valid || !sample.service_checks ||
                   sample.service_samples!=(sample.service_checks-1)/256+1 ||
                   sample.service_gap_max_us>observe || sample.completion_gap_us>sample.service_gap_max_us ||
                   observe/(sample.service_samples+1)+(observe%(sample.service_samples+1)!=0)>sample.service_gap_max_us)target->valid=false;
                else {
                    timing_add(target,&p->service_checks,sample.service_checks);
                    timing_add(target,&p->service_samples,sample.service_samples);
                    timing_max(&p->service_gap_max_us,sample.service_gap_max_us);
                    timing_max(&p->completion_gap_max_us,sample.completion_gap_us);
                }
                timing_max(&p->observe_max_us,observe);timing_max(&p->resume_max_us,resume);
            }
        } else if(ok) target->valid=false; /* An instrumented success must have an event. */
    }
}
static bool timed_bulk(struct usb_storage *s,unsigned phase,bool in,void *data,
                       uint32_t length,uint32_t *actual,uint8_t opcode,uint32_t command_bytes) {
    bool legacy=timing_active,auxiliary=aux_timing_active;
    if(!legacy && !auxiliary && !content_active)return s->bulk(s->context,in,data,length,actual);
    uint64_t start=timer_monotonic_us();
    bool ok=s->bulk(s->context,in,data,length,actual);
    uint64_t end=timer_monotonic_us();
    struct usb_bulk_timing sample={0};
    if(s->transfer_timing)s->transfer_timing(s->context,&sample);
    content_command_bulk(phase,start,end,length,*actual);
    /* Capture once, then copy the same observation into independent windows.
     * No additional bulk/controller request or event-service operation. */
    if(legacy)record_bulk(&timing,phase,start,end,ok,s->transfer_timing!=0,&sample);
    if(auxiliary) {
        bool peak=phase==2 && (!aux_timing.calls[2] ||
            (end>=start && end-start>aux_timing.phase[2].max_us));
        record_bulk(&aux_timing,phase,start,end,ok,s->transfer_timing!=0,&sample);
        if(peak)aux_timing.csw=(struct usb_storage_csw_timing){
            .start_us=start,.end_us=end,.opcode=opcode,.command_bytes=command_bytes,
            .request_bytes=length,.actual=*actual,.ok=ok,.sample=sample};
    }
    if(content_active && phase==2) {
        if(!content_command_active || !auxiliary || end<start)content_bad();
        else {
            struct usb_storage_content_role *p=&content_timing.roles[content_command_role];
            if(!aux_timing.valid)content_bad();
            uint64_t elapsed=end-start;
            bool peak=!p->csw_calls || elapsed>p->worst.end_us-p->worst.start_us;
            content_add(&p->csw_calls,1);content_add(&p->csw_us,elapsed);
            if(elapsed>100000)content_add(&p->over100ms,1);
            if(opcode==0x2a) {
                content_add(&p->write_csw_us,elapsed);
                if(command_bytes==512)content_add(&p->write512_csw_us,elapsed);
                if(elapsed>100000) {
                    content_add(&p->write_over100ms,1);
                    if(command_bytes==512)content_add(&p->write512_over100ms,1);
                }
            }
            if(peak)p->worst=(struct usb_storage_csw_timing){
                .start_us=start,.end_us=end,.opcode=opcode,.command_bytes=command_bytes,
                .request_bytes=length,.actual=*actual,.ok=ok,.sample=sample};
        }
    }
    return ok;
}

/* Observe the existing checkpoint once; no extra yield or eligibility change.
 * Disjoint from bulk callback intervals, so this accounts for part of the
 * residual phase wall time without treating it as CPU execution. */
static void timed_checkpoint(void) {
    bool active=aux_timing_active;
    if(!active){scheduler_io_checkpoint();return;}
    uint64_t start=timer_monotonic_us();
    scheduler_io_checkpoint();
    uint64_t end=timer_monotonic_us();
    timing_add(&aux_timing,&aux_timing.checkpoint_calls,1);
    if(end<start)aux_timing.valid=false;
    else timing_add(&aux_timing,&aux_timing.checkpoint_us,end-start);
    if(content_active && content_command_active) {
        if(end<start || start<content_command.end_us)content_bad();
        else {
            if(!content_command.checkpoint_calls)content_command.checkpoint_first_us=start;
            content_add(&content_command.checkpoint_calls,1);
            content_add(&content_command.checkpoint_us,end-start);
            content_command.checkpoint_last_us=end;content_command.end_us=end;
        }
    }
}

static uint32_t le32(const uint8_t *p) {
    return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void put_le32(uint8_t *p, uint32_t n) {
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(n>>(i*8));
}
static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static void put_be32(uint8_t *p, uint32_t n) {
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(n>>((3-i)*8));
}
static void copy(void *out, const void *in, uint32_t n) {
    for (uint32_t i=0;i<n;i++) ((uint8_t *)out)[i]=((const uint8_t *)in)[i];
}
static void record_error(struct usb_storage *s, uint8_t opcode, uint8_t phase,
    const char *reason, uint32_t tag, uint32_t actual, const uint8_t csw[13]) {
    s->error_command=s->commands; s->error_opcode=opcode; s->error_phase=phase;
    s->error_reason=reason; s->error_expected_tag=tag; s->error_actual=actual;
    s->error_signature=le32(csw); s->error_tag=le32(csw+4);
    s->error_residue=le32(csw+8); s->error_csw_status=csw[12];
}
/* 0 success, 1 SCSI command failure (REQUEST SENSE allowed), -1 transport
 * failure. Validate the wrapper before trusting any returned data. */
static int command(struct usb_storage *s, const uint8_t *cdb, uint8_t cdb_len,
    bool in, void *data, uint32_t length) {
    if (s->failed || cdb_len==0 || cdb_len>16 || length>sizeof(s->data)) return -1;
    uint8_t cbw[31]={0}, csw[13]={0};
    uint32_t actual=0, transferred=0, tag=++s->tag;
    put_le32(cbw,0x43425355); put_le32(cbw+4,tag); put_le32(cbw+8,length);
    cbw[12]=in?0x80:0; cbw[14]=cdb_len; copy(cbw+15,cdb,cdb_len);
    s->commands++;
    bool content_observed=content_command_begin(cdb[0],length);
    latency_usb(length,cdb[0]);
    uint8_t phase = 1; /* CBW, data, CSW, then full-transfer validation. */
    const char *reason = "cbw-transfer";
    if (!timed_bulk(s,0,false,cbw,sizeof(cbw),&actual,cdb[0],length)) goto bad;
    reason="cbw-length";
    if (actual!=sizeof(cbw)) goto bad;
    phase = 2;
    /* One BOT data phase may span several controller requests. Keep each
     * request within the existing pinned 4 KiB DMA buffer; no intermediate
     * short/zero packet or new command wrapper terminates the phase. */
    while (transferred<length) {
        uint32_t chunk=length-transferred;
        if(chunk>USB_STORAGE_BULK_BYTES)chunk=USB_STORAGE_BULK_BYTES;
        actual=0;reason="data-transfer";
        if(!timed_bulk(s,1,in,(uint8_t *)data+transferred,chunk,&actual,cdb[0],length)) {
            if(actual<=chunk)transferred+=actual;
            goto bad;
        }
        reason="data-overrun";
        if(actual>chunk)goto bad;
        transferred+=actual;
        /* A short data transfer ends the phase. Do not consume its CSW as
         * another data chunk; the existing status/residue checks decide it. */
        if(actual<chunk)break;
        if(transferred<length)timed_checkpoint();
    }
    phase = 3;
    actual=0; reason="csw-transfer";
    if (!timed_bulk(s,2,true,csw,sizeof(csw),&actual,cdb[0],length)) goto bad;
    reason="csw-length"; if (actual!=sizeof(csw)) goto bad;
    reason="csw-signature"; if (le32(csw)!=0x53425355) goto bad;
    reason="csw-tag"; if (le32(csw+4)!=tag) goto bad;
    reason="csw-residue"; if (le32(csw+8)>length) goto bad;
    reason="csw-status"; if (csw[12]>1) goto bad;
    if (csw[12]==1) {
        s->errors++; record_error(s,cdb[0],phase,"scsi-command",tag,actual,csw);
        s->error_status=1;
        /* Startup may clear Unit Attention before publication. A failed command
         * after publication cannot establish whether the medium changed. Never
         * resume old geometry/mount/cache state against a replacement medium. */
        if(s->block) {s->failed=true;block_invalidate(s->block);}
        content_command_end(content_observed,false);
        return 1;
    }
    /* All implemented successful commands require their full requested data. */
    phase = 4;
    reason="data-length";
    if (le32(csw+8)!=0 || transferred!=length) goto bad;
    timed_checkpoint();
    content_command_end(content_observed,true);
    return 0;
bad:
    s->failed=true; s->errors++;
    if(s->block)block_invalidate(s->block);
    record_error(s,cdb[0],phase,reason,tag,(phase==2 || phase==4)?transferred:actual,csw);
    s->error_status=255; /* No trustworthy command status. */
    console_printf("usb-storage: transport failed; device quarantined commands=%u\n",s->commands);
    content_command_end(content_observed,false);
    return -1;
}
static uint64_t save_irq(void) {
#ifdef SRVROS_USB_STORAGE_HOST_TEST
    return usb_test_irq_save();
#else
    uint64_t flags; __asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory"); return flags;
#endif
}
static void restore_irq(uint64_t flags) {
#ifdef SRVROS_USB_STORAGE_HOST_TEST
    usb_test_irq_restore(flags);
#else
    if(flags & (1ull<<9)) __asm__ volatile("sti":::"memory");
#endif
}
static bool begin(struct usb_storage *s, uint64_t *flags) {
    *flags=save_irq();
    if(s->failed || s->busy) { restore_irq(*flags); return false; }
    s->services=scheduler_io_begin();
    if(!s->services && scheduler_io_active()) { restore_irq(*flags); return false; }
    s->busy=true; return true;
}
static void end(struct usb_storage *s, uint64_t flags) {
    s->busy=false; if(s->services) scheduler_io_end(); restore_irq(flags);
}
static bool sectors(struct usb_storage *s, uint32_t lba, uint32_t count, bool write) {
    uint8_t cdb[10]={write?0x2a:0x28,0,0,0,0,0,0,0,0,0};
    if (!count || count>sizeof(s->data)/s->sector_size) return false;
    put_be32(cdb+2,lba);
    cdb[7]=(uint8_t)(count>>8); cdb[8]=(uint8_t)count;
    return command(s,cdb,10,!write,s->data,count*s->sector_size)==0;
}
static bool transfer(struct block_device *d, uint64_t offset, void *buffer, uint64_t length, bool write) {
    struct usb_storage *s=d?d->private_data:NULL;
    if(!s)return false;
    uint64_t size=s->sectors*s->sector_size, flags;
    if (!buffer || offset>size || length>size-offset || !begin(s,&flags)) return false;
    bool ok=true;
    while(length) {
        uint32_t within=offset%s->sector_size, n=s->sector_size-within;
        if (!within && length>=s->sector_size) {
            n=length>sizeof(s->data)?sizeof(s->data):(uint32_t)length;
            n-=n%s->sector_size;
        }
        if(n>length) n=(uint32_t)length;
        uint32_t lba=(uint32_t)(offset/s->sector_size);
        uint32_t count=(within+n+s->sector_size-1)/s->sector_size;
        if ((!write || within || n<s->sector_size) && !sectors(s,lba,count,false)) { ok=false; break; }
        if(write) {
            copy(s->data+within,buffer,n);
            if(!sectors(s,lba,count,true)) { ok=false; break; }
        } else copy(buffer,s->data+within,n);
        buffer=(uint8_t *)buffer+n; offset+=n; length-=n;
    }
    end(s,flags); return ok;
}
static bool read_blocks(struct block_device *d,uint64_t off,void *buf,uint64_t len) {
    return transfer(d,off,buf,len,false);
}
static bool write_blocks(struct block_device *d,uint64_t off,const void *buf,uint64_t len) {
    return transfer(d,off,(void *)buf,len,true);
}
static bool flush(struct block_device *d) {
    struct usb_storage *s=d?d->private_data:NULL; uint64_t flags;
    if(!s)return false;
    if(!begin(s,&flags)) return false;
    uint8_t cdb[10]={0x35};
    bool ok=command(s,cdb,10,false,0,0)==0;
    end(s,flags); return ok;
}
/* Preserve failure/durability visibility without retaining a transport context. */
static bool retired_read(struct block_device *d,uint64_t off,void *data,uint64_t len) {
    (void)d;(void)off;(void)data;(void)len;return false;
}
static bool retired_write(struct block_device *d,uint64_t off,const void *data,uint64_t len) {
    (void)d;(void)off;(void)data;(void)len;return false;
}
static bool retired_flush(struct block_device *d) { (void)d;return false; }
bool usb_storage_retire(struct usb_storage *s) {
    if(!s)return false;
    uint64_t flags=save_irq();
    if(s->block && s->block->private_data!=s && !(s->retired && !s->block->private_data)) {restore_irq(flags);return false;}
    s->retiring=s->failed=true;if(s->block)block_invalidate(s->block);
    if(s->initializing || s->busy || s->diagnostic_users) {restore_irq(flags);return false;}
    if(s->block) {
        s->block->read=retired_read;
        if(s->block->write)s->block->write=retired_write;
        if(s->block->flush)s->block->flush=retired_flush;
        s->block->private_data=NULL;
    }
    s->bulk=NULL;s->context=NULL;s->diagnose=NULL;s->transfer_timing=NULL;s->retired=true;
    restore_irq(flags);return true;
}
bool usb_storage_is_device(struct block_device *device) {
    struct block_device *parent = block_parent(device);
    return parent && (parent->read==read_blocks || parent->read==retired_read);
}

void usb_storage_print_status(struct block_device *device) {
    uint64_t flags=save_irq();
    if(!usb_storage_is_device(device)) {restore_irq(flags);return;}
    struct usb_storage *s=block_parent(device)->private_data;
    if(!s || s->retiring || s->retired || s->diagnostic_users==~0u) {restore_irq(flags);return;}
    s->diagnostic_users++;restore_irq(flags);
    static const char *phases[] = {"none", "cbw", "data", "csw", "length"};
    console_printf("usb-storage: commands=%u errors=%u quarantined=%u busy=%u\n",
        s->commands, s->errors, (uint64_t)s->failed, (uint64_t)s->busy);
    console_printf("usb-storage: last-error-command=%u opcode=%u phase=%s status=%u\n",
        s->error_command, (uint64_t)s->error_opcode,
        s->error_phase < 5 ? phases[s->error_phase] : "unknown", (uint64_t)s->error_status);
    console_printf("usb-storage: reason=%s actual=%u expected-tag=%u\n",
        s->error_reason?s->error_reason:"none",(uint64_t)s->error_actual,(uint64_t)s->error_expected_tag);
    console_printf("usb-storage: raw-csw signature=%x tag=%u residue=%u status=%u\n",
        (uint64_t)s->error_signature,(uint64_t)s->error_tag,(uint64_t)s->error_residue,(uint64_t)s->error_csw_status);
    if (s->diagnose) s->diagnose(s->context);
    flags=save_irq();s->diagnostic_users--;restore_irq(flags);
}

bool usb_storage_enable_volume_writes(struct block_device *device) {
    uint64_t flags=save_irq();
    if(!usb_storage_is_device(device)) {restore_irq(flags);return false;}
    struct block_device *parent = block_parent(device);
    struct usb_storage *storage = parent->private_data;
    if(!storage || storage->failed || storage->busy || !block_device_online(device) || !block_claim_available(parent,device)) {restore_irq(flags);return false;}
    parent->write = write_blocks;
    parent->flush = flush;
    bool ok=block_enable_partition_writes(device);restore_irq(flags);return ok;
}

bool usb_storage_disable_volume_writes(struct block_device *device) {
    uint64_t flags=save_irq();
    if(!usb_storage_is_device(device)) {restore_irq(flags);return false;}
    struct block_device *parent = block_parent(device);
    struct usb_storage *storage = parent->private_data;
    if (!storage || storage->busy || !block_claim_available(parent,device)) { restore_irq(flags); return false; }
    /* Revoke every alias, including the raw disk, before returning to the monitor. */
    for (uint64_t i = 0; i < BLOCK_MAX_DEVICES; i++) {
        struct block_device *alias = block_device_at(i);
        if (alias && block_parent(alias) == parent) alias->write = NULL;
    }
    parent->write = NULL;
    parent->flush = NULL;
    restore_irq(flags);
    return true;
}

static bool attach_storage(struct usb_storage *s,void *context,usb_bulk_fn bulk,bool writable) {
    /* Published callbacks, geometry and aliases belong to this boot lifetime. */
    if(!s || !bulk || s->block || s->failed || s->busy)return false;
    s->context=context; s->bulk=bulk;
    uint8_t inquiry[6]={0x12,0,0,0,36,0};
    if(command(s,inquiry,6,true,s->data,36)!=0 || (s->data[0]&0x1f)!=0 || (s->data[0]>>5)!=0) return false;
    /* Clear a bounded number of Unit Attention responses. Empty readers are
     * not disks; avoid issuing READ CAPACITY's data phase on absent media. */
    bool ready=false;
    for(unsigned i=0;i<3;i++) {
        uint8_t tur[6]={0}; int result=command(s,tur,6,false,0,0);
        if(result==0) { ready=true; break; }
        if(result<0) return false;
        uint8_t sense[6]={3,0,0,0,18,0};
        if(command(s,sense,6,true,s->data,18)!=0) return false;
        if ((s->data[0]&0x7f)!=0x70 && (s->data[0]&0x7f)!=0x71) return false;
        if((s->data[2]&15)!=6) return false;
    }
    if(!ready) return false;
    uint8_t capacity[10]={0x25};
    if(command(s,capacity,10,true,s->data,8)!=0) return false;
    uint32_t last=be32(s->data), bytes=be32(s->data+4);
    /* READ(10)/WRITE(10) only, sector sizes supported by the block cache. */
    if(last==UINT32_MAX || bytes<512 || bytes>4096 || (bytes&(bytes-1))) return false;
    s->sectors=(uint64_t)last+1; s->sector_size=bytes;
    for(unsigned i=0;i<BLOCK_MAX_DEVICES;i++) {
        s->name[0]='u'; s->name[1]='s'; s->name[2]='b';
        unsigned pos=3; if(i>=10) s->name[pos++]='0'+i/10;
        s->name[pos++]='0'+i%10; s->name[pos]=0;
        if(!block_find(s->name)) break;
    }
    s->block=block_register(s->name,bytes,s->sectors,read_blocks,writable?write_blocks:0,s);
    if(!s->block) return false;
    s->block->max_transfer_bytes=sizeof(s->data);
    if(writable) s->block->flush=flush;
    console_printf("usb-storage: %s blocks=%u sector=%u mode=%s\n",s->name,s->sectors,(uint64_t)bytes,writable?"rw":"read-only");
    if (!block_register_gpt_partitions(s->block)) block_register_mbr_partitions(s->block);
    return true;
}

/* Startup may perform block callbacks while scanning partitions, so its owner
 * is separate from the ordinary per-request busy gate. */
bool usb_storage_attach(struct usb_storage *s,void *context,usb_bulk_fn bulk,bool writable) {
    uint64_t flags=save_irq();
    if(!s || !bulk || s->block || s->failed || s->busy || s->initializing || s->retiring) {
        restore_irq(flags);return false;
    }
    s->initializing=true;restore_irq(flags);
    bool ok=attach_storage(s,context,bulk,writable);
    flags=save_irq();
    if(s->retiring && s->block)block_invalidate(s->block);
    s->initializing=false;ok=ok && !s->retiring;
    restore_irq(flags);return ok;
}
