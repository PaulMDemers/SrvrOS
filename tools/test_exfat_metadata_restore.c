/* Production exFAT/VFS on disposable RAM media,not native acceptance. */
#define main import_fixture_main
#include "test_exfat_import.c"
#undef main
uint64_t initramfs_file_count(void){return 0;}
const struct initramfs_file *initramfs_file_at(uint64_t i){(void)i;return NULL;}
static char outside_names[1200][32];
static void outside_nodes(unsigned count){
 vfs_init();assert(count>=1 && count<=1200);
 for(unsigned i=1;i<count;i++){snprintf(outside_names[i],sizeof outside_names[i],"/outside-%u",i);assert(vfs_register_directory(outside_names[i]));}
 assert(vfs_count()==count);
}
static void auth_metadata_refusal(const char *sidecar) {
 outside_nodes(10);fixture(1100,true);
 if(sidecar) {
  assert(exfat_mount_block_device_clean("/vol",&device));
  assert(exfat_write_file("/vol/extra",NULL,0));
  assert(exfat_write_file("/vol/.srvros/meta",(const uint8_t *)sidecar,strlen(sidecar)));
  assert(exfat_unmount("/vol"));
 }
 uint8_t *before=malloc(BYTES);assert(before);memcpy(before,disk,BYTES);
 unsigned before_writes=writes,before_flushes=flushes;
 assert(!exfat_mount_block_device_auth("/vol",&device));
 assert(!vfs_lookup("/vol") && vfs_count()==10);
 assert(writes==before_writes && flushes==before_flushes && !memcmp(before,disk,BYTES));
 free(before);
}
/* Snapshot reads only retained mount/VFS state: never the backing medium. */
static void snapshot_no_io(struct srv_mount_snapshot *snapshot, uint64_t uid, uint64_t gid) {
 unsigned rd=reads,wr=writes,fl=flushes;
 memset(snapshot,0,sizeof(*snapshot));
 snapshot->version=SRV_MOUNT_SNAPSHOT_VERSION;
 assert(exfat_mount_snapshot(snapshot,uid,gid)==0);
 assert(reads==rd && writes==wr && flushes==fl);
}
static const struct srv_mount_entry *snapshot_entry(const struct srv_mount_snapshot *snapshot,const char *path) {
 for(unsigned i=0;i<snapshot->count;i++)
  if(!strcmp(snapshot->entries[i].mount_path,path))return &snapshot->entries[i];
 return NULL;
}
static void snapshot_regression(void) {
 struct srv_mount_snapshot snapshot;
 vfs_namespace_system_reset();outside_nodes(1);
 snapshot_no_io(&snapshot,0,0);
 assert(snapshot.count==1 && snapshot_entry(&snapshot,"/") &&
        snapshot.entries[0].filesystem==SRV_MOUNT_VFS && !strcmp(snapshot.entries[0].root_path,"/") &&
        !strcmp(snapshot.entries[0].source,"srvros-vfs"));
 fixture(1100,true);assert(exfat_mount_block_device_clean("/vol",&device));
 snapshot_no_io(&snapshot,0,0);
 const struct srv_mount_entry *entry=snapshot_entry(&snapshot,"/vol");
 assert(snapshot.count==2 && entry && entry->filesystem==SRV_MOUNT_EXFAT &&
        !strcmp(entry->root_path,"/") && !strcmp(entry->source,"import-test") &&
        !(entry->flags&SRV_MOUNT_PROJECTION));
 snapshot_no_io(&snapshot,42,43);
 assert(snapshot.count==1 && snapshot_entry(&snapshot,"/") && !snapshot_entry(&snapshot,"/vol"));
 assert(exfat_unmount("/vol"));
 snapshot_no_io(&snapshot,0,0);assert(snapshot.count==1 && !snapshot_entry(&snapshot,"/vol"));

 outside_nodes(1);fixture(1100,true);assert(exfat_mount_block_device_clean("/fat",&device));
 assert(exfat_create_directory("/fat/bin") && exfat_create_directory("/fat/var") &&
        exfat_create_directory("/fat/home"));
 assert(exfat_sync_mount_metadata(&mounts[0]) && vfs_lookup("/fat/.srvros/meta"));
 const struct vfs_node *var=vfs_lookup("/fat/var");assert(var);
 struct vfs_metadata restricted=var->metadata;restricted.mode=(restricted.mode&VFS_MODE_IFMT)|0700;
 assert(vfs_set_metadata("/fat/var",&restricted));
 assert(vfs_namespace_system_activate());
 snapshot_no_io(&snapshot,0,0);
 assert(snapshot.count==6);
 const char *paths[]={"/bin","/usr","/var","/home"};
 const char *roots[]={"/bin","/","/var","/home"};
 for(unsigned i=0;i<4;i++) {
  entry=snapshot_entry(&snapshot,paths[i]);
  assert(entry && entry->filesystem==SRV_MOUNT_EXFAT && (entry->flags&SRV_MOUNT_PROJECTION) &&
         !strcmp(entry->root_path,roots[i]) && !strcmp(entry->source,"import-test"));
 }
 snapshot_no_io(&snapshot,42,43);
 assert(snapshot.count==5 && snapshot_entry(&snapshot,"/fat") && snapshot_entry(&snapshot,"/bin") &&
        snapshot_entry(&snapshot,"/usr") && snapshot_entry(&snapshot,"/home") && !snapshot_entry(&snapshot,"/var"));
 assert(exfat_unmount("/fat"));
 snapshot_no_io(&snapshot,0,0);assert(snapshot.count==1 && snapshot_entry(&snapshot,"/"));
 vfs_namespace_system_reset();
}
int main(void){
 snapshot_regression();
 auth_metadata_refusal(NULL);
 auth_metadata_refusal("SRVROSMETA1\n/vol\t60000\t16877\t2\t0\t0\t1\t2\t3\n");
 auth_metadata_refusal("SRVROSMETA1\n/vol\t60000\t16877\t2\t0\t0\t1\t2\t3\n"
  "/vol/source\t60001\t33188\t1\t0\t0\t1\t2\t3\n"
  "/vol/source\t60002\t33188\t1\t0\t0\t1\t2\t3\n");
 outside_nodes(10);fixture(1100,true);assert(exfat_mount_block_device_clean("/vol",&device));
 assert(exfat_write_file("/vol/extra",(const uint8_t *)"payload",7));
 const char *paths[]={"/vol","/vol/source","/vol/extra"};struct vfs_metadata_restore saved[3];
 for(unsigned i=0;i<3;i++){
  saved[i].node=vfs_lookup(paths[i]);saved[i].metadata=saved[i].node->metadata;
  saved[i].metadata.inode=1000+i;saved[i].metadata.uid=42;saved[i].metadata.gid=43;
  saved[i].metadata.atime=100;saved[i].metadata.mtime=200;saved[i].metadata.ctime=300;
 }
 assert(vfs_restore_metadata("/vol",saved,3));assert(exfat_sync_mount_metadata(&mounts[0]));assert(exfat_unmount("/vol"));
 const unsigned counts[]={998,997,100,998};
 for(unsigned round=0;round<4;round++){
  outside_nodes(counts[round]);unsigned wr=writes,fl=flushes;
  assert(exfat_mount_block_device_auth("/vol",&device));assert(writes==wr && flushes==fl);
  for(unsigned i=0;i<3;i++){const struct vfs_node *n=vfs_lookup(paths[i]);assert(n && !memcmp(&n->metadata,&saved[i].metadata,sizeof n->metadata));}
  assert(exfat_sync_mount_metadata(&mounts[0]));
  for(unsigned i=0;i<3;i++)assert(!memcmp(&vfs_lookup(paths[i])->metadata,&saved[i].metadata,sizeof saved[i].metadata));
  const struct vfs_node *meta=vfs_lookup("/vol/.srvros/meta");const uint8_t *raw;uint64_t length;
  assert(vfs_read_all(meta,&raw,&length));wr=writes;fail_allocation=allocations+1;
  assert(!parse_metadata_records(&mounts[0],raw,length,strlen("SRVROSMETA1\n"),false));fail_allocation=0;
  for(unsigned i=0;i<3;i++)assert(!memcmp(&vfs_lookup(paths[i])->metadata,&saved[i].metadata,sizeof saved[i].metadata));
  live_reference=true;assert(!parse_metadata_records(&mounts[0],raw,length,strlen("SRVROSMETA1\n"),false));live_reference=false;
  assert(writes==wr);vfs_release_data(meta,raw);assert(exfat_unmount("/vol"));
 }
 outside_nodes(1001);uint8_t *before=malloc(BYTES);assert(before);memcpy(before,disk,BYTES);unsigned wr=writes;
 assert(!exfat_mount_block_device_clean("/vol",&device));assert(!vfs_lookup("/vol") && vfs_count()==1001 && writes==wr && !memcmp(before,disk,BYTES));
 assert(exfat_mount_block_device("/vol",&device));assert(!exfat_is_writable("/vol"));
 assert(!exfat_write_file("/vol/new",(const uint8_t *)"bad",3) && writes==wr && !memcmp(before,disk,BYTES));assert(exfat_unmount("/vol"));free(before);
 /* Existing malformed main sidecar stays read-only and byte-preserved. */
 outside_nodes(10);fixture(1100,true);assert(exfat_mount_block_device("/vol",&device));
 assert(exfat_write_file("/vol/extra",NULL,0));
 const char malformed[]="SRVROSMETA1\n/vol/source\tbad\n";
 assert(exfat_write_file("/vol/.srvros/meta",(const uint8_t *)malformed,sizeof malformed-1));assert(exfat_unmount("/vol"));
 outside_nodes(10);before=malloc(BYTES);assert(before);memcpy(before,disk,BYTES);wr=writes;
 assert(exfat_mount_block_device("/vol",&device));assert(!exfat_is_writable("/vol"));
 assert(!exfat_write_file("/vol/new",NULL,0) && writes==wr && !memcmp(before,disk,BYTES));assert(exfat_unmount("/vol"));free(before);
 /* Duplicate persisted IDs are invalid even if both paths are live. */
 outside_nodes(10);fixture(1100,true);assert(exfat_mount_block_device("/vol",&device));assert(exfat_write_file("/vol/extra",NULL,0));
 const char duplicate[]="SRVROSMETA1\n/vol/source\t50000\t33188\t1\t0\t0\t1\t2\t3\n/vol/extra\t50000\t33188\t1\t0\t0\t1\t2\t3\n";
 assert(exfat_write_file("/vol/.srvros/meta",(const uint8_t *)duplicate,sizeof duplicate-1));assert(exfat_unmount("/vol"));
 outside_nodes(10);before=malloc(BYTES);assert(before);memcpy(before,disk,BYTES);wr=writes;
 assert(exfat_mount_block_device("/vol",&device));assert(!exfat_is_writable("/vol"));
 assert(!exfat_write_file("/vol/new",NULL,0) && writes==wr && !memcmp(before,disk,BYTES));assert(exfat_unmount("/vol"));free(before);
 /* No sidecar is still a fresh writable volume. A later valid sidecar apply
  * allocation failure must never overwrite it with current defaults. */
 outside_nodes(10);fixture(1100,true);assert(exfat_mount_block_device("/vol",&device));assert(exfat_is_writable("/vol"));
 assert(exfat_write_file("/vol/extra",NULL,0));before=malloc(BYTES);assert(before);memcpy(before,disk,BYTES);wr=writes;
 fail_allocation=allocations+2;assert(!load_mount_metadata(&mounts[0],EXFAT_ADMIT_LEGACY));fail_allocation=0;
 assert(!exfat_is_writable("/vol") && !exfat_write_file("/vol/new",NULL,0) && writes==wr && !memcmp(before,disk,BYTES));assert(exfat_unmount("/vol"));free(before);
 puts("exfat-metadata-restore: PASS strict admission, no-write refusals, sync/remount, allocation and identity checks");return 0;
}
