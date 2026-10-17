/* USB-IF Bulk-Only Transport 1.0, SCSI transparent subclass, LUN 0.
 * No transport retries: an ambiguous write or halted endpoint quarantines the
 * device for this boot. xHCI retains DMA memory even after timeout/disconnect. */
#include <srvros/usb_storage.h>
#include <srvros/console.h>
#include <srvros/scheduler.h>

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
    uint8_t phase = 1; /* CBW, data, CSW, then full-transfer validation. */
    const char *reason = "cbw-transfer";
    if (!s->bulk(s->context,false,cbw,sizeof(cbw),&actual)) goto bad;
    reason="cbw-length";
    if (actual!=sizeof(cbw)) goto bad;
    phase = 2;
    reason="data-transfer";
    if (length && !s->bulk(s->context,in,data,length,&transferred)) goto bad;
    reason="data-overrun";
    if (transferred>length) goto bad;
    phase = 3;
    actual=0; reason="csw-transfer";
    if (!s->bulk(s->context,true,csw,sizeof(csw),&actual)) goto bad;
    reason="csw-length"; if (actual!=sizeof(csw)) goto bad;
    reason="csw-signature"; if (le32(csw)!=0x53425355) goto bad;
    reason="csw-tag"; if (le32(csw+4)!=tag) goto bad;
    reason="csw-residue"; if (le32(csw+8)>length) goto bad;
    reason="csw-status"; if (csw[12]>1) goto bad;
    if (csw[12]==1) {
        s->errors++; record_error(s,cdb[0],phase,"scsi-command",tag,actual,csw);
        s->error_status=1;
        return 1;
    }
    /* All implemented successful commands require their full requested data. */
    phase = 4;
    reason="data-length";
    if (le32(csw+8)!=0 || transferred!=length) goto bad;
    return 0;
bad:
    s->failed=true; s->errors++;
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
static bool sector(struct usb_storage *s, uint32_t lba, bool write) {
    uint8_t cdb[10]={write?0x2a:0x28,0,0,0,0,0,0,0,1,0};
    put_be32(cdb+2,lba);
    return command(s,cdb,10,!write,s->data,s->sector_size)==0;
}
static bool transfer(struct block_device *d, uint64_t offset, void *buffer, uint64_t length, bool write) {
    struct usb_storage *s=d->private_data;
    uint64_t size=s->sectors*s->sector_size, flags;
    if (!buffer || offset>size || length>size-offset || !begin(s,&flags)) return false;
    bool ok=true;
    while(length) {
        uint32_t within=offset%s->sector_size, n=s->sector_size-within;
        if(n>length) n=(uint32_t)length;
        uint32_t lba=(uint32_t)(offset/s->sector_size);
        if ((!write || within || n<s->sector_size) && !sector(s,lba,false)) { ok=false; break; }
        if(write) {
            copy(s->data+within,buffer,n);
            if(!sector(s,lba,true)) { ok=false; break; }
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
    if (storage->failed || storage->busy) return false;
    parent->write = write_blocks;
    parent->flush = flush;
    return block_enable_partition_writes(device);
}

bool usb_storage_disable_volume_writes(struct block_device *device) {
    if (!usb_storage_is_device(device)) return false;
    struct block_device *parent = block_parent(device);
    struct usb_storage *storage = parent->private_data;
    uint64_t flags = save_irq();
    if (storage->busy) { restore_irq(flags); return false; }
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
    if(writable) s->block->flush=flush;
    console_printf("usb-storage: %s blocks=%u sector=%u mode=%s\n",s->name,s->sectors,(uint64_t)bytes,writable?"rw":"read-only");
    block_register_mbr_partitions(s->block);
    return true;
}
