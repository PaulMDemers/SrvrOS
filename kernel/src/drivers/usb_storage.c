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
static bool timing_active;
static struct usb_storage_timing timing;
bool usb_storage_timing_enabled(void) { return timing_active; }
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
static void timing_add(uint64_t *sum,uint64_t value) {
    if(value>UINT64_MAX-*sum) timing.valid=false;
    else *sum+=value;
}
static void timing_max(uint64_t *maximum,uint64_t value) {
    if(value>*maximum)*maximum=value;
}
static bool timed_bulk(struct usb_storage *s,unsigned phase,bool in,void *data,
                       uint32_t length,uint32_t *actual) {
    if(!timing_active) return s->bulk(s->context,in,data,length,actual);
    uint64_t start=timer_monotonic_us();
    bool ok=s->bulk(s->context,in,data,length,actual);
    uint64_t end=timer_monotonic_us();
    timing_add(&timing.calls[phase],1);
    if(!ok) timing_add(&timing.failures,1);
    if(end<start) { timing.valid=false;return ok; }
    uint64_t elapsed=end-start;
    struct usb_storage_phase_timing *p=&timing.phase[phase];
    timing_add(&timing.us[phase],elapsed);timing_max(&p->max_us,elapsed);
    if(elapsed>20000)timing_add(&p->over20ms,1);
    if(elapsed>100000)timing_add(&p->over100ms,1);
    if(elapsed>1000000)timing_add(&p->over1s,1);
    if(s->transfer_timing) {
        struct usb_bulk_timing sample={0};s->transfer_timing(s->context,&sample);
        if(sample.seen) {
            if(sample.submitted<start || sample.observed<sample.submitted ||
               sample.resumed<sample.observed || sample.resumed>end)timing.valid=false;
            else {
                uint64_t observe=sample.observed-sample.submitted,resume=sample.resumed-sample.observed;
                timing_add(&p->observed_calls,1);
                timing_add(&p->observe_us,observe);timing_add(&p->resume_us,resume);
                if(!sample.service_valid || !sample.service_checks ||
                   sample.service_samples!=(sample.service_checks-1)/256+1 ||
                   sample.service_gap_max_us>observe || sample.completion_gap_us>sample.service_gap_max_us ||
                   observe/(sample.service_samples+1)+(observe%(sample.service_samples+1)!=0)>sample.service_gap_max_us)timing.valid=false;
                else {
                    timing_add(&p->service_checks,sample.service_checks);
                    timing_add(&p->service_samples,sample.service_samples);
                    timing_max(&p->service_gap_max_us,sample.service_gap_max_us);
                    timing_max(&p->completion_gap_max_us,sample.completion_gap_us);
                }
                timing_max(&p->observe_max_us,observe);timing_max(&p->resume_max_us,resume);
            }
        } else if(ok) timing.valid=false; /* An instrumented success must have an event. */
    }
    return ok;
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
    latency_usb(length,cdb[0]);
    uint8_t phase = 1; /* CBW, data, CSW, then full-transfer validation. */
    const char *reason = "cbw-transfer";
    if (!timed_bulk(s,0,false,cbw,sizeof(cbw),&actual)) goto bad;
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
        if(!timed_bulk(s,1,in,(uint8_t *)data+transferred,chunk,&actual)) {
            if(actual<=chunk)transferred+=actual;
            goto bad;
        }
        reason="data-overrun";
        if(actual>chunk)goto bad;
        transferred+=actual;
        /* A short data transfer ends the phase. Do not consume its CSW as
         * another data chunk; the existing status/residue checks decide it. */
        if(actual<chunk)break;
        if(transferred<length)scheduler_io_checkpoint();
    }
    phase = 3;
    actual=0; reason="csw-transfer";
    if (!timed_bulk(s,2,true,csw,sizeof(csw),&actual)) goto bad;
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
        return 1;
    }
    /* All implemented successful commands require their full requested data. */
    phase = 4;
    reason="data-length";
    if (le32(csw+8)!=0 || transferred!=length) goto bad;
    scheduler_io_checkpoint();
    return 0;
bad:
    s->failed=true; s->errors++;
    if(s->block)block_invalidate(s->block);
    record_error(s,cdb[0],phase,reason,tag,(phase==2 || phase==4)?transferred:actual,csw);
    s->error_status=255; /* No trustworthy command status. */
    console_printf("usb-storage: transport failed; device quarantined commands=%u\n",s->commands);
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
    struct usb_storage *s=d->private_data;
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
    struct usb_storage *s=d->private_data; uint64_t flags;
    if(!begin(s,&flags)) return false;
    uint8_t cdb[10]={0x35};
    bool ok=command(s,cdb,10,false,0,0)==0;
    end(s,flags); return ok;
}
bool usb_storage_is_device(struct block_device *device) {
    struct block_device *parent = block_parent(device);
    return parent && parent->read == read_blocks;
}

void usb_storage_print_status(struct block_device *device) {
    if (!usb_storage_is_device(device)) return;
    struct usb_storage *s = block_parent(device)->private_data;
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
}

bool usb_storage_enable_volume_writes(struct block_device *device) {
    if (!usb_storage_is_device(device)) return false;
    struct block_device *parent = block_parent(device);
    struct usb_storage *storage = parent->private_data;
    if (storage->failed || storage->busy || !block_device_online(device) || !block_claim_available(parent,device)) return false;
    parent->write = write_blocks;
    parent->flush = flush;
    return block_enable_partition_writes(device);
}

bool usb_storage_disable_volume_writes(struct block_device *device) {
    if (!usb_storage_is_device(device)) return false;
    struct block_device *parent = block_parent(device);
    struct usb_storage *storage = parent->private_data;
    uint64_t flags = save_irq();
    if (storage->busy || !block_claim_available(parent,device)) { restore_irq(flags); return false; }
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

bool usb_storage_attach(struct usb_storage *s,void *context,usb_bulk_fn bulk,bool writable) {
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
