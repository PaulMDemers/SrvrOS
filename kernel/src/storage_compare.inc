#include <srvros/workspace_log.h>
/* Explicit isolated control. Existing filesystem/transport limits are unchanged. */
#define STORAGE_COMPARE_BYTES 32768u
#define STORAGE_COMPARE_CHUNK 16384u
#define STORAGE_COMPARE_SAMPLES 12u
#define STORAGE_COMPARE_LIMIT_US 180000000u
struct storage_compare_sample {
    uint64_t sequence,kind,start,end,write_us,flush_us,read_us,crc,offset,ok,retired;
    struct usb_storage_timing usb,flush_io,read_io;
    struct usb_storage_content_timing content;
};
static uint8_t compare_payload[STORAGE_COMPARE_BYTES],compare_read[STORAGE_COMPARE_CHUNK];
static struct storage_compare_sample compare_samples[STORAGE_COMPARE_SAMPLES];
static bool compare_ran;
static uint32_t compare_crc(const uint8_t *p,unsigned size) {
    uint32_t crc=UINT32_MAX;
    for(unsigned i=0;i<size;i++) {
        crc^=p[i];for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
    }
    return ~crc;
}
static void compare_pattern(unsigned sequence) {
    for(unsigned i=0;i<STORAGE_COMPARE_BYTES;i++)compare_payload[i]=(uint8_t)(i*37u+(i>>8)*17u+sequence*29u+11u);
}
static bool compare_file_read(const char *path,struct block_device *parent) {
    block_forget(parent); /* Never validate against a clean block-cache alias. */
    const struct vfs_node *node=vfs_lookup(path);const uint8_t *data=NULL;uint64_t size=0;
    bool ok=node && node->type==VFS_NODE_FILE && vfs_read_all(node,&data,&size) && size==STORAGE_COMPARE_BYTES;
    for(uint64_t i=0;ok && i<size;i++)if(data[i]!=compare_payload[i])ok=false;
    if(node && data)vfs_release_data(node,data);
    return ok;
}
static bool compare_trace_begin(void) {
    if(!usb_storage_timing_aux_begin())return false;
    if(usb_storage_content_begin())return true;
    struct usb_storage_timing discarded;usb_storage_timing_aux_end(&discarded);return false;
}
static bool compare_trace_end(struct storage_compare_sample *r) {
    usb_storage_content_end(&r->content);usb_storage_timing_aux_end(&r->usb);
    const struct usb_storage_command_timing *q=&r->content.command;
    const struct usb_storage_csw_timing *c=&r->usb.csw;
    return r->usb.valid && !r->usb.failures && r->content.valid && !r->content.active &&
        !r->content.invalid && q->valid && q->complete && q->ok && q->role<USB_STORAGE_CONTENT_ROLES &&
        q->csw_start_us==c->start_us && q->csw_end_us==c->end_us;
}
static void compare_emit(const struct storage_compare_sample *r) {
    const struct usb_storage_timing *u=&r->usb;
    console_printf("storage-only-sample: version=1 seq=%u kind=%u start_us=%u end_us=%u bytes=32768 write_us=%u flush_us=%u read_us=%u crc=%u offset=%u retired=%u cbws=%u cbw_us=%u data_calls=%u data_us=%u csws=%u csw_us=%u failures=%u valid=%u flush_csws=%u flush_valid=%u read_csws=%u read_valid=%u ok=%u\n",
        r->sequence,r->kind,r->start,r->end,r->write_us,r->flush_us,r->read_us,r->crc,r->offset,r->retired,
        u->calls[0],u->us[0],u->calls[1],u->us[1],u->calls[2],u->us[2],u->failures,(uint64_t)u->valid,r->flush_io.calls[2],(uint64_t)(r->flush_io.valid&&!r->flush_io.failures),r->read_io.calls[2],(uint64_t)(r->read_io.valid&&!r->read_io.failures),r->ok);
    const struct usb_storage_command_timing *q=&r->content.command;
    const struct usb_storage_csw_timing *c=&u->csw;
    console_printf("storage-only-command: version=1 seq=%u opcode=%u bytes=%u valid=%u complete=%u ok=%u role=%u start_us=%u end_us=%u csw_start=%u csw_end=%u cbw_calls=%u cbw_us=%u data_calls=%u data_req=%u data_actual=%u data_us=%u csw_calls=%u csw_us=%u ck_calls=%u ck_us=%u\n",
        r->sequence,c->opcode,c->command_bytes,q->valid,q->complete,q->ok,q->role,q->start_us,q->end_us,q->csw_start_us,q->csw_end_us,
        q->phase[0].calls,q->phase[0].us,q->phase[1].calls,q->phase[1].requested,q->phase[1].actual,q->phase[1].us,q->phase[2].calls,q->phase[2].us,q->checkpoint_calls,q->checkpoint_us);
    const struct usb_bulk_timing *b=&c->sample;
    console_printf("storage-only-service: version=1 seq=%u start_us=%u end_us=%u requested=%u actual=%u ok=%u submitted=%u observed=%u resumed=%u seen=%u valid=%u checks=%u samples=%u gap=%u completion_gap=%u empty=%u events=%u last_empty=%u\n",
        r->sequence,c->start_us,c->end_us,c->request_bytes,c->actual,c->ok,b->submitted,b->observed,b->resumed,(uint64_t)b->seen,(uint64_t)b->service_valid,b->service_checks,b->service_samples,b->service_gap_max_us,b->completion_gap_us,b->empty_checks,b->event_checks,b->last_empty_sample_us);
}
static void compare_poison(struct block_device *parent) {
    comparison_poisoned=failed=true;
    if(parent)block_invalidate(parent); /* Terminal identity; never reset/reuse pending DMA. */
}
bool workspace_storage_compare(void) {
    struct scope scope;if(!begin(&scope)){end(&scope);return false;}
    bool ok=false;unsigned completed=0;uint64_t claim=0;
    struct block_device *volume=active,*parent=block_parent(active);
    char uuid[9];for(unsigned i=0;i<9;i++)uuid[i]=active_uuid[i];
    char path[]="/work/srvros-work/storage-compare-0000000000000000.bin";
    uint64_t start=timer_monotonic_us(),nonce=start;
    if(compare_ran || !timer_reference_ready() || !nonce || !volume || !parent || failed || comparison_poisoned ||
       mount_hold.token || workspace_image_busy() || scratch.open || scratch.poisoned ||
       process_has_open_vfs_prefix("/work") || !block_claim_available(parent,volume))goto out;
    for(unsigned i=0;i<16;i++)path[sizeof("/work/srvros-work/storage-compare-")-1+i]="0123456789abcdef"[(nonce>>(60-i*4))&15];
    if(vfs_lookup(path))goto out; /* Existing paths are never overwritten. */
    compare_ran=true;compare_pattern(0);
    console_printf("storage-only: begin version=1 path=%s start_us=%u bytes=32768 pairs=6 samples=12 limit_us=180000000 policy=unchanged\n",path,start);
    if(!exfat_write_file(path,compare_payload,STORAGE_COMPARE_BYTES) || !sync_locked() || !compare_file_read(path,parent)) {
        compare_poison(parent);goto finish;
    }
    /* Persist fixture identity before parking; no logging I/O in measurements. */
    if(!workspace_log_pause())goto finish;
    ok=true;
    for(unsigned i=0;ok && i<STORAGE_COMPARE_SAMPLES;i++) {
        struct storage_compare_sample *r=&compare_samples[i];
        *r=(struct storage_compare_sample){.sequence=i+1,.kind=((i/2)^(i%2))&1};
        if(timer_monotonic_us()<start || timer_monotonic_us()-start>=STORAGE_COMPARE_LIMIT_US) {ok=false;break;}
        compare_pattern(i+1);r->crc=compare_crc(compare_payload,STORAGE_COMPARE_BYTES);
        console_printf("Storage-only comparison: sample %u/12 path=%s; no input required\n",(uint64_t)i+1,r->kind?"direct extent":"filesystem");
        block_write_fn writer=NULL;block_flush_fn flusher=NULL;
        if(r->kind) {
            struct block_device *proved=NULL,*raw_parent=NULL;uint64_t offset=0,raw=0;
            if(!exfat_exclusive_range(path,STORAGE_COMPARE_BYTES,&proved,&offset) || proved!=volume ||
               !block_raw_range(proved,offset,STORAGE_COMPARE_BYTES,&raw_parent,&raw) || raw_parent!=parent ||
               !parent->write || !parent->flush || !parent->read || !parent->block_size ||
               raw%parent->block_size || STORAGE_COMPARE_CHUNK%parent->block_size ||
               (parent->max_transfer_bytes && parent->max_transfer_bytes<STORAGE_COMPARE_CHUNK)) {ok=false;break;}
            writer=parent->write;flusher=parent->flush;
            if(!workspace_unmount() || workspace_parked_device()!=volume) {ok=false;break;}
            claim=block_claim_raw(parent,NULL);
            if(!claim) {ok=false;break;}
            r->offset=raw;block_forget(parent);
            console_printf("storage-only-lease: seq=%u offset=%u bytes=32768 parked=1 aliases_revoked=1 claimed=1\n",r->sequence,raw);
        }
        bool tracing=compare_trace_begin();
        if(!tracing) {
            if(claim && block_claim_release(claim)) {claim=0;(void)workspace_mount(uuid);}
            ok=false;break;
        }
        r->start=timer_monotonic_us();
        if(r->kind)ok=workspace_parked_device()==volume && writer(parent,r->offset,compare_payload,STORAGE_COMPARE_CHUNK) &&
            writer(parent,r->offset+STORAGE_COMPARE_CHUNK,compare_payload+STORAGE_COMPARE_CHUNK,STORAGE_COMPARE_CHUNK);
        else ok=active==volume && exfat_write_file(path,compare_payload,STORAGE_COMPARE_BYTES);
        uint64_t written=timer_monotonic_us();
        bool trace_ok=compare_trace_end(r);
        r->write_us=written>=r->start?written-r->start:0;
        ok=ok && trace_ok && written>=r->start;
        uint64_t flushed=written;
        if(ok) {
            bool timed=usb_storage_timing_aux_begin();
            ok=timed && (r->kind?flusher(parent):sync_locked());
            flushed=timer_monotonic_us();
            if(timed)usb_storage_timing_aux_end(&r->flush_io);
            ok=ok && flushed>=written && r->flush_io.valid && !r->flush_io.failures;
        }
        r->flush_us=flushed>=written?flushed-written:0;
        bool read_timed=ok && usb_storage_timing_aux_begin();
        ok=ok && read_timed;
        if(ok && r->kind) {
            block_forget(parent);
            for(unsigned offset=0;ok && offset<STORAGE_COMPARE_BYTES;offset+=STORAGE_COMPARE_CHUNK) {
                ok=parent->read(parent,r->offset+offset,compare_read,STORAGE_COMPARE_CHUNK);
                for(unsigned j=0;ok && j<STORAGE_COMPARE_CHUNK;j++)if(compare_read[j]!=compare_payload[offset+j])ok=false;
            }
            block_forget(parent);
        } else if(ok)ok=compare_file_read(path,parent);
        if(read_timed)usb_storage_timing_aux_end(&r->read_io);
        ok=ok && r->read_io.valid && !r->read_io.failures;
        r->end=timer_monotonic_us();
        ok=ok && r->end>=flushed && r->end>=start && r->end-start<=STORAGE_COMPARE_LIMIT_US;
        r->read_us=r->end>=flushed?r->end-flushed:0;
        if(!ok) {compare_poison(parent);break;}
        if(claim) {
            if(!block_claim_release(claim)) {compare_poison(parent);ok=false;break;}
            claim=0;r->retired=1;
            if(!workspace_mount(uuid) || active!=volume || !compare_file_read(path,parent)) {compare_poison(parent);ok=false;break;}
        }
        r->ok=1;completed++;
    }
    if(ok)ok=completed==STORAGE_COMPARE_SAMPLES && !claim && active==volume && sync_locked() && compare_file_read(path,parent);
finish:
    if(claim && !comparison_poisoned) { /* No successful transfer is retried to hide a refusal. */
        if(!block_claim_release(claim))compare_poison(parent);
        else {claim=0;if(!workspace_mount(uuid))ok=false;}
    }
    if(ok && (timer_monotonic_us()<start || timer_monotonic_us()-start>STORAGE_COMPARE_LIMIT_US))ok=false;
    console_printf("storage-only: complete=%s samples=%u elapsed_us=%u mounted=%u clean=%u final_seq=%u\n",ok?"yes":"NO",(uint64_t)completed,timer_monotonic_us()-start,(uint64_t)(active==volume),(uint64_t)!comparison_poisoned,(uint64_t)completed);
    end(&scope);
    for(unsigned i=0;i<completed;i++)compare_emit(&compare_samples[i]);
    return ok;
out:
    end(&scope);return false;
}
