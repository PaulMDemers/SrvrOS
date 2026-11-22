/* Real VFS restore fixture;no disk/hardware or native acceptance. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/vfs.c"
static unsigned allocations,notifications;static bool fail_alloc;
void *kcalloc(size_t n,size_t s){allocations++;return fail_alloc?NULL:calloc(n,s);}
void kfree(void *p){free(p);}
uint64_t initramfs_file_count(void){return 0;}
const struct initramfs_file *initramfs_file_at(uint64_t i){(void)i;return NULL;}
int64_t timer_realtime_us(void){return 1794186027000000LL;}
void console_write(const char *s){(void)s;}
void console_printf(const char *s,...){(void)s;}
static bool read_test(const struct vfs_node *n,const uint8_t **p,uint64_t *s){(void)n;*p=(const uint8_t *)"x";*s=1;return true;}
static bool changed(const char *p,const struct vfs_metadata *m){(void)p;(void)m;notifications++;return true;}
static char outside[1600][32],files[567][32];static struct vfs_metadata_restore entries[568];
static void setup(unsigned external){
 fail_alloc=false;vfs_init();assert(external>=1 && external<=1600);
 for(unsigned i=1;i<external;i++){snprintf(outside[i],sizeof outside[i],"/outside-%u",i);assert(vfs_register_file(outside[i],1,NULL,read_test));}
 assert(vfs_count()==external);assert(vfs_register_directory("/work"));entries[0].node=vfs_lookup("/work");
 for(unsigned i=0;i<567;i++){snprintf(files[i],sizeof files[i],"/work/file-%u",i);assert(vfs_register_file(files[i],1,NULL,read_test));entries[i+1].node=vfs_lookup(files[i]);}
 assert(vfs_register_directory("/work/.srvros"));assert(vfs_register_file("/work/.srvros/meta",1,NULL,read_test));
 for(unsigned i=0;i<568;i++){
  entries[i].metadata=entries[i].node->metadata;entries[i].metadata.inode=i<75?1114+i:2000+i;
  entries[i].metadata.uid=42;entries[i].metadata.gid=43;entries[i].metadata.atime=100;entries[i].metadata.mtime=200;entries[i].metadata.ctime=300;
 }
 notifications=0;vfs_set_metadata_changed_callback(changed);
}
static void unique(void){for(uint64_t i=0;i<vfs_count();i++){assert(vfs_node_at(i)->metadata.inode);for(uint64_t j=0;j<i;j++)assert(vfs_node_at(i)->metadata.inode!=vfs_node_at(j)->metadata.inode);}}
static void exact(void){for(unsigned i=0;i<568;i++)assert(!memcmp(&entries[i].node->metadata,&entries[i].metadata,sizeof entries[i].metadata));unique();assert(!notifications);}
static void unchanged_refusal(const char *prefix,struct vfs_metadata_restore *e,uint64_t count){
 uint64_t n=vfs_count(),before_next=next_inode;struct vfs_metadata *before=malloc(n*sizeof *before);assert(before);
 for(uint64_t i=0;i<n;i++)before[i]=vfs_node_at(i)->metadata;
 assert(!vfs_restore_metadata(prefix,e,count));assert(next_inode==before_next && vfs_count()==n && !notifications);
 for(uint64_t i=0;i<n;i++)assert(!memcmp(&before[i],&vfs_node_at(i)->metadata,sizeof *before));
 free(before);
}
int main(void){
 setup(544);assert(vfs_lookup("/work/.srvros/meta")->metadata.inode==1114);unsigned shifted=0;
 for(unsigned i=0;i<568;i++){assert(vfs_set_metadata(entries[i].node->path,&entries[i].metadata));if(entries[i].node->metadata.inode!=entries[i].metadata.inode){assert(i<75 && entries[i].node->metadata.inode==entries[i].metadata.inode+1);shifted++;}}
 assert(shifted==75); /* Same cascade shape as the physical diff,not causality proof. */
 const unsigned counts[]={543,544,545,1000};
 for(unsigned k=0;k<4;k++){
  setup(counts[k]);unsigned before=allocations;fail_alloc=true;assert(vfs_restore_metadata("/work",entries,568));assert(allocations==before);exact();fail_alloc=false;
  struct vfs_metadata_restore swap=entries[0];entries[0]=entries[567];entries[567]=swap;assert(vfs_restore_metadata("/work",entries,568));exact();
 }
 setup(544);struct vfs_metadata_restore a[2]={entries[0],entries[1]};
 a[0].metadata.inode=a[1].node->metadata.inode;a[1].metadata.inode=a[0].node->metadata.inode;
 assert(vfs_restore_metadata("/work",a,2));assert(a[0].node->metadata.inode==a[0].metadata.inode && a[1].node->metadata.inode==a[1].metadata.inode);unique();
 setup(544);a[0]=entries[0];a[1]=entries[1];a[1].metadata.inode=a[0].metadata.inode;unchanged_refusal("/work",a,2);
 a[1]=entries[0];unchanged_refusal("/work",a,2);
 a[1]=entries[1];a[1].node=NULL;unchanged_refusal("/work",a,2);
 a[1]=entries[1];a[1].metadata.inode=0;unchanged_refusal("/work",a,2);
 a[1]=entries[1];a[1].metadata.mode=VFS_MODE_IFDIR|0755;unchanged_refusal("/work",a,2);
 a[1]=entries[1];a[1].metadata.mode|=1ull<<63;unchanged_refusal("/work",a,2);
 a[1]=entries[1];a[1].metadata.nlink=2;unchanged_refusal("/work",a,2);
 a[1]=entries[1];a[1].metadata.inode=vfs_lookup("/")->metadata.inode;unchanged_refusal("/work",a,2);
 a[1]=entries[1];a[1].node=vfs_lookup("/");unchanged_refusal("/work",a,2);
 a[1]=entries[1];unchanged_refusal("/wor",a,2);unchanged_refusal("/work/file-1",a,2);unchanged_refusal(NULL,a,2);
 assert(vfs_register_directory("/workspace"));a[1].node=vfs_lookup("/workspace");a[1].metadata=a[1].node->metadata;unchanged_refusal("/work",a,2);
 a[0]=entries[0];a[0].metadata.inode=UINT64_MAX;assert(vfs_restore_metadata("/work",a,1));assert(vfs_register_file("/after-wrap",1,NULL,read_test));unique();
 setup(1114);unchanged_refusal("/work",entries,568); /* Genuine outside collision. */
 puts("vfs-metadata-restore: PASS real75-ID cascade reproduction,batch/permutation/internal displacement,different boot counts,atomic refusals,no callbacks/allocation,UINT64 wrap;no native acceptance");return 0;
}
