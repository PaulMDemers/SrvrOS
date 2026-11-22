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
int main(void){
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
  outside_nodes(counts[round]);unsigned wr=writes;assert(exfat_mount_block_device_clean("/vol",&device));assert(writes==wr);
  for(unsigned i=0;i<3;i++){const struct vfs_node *n=vfs_lookup(paths[i]);assert(n && !memcmp(&n->metadata,&saved[i].metadata,sizeof n->metadata));}
  assert(exfat_sync_mount_metadata(&mounts[0]));
  for(unsigned i=0;i<3;i++)assert(!memcmp(&vfs_lookup(paths[i])->metadata,&saved[i].metadata,sizeof saved[i].metadata));
  const struct vfs_node *meta=vfs_lookup("/vol/.srvros/meta");const uint8_t *raw;uint64_t length;
  assert(vfs_read_all(meta,&raw,&length));wr=writes;fail_allocation=allocations+1;
  assert(!parse_metadata_records(&mounts[0],raw,length,strlen("SRVROSMETA1\n"),true));fail_allocation=0;
  for(unsigned i=0;i<3;i++)assert(!memcmp(&vfs_lookup(paths[i])->metadata,&saved[i].metadata,sizeof saved[i].metadata));
  live_reference=true;assert(!parse_metadata_records(&mounts[0],raw,length,strlen("SRVROSMETA1\n"),true));live_reference=false;
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
 fail_allocation=allocations+2;assert(!load_mount_metadata(&mounts[0],false));fail_allocation=0;
 assert(!exfat_is_writable("/vol") && !exfat_write_file("/vol/new",NULL,0) && writes==wr && !memcmp(before,disk,BYTES));assert(exfat_unmount("/vol"));free(before);
 puts("exfat-metadata-restore: PASS disposable sync/remount across boot counts,provisional/internal inode displacement,all persisted fields unchanged,OOM/live-reader/real outside-collision refusal;no native acceptance");return 0;
}
