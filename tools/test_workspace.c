#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_WORKSPACE_HOST_TEST
#include "../kernel/src/workspace.c"

static struct block_device disks[2];
static bool image_busy;
bool workspace_image_busy(void){return image_busy;}
static bool mounted, busy_files, dirty, bad_marker, missing, flush_ok, file_sync_ok, foreign;
static unsigned count, enables, disables, flushes, depth, raw_writes, invalidations;
static bool scratch_file, extent_ok, raw_write_ok, raw_read_ok, mismatch;
static uint8_t scratch_data[4096];
uint64_t timer_monotonic_us(void){return 12345;}
uint64_t block_device_size(const struct block_device *d){return d?d->block_size*d->block_count:0;}
void block_forget(struct block_device *d){assert(d==disks);invalidations++;}
bool exfat_write_file(const char *path,const uint8_t *data,uint64_t size){
    assert(mounted&&!scratch_file&&strstr(path,"battery-io-")&&size==4096);
    scratch_file=true;memcpy(scratch_data,data,4096);return true;
}
bool exfat_scratch_range(const char *path,struct block_device **d,uint64_t *off){
    assert(mounted&&scratch_file&&strstr(path,"battery-io-"));
    if(!extent_ok)return false;
    *d=disks;*off=4096;return true;
}
static bool raw_read(struct block_device *d,uint64_t off,void *data,uint64_t length){
    assert(d==disks&&off==4096&&length==4096&&!mounted&&!d->write&&!d->flush);
    memcpy(data,scratch_data,4096);if(mismatch)((uint8_t *)data)[0]^=1;return raw_read_ok;
}
static struct vfs_node dir, marker;
static const char marker_text[] = "SRVROS-WORKSPACE-V1 37d8c5d0\n";
void console_write(const char *s) { (void)s; }
void console_printf(const char *f,...) { (void)f; }
bool scheduler_io_begin(void) { if(foreign)return false; depth++;return true; }
bool scheduler_io_active(void) { return foreign || depth; }
void scheduler_io_end(void) { assert(depth);depth--; }
struct block_device *block_device_at(uint64_t i) { return i<count?&disks[i]:NULL; }
struct block_device *block_parent(struct block_device *d) { return d; }
bool usb_storage_is_device(struct block_device *d) { return d!=NULL; }
bool block_read(struct block_device *d,uint64_t offset,void *data,uint64_t length) {
    (void)d;assert(!offset && length==512);memset(data,0,length);
    uint8_t *b=data;memcpy(b+3,"EXFAT   ",8);b[510]=0x55;b[511]=0xaa;
    b[100]=0xd0;b[101]=0xc5;b[102]=0xd8;b[103]=0x37;return true;
}
static bool write_stub(struct block_device *d,uint64_t o,const void *p,uint64_t n) {
    assert(d==disks&&o==4096&&n==4096&&!mounted&&!d->write&&!d->flush);
    raw_writes++;memcpy(scratch_data,p,raw_write_ok?4096:2048);return raw_write_ok;
}
static bool flush_stub(struct block_device *d) { assert(d==disks);flushes++;return flush_ok; }
bool usb_storage_enable_volume_writes(struct block_device *d) {
    enables++;d->write=write_stub;d->flush=flush_stub;return true;
}
bool usb_storage_disable_volume_writes(struct block_device *d) {
    disables++;d->write=NULL;d->flush=NULL;return true;
}
bool exfat_mount_block_device(const char *p,struct block_device *d) {
    assert(!strcmp(p,"/work") && d==disks && !mounted);mounted=true;return true;
}
bool exfat_unmount(const char *p) { assert(!strcmp(p,"/work") && mounted);mounted=false;return true; }
bool exfat_is_writable(const char *p) { assert(!strcmp(p,"/work"));return mounted && !dirty && disks[0].write; }
const struct vfs_node *vfs_lookup(const char *p) {
    if(!mounted)return NULL;
    if(strstr(p,"battery-io-")&&scratch_file)return &marker;
    if(!strcmp(p,"/work") || !strcmp(p,"/work/srvros-work"))return &dir;
    if(!strcmp(p,"/work/srvros-work/.workspace") && !missing)return &marker;
    return NULL;
}
bool vfs_read_all(const struct vfs_node *n,const uint8_t **data,uint64_t *size) {
    assert(n==&marker);*data=(const uint8_t *)(bad_marker?"INVALID-WORKSPACE-V1 37d8c5d0\n":marker_text);
    *size=sizeof(marker_text)-1;return true;
}
void vfs_release_data(const struct vfs_node *n,const uint8_t *d) { (void)n;(void)d; }
bool process_has_open_vfs_prefix(const char *p) { assert(!strcmp(p,"/work"));return busy_files; }
int64_t process_file_sync_all(void) { return file_sync_ok?0:-1; }
static void reset(void) {
    if(scratch.claim)block_claim_release(scratch.claim);
    active=NULL;parked=NULL;failed=image_busy=false;scratch=(__typeof__(scratch)){0};memset(disks,0,sizeof(disks));
    mount_generation=hold_serial=0;mount_hold=(__typeof__(mount_hold)){0};
    count=1;enables=disables=flushes=depth=raw_writes=invalidations=0;
    scratch_file=mismatch=false;extent_ok=raw_write_ok=raw_read_ok=true;
    disks[0].used=true;disks[0].block_count=16;disks[0].block_size=512;disks[0].read=raw_read;
    mounted=busy_files=dirty=bad_marker=missing=foreign=false;
    flush_ok=file_sync_ok=true;
    dir=(struct vfs_node){.type=VFS_NODE_DIRECTORY};
    marker=(struct vfs_node){.type=VFS_NODE_FILE,.size=sizeof(marker_text)-1};
}
int main(void) {
    reset();assert(!workspace_mount_hold_acquire()&&!workspace_mount_held());
    assert(!workspace_mount_hold_valid(0)&&!workspace_mount_hold_release(0));
    assert(workspace_mount("37d8c5d0"));
    foreign=true;assert(!workspace_mount_hold_acquire());foreign=false;
    dirty=true;assert(!workspace_mount_hold_acquire());dirty=false;
    uint64_t token=workspace_mount_hold_acquire();assert(token&&workspace_mount_held());
    assert(workspace_mount_hold_valid(token)&&!workspace_mount_hold_acquire());
    assert(!workspace_mount_hold_valid(token+1)&&!workspace_mount_hold_release(token+1));
    assert(!workspace_unmount()&&!flushes&&!disables&&mounted);
    assert(!workspace_scratch_prepare()&&!scratch_file&&!flushes&&!disables);
    assert(!workspace_mount("37d8c5d0")&&workspace_mount_hold_valid(token));
    assert(workspace_sync()&&flushes==1&&workspace_mount_hold_valid(token));
    /* File-I/O activity does not invalidate a lifetime pin. */
    foreign=true;assert(workspace_mount_hold_valid(token));foreign=false;
    assert(workspace_mount_hold_release(token)&&!workspace_mount_held());
    assert(!workspace_mount_hold_release(token)&&!workspace_mount_hold_valid(token));
    uint64_t next=workspace_mount_hold_acquire();assert(next>token);
    assert(!workspace_mount_hold_release(token)&&workspace_mount_hold_valid(next));
    assert(workspace_mount_hold_release(next)&&workspace_unmount());
    assert(workspace_mount("37d8c5d0"));token=workspace_mount_hold_acquire();assert(token>next);
    assert(!workspace_mount_hold_valid(next)&&!workspace_mount_hold_release(next));
    /* Fail closed on an unexpected mount identity change, even the same device
     * pointer/UUID reused under a later mount. A mismatched release cannot unpin. */
    mount_generation++;assert(!workspace_mount_hold_valid(token)&&!workspace_mount_hold_release(token));
    assert(workspace_mount_held()&&!workspace_unmount());mount_generation--;
    active=&disks[1];assert(!workspace_mount_hold_valid(token)&&!workspace_mount_hold_release(token));active=disks;
    /* Storage failure must not trap a successfully retired owner forever. The
     * ordinary failed-storage policy still refuses sync/unmount after release. */
    flush_ok=false;assert(!workspace_sync()&&failed&&workspace_mount_hold_valid(token));
    assert(workspace_mount_hold_release(token)&&!workspace_mount_held());
    assert(!workspace_mount_hold_acquire()&&!workspace_unmount());
    reset();assert(workspace_mount("37d8c5d0"));hold_serial=UINT64_MAX;
    assert(!workspace_mount_hold_acquire()&&!workspace_mount_held());
    assert(workspace_unmount());mount_generation=UINT64_MAX;
    assert(!workspace_mount("37d8c5d0")&&!mounted);
    reset();image_busy=true;assert(!workspace_mount("37d8c5d0")&&!mounted&&!enables);
    reset();assert(!workspace_parked_device());workspace_boot("srvros.storage=ram");assert(!active && !enables);
    workspace_boot("srvros.workspace-volume=37d8c5d0 srvros.workspace-volume=37d8c5d0");assert(!active && !enables);
    assert(!workspace_mount("bad") && !workspace_mount("37d8/c5d0"));
    workspace_boot("srvros.workspace-volume=37d8c5d0");assert(active && mounted && enables==1);
    assert(!workspace_mount("37d8c5d0"));
    busy_files=true;assert(!workspace_unmount() && mounted && !flushes);
    busy_files=false;assert(workspace_sync() && flushes==1);
    assert(workspace_unmount() && !active && !mounted && !disks[0].write && disables==1 && !depth);
    reset();count=2;assert(!workspace_mount("37d8c5d0") && !enables && !mounted);
    reset();missing=true;assert(!workspace_mount("37d8c5d0") && !enables && !mounted);
    reset();bad_marker=true;assert(!workspace_mount("37d8c5d0") && !enables && !mounted);
    reset();dirty=true;assert(!workspace_mount("37d8c5d0") && enables==1 && disables==1 && !mounted);
    reset();foreign=true;assert(!workspace_mount("37d8c5d0") && !enables && !depth);
    reset();assert(workspace_mount("37D8-C5D0"));flush_ok=false;
    assert(!workspace_unmount() && active && mounted && !disables);
    flush_ok=true;assert(!workspace_sync());
    reset();assert(workspace_mount("37d8c5d0"));file_sync_ok=false;
    assert(!workspace_sync() && active && mounted && !flushes);
    reset();assert(workspace_mount("37d8c5d0"));
    assert(!workspace_parked_device());assert(workspace_unmount());
    assert(workspace_parked_device()==disks);
    disks[0].write=write_stub;assert(!workspace_parked_device());disks[0].write=NULL;
    disks[0].flush=flush_stub;assert(!workspace_parked_device());disks[0].flush=NULL;
    assert(workspace_mount("37d8c5d0")&&!workspace_parked_device());
    struct block_device sibling=disks[0];sibling.backing=disks;sibling.block_count=1;
    uint64_t sibling_claim=block_claim_mount(&sibling);assert(sibling_claim);
    unsigned old_flushes=flushes;
    assert(!workspace_unmount() && mounted && flushes==old_flushes);
    assert(!workspace_scratch_prepare() && !scratch_file && flushes==old_flushes);
    assert(block_claim_release(sibling_claim));
    uint8_t data[4096];memset(data,0x72,sizeof(data));
    reset();assert(!workspace_scratch_prepare()&&!workspace_scratch_cycle(data));
    assert(workspace_mount("37d8c5d0"));extent_ok=false;
    assert(!workspace_scratch_prepare()&&mounted&&!raw_writes&&!scratch.open);
    reset();assert(workspace_mount("37d8c5d0"));scratch_file=true;
    assert(!workspace_scratch_prepare()&&!raw_writes&&mounted);
    reset();assert(workspace_mount("37d8c5d0"));
    assert(workspace_scratch_prepare()&&!mounted&&!disks[0].write&&!disks[0].flush);
    assert(!workspace_mount("37d8c5d0")&&!workspace_scratch_prepare());
    assert(!block_claim_mount(disks) && !block_claim_raw(disks,NULL));
    assert(workspace_scratch_cycle(data)&&raw_writes==1&&!memcmp(data,scratch_data,4096));
    assert(workspace_scratch_end()&&!workspace_scratch_cycle(data));
    assert(block_claim_available(disks,NULL));
    assert(workspace_mount("37d8c5d0")&&invalidations>=3);
    for(unsigned fault=0;fault<4;fault++){
        reset();assert(workspace_mount("37d8c5d0")&&workspace_scratch_prepare());
        if(fault==0)raw_write_ok=false;
        if(fault==1)flush_ok=false;
        if(fault==2)raw_read_ok=false;
        if(fault==3)mismatch=true;
        assert(!workspace_scratch_cycle(data)&&scratch.poisoned&&raw_writes==1);
        assert(!workspace_scratch_cycle(data)&&raw_writes==1);
        assert(!workspace_scratch_end()&&!scratch.open&&!scratch.write&&!scratch.flush);
        assert(!workspace_mount("37d8c5d0")&&!depth&&!disks[0].write);
        assert(!block_claim_mount(disks) && !block_claim_available(disks,NULL));
    }
    reset();assert(workspace_mount("37d8c5d0") && workspace_scratch_prepare());
    disks[0].offline=true;assert(!workspace_scratch_ready() && !workspace_parked_device());
    assert(!workspace_scratch_cycle(data) && !raw_writes && !workspace_scratch_end());
    assert(scratch.poisoned && scratch.claim);
    /* No runtime revival API: flipping this bit here only checks the retained claim. */
    disks[0].offline=false;assert(!block_claim_mount(disks));
    puts("workspace-host: PASS (mount identity/hold/release/stale tokens, UUID/marker/duplicate/dirty guards, I/O owner, busy files, flush failure, unmount/revoke)");
}

#define SRVROS_BLOCK_CLAIM_HOST_TEST
#include "../kernel/src/block_claim.c"
