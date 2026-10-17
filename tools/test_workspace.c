#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_WORKSPACE_HOST_TEST
#include "../kernel/src/workspace.c"

static struct block_device disks[2];
static unsigned count, enables, disables, flushes, depth;
static bool mounted, busy_files, dirty, bad_marker, missing, flush_ok, file_sync_ok, foreign;
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
    (void)d;(void)o;(void)p;(void)n;assert(!"workspace control must not write user files");return false;
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
    active=NULL;failed=false;memset(disks,0,sizeof(disks));
    count=1;enables=disables=flushes=depth=0;
    mounted=busy_files=dirty=bad_marker=missing=foreign=false;
    flush_ok=file_sync_ok=true;
    dir=(struct vfs_node){.type=VFS_NODE_DIRECTORY};
    marker=(struct vfs_node){.type=VFS_NODE_FILE,.size=sizeof(marker_text)-1};
}
int main(void) {
    reset();workspace_boot("srvros.storage=ram");assert(!active && !enables);
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
    puts("workspace-host: PASS (UUID/marker/duplicate/dirty guards, I/O owner, busy files, flush failure, unmount/revoke)");
}
