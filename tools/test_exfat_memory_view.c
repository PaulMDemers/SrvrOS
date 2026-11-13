/* Borrowing immutable bytes may never invoke the registered file/device I/O.
 * Mutability, identity, range and incomplete-publication negatives are real. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_EXFAT_HOST_TEST
#define SRVROS_SYNC_CONTENT_DISABLED_HOST_TEST
#include "../kernel/src/fs/exfat.c"
static uint8_t image[65536];
static struct exfat_file_slot slots[EXFAT_NODE_BLOCK];
static struct vfs_node node;
static struct block_device device;
static bool offline,physical,retired;
const struct vfs_node *vfs_lookup(const char *path){assert(!strcmp(path,node.path));return retired?NULL:&node;}
const uint8_t *block_memory_data(struct block_device *d,uint64_t *size){assert(d==&device);*size=sizeof(image);return physical?NULL:image;}
bool block_device_online(struct block_device *d){assert(d==&device);return !offline;}
bool block_read(struct block_device *d,uint64_t o,void *p,uint64_t n){(void)d;(void)o;(void)p;(void)n;assert(0);return false;}
bool scheduler_io_begin(void){assert(0);return false;}
bool scheduler_io_active(void){assert(0);return true;}
void scheduler_io_end(void){assert(0);}
void *kmalloc(uint64_t n){(void)n;assert(0);return NULL;}
void kfree(void *p){(void)p;assert(0);}
static bool dummy_write(struct block_device *d,uint64_t o,const void *p,uint64_t n){(void)d;(void)o;(void)p;(void)n;assert(0);return false;}
static void reset(void){
 memset(mounts,0,sizeof(mounts));memset(slots,0,sizeof(slots));memset(&device,0,sizeof(device));offline=physical=retired=false;
 struct exfat_mount *m=&mounts[0];m->used=m->ready=true;m->file_count=1;m->file_capacity=EXFAT_NODE_BLOCK;m->file_blocks[0]=slots;
 m->volume=(struct exfat_volume){.device=&device,.image_size=sizeof(image),.cluster_size=4096,.cluster_count=14,.cluster_heap_offset_bytes=4096};
 strcpy(slots[0].path,"/fat/bin/app");slots[0].file=(struct exfat_file){.volume=&m->volume,.first_cluster=2,.size=5000,.valid_size=5000,.allocated_clusters=2,.no_fat_chain=true};
 node=(struct vfs_node){.used=true,.path=slots[0].path,.type=VFS_NODE_FILE,.size=5000,.private_data=&slots[0].file,.read_all=exfat_read_all,.release_data=exfat_release_data};
}
int main(void){
 const uint8_t *data=NULL;uint64_t size=0;
 reset();assert(exfat_readonly_memory_view(&node,&data,&size) && data==image+4096 && size==5000);
 mounts[0].volume.device=NULL;mounts[0].volume.image=image;
 assert(exfat_readonly_memory_view(&node,&data,&size) && data==image+4096);
 for(unsigned reason=0;reason<18;reason++){
  reset();switch(reason){
   case 0:mounts[0].ready=false;break;case 1:device.write=dummy_write;mounts[0].volume.write_blocked=true;break;
   case 2:offline=true;break;case 3:physical=true;break;case 4:retired=true;break;
   case 5:slots[0].file.valid_size--;break;case 6:slots[0].file.no_fat_chain=false;break;
   case 7:slots[0].file.allocated_clusters=1;break;case 8:slots[0].file.first_cluster=15;break;
   case 9:slots[0].file.first_cluster=1;break;case 10:node.used=false;break;
   case 11:node.private_data=&slots[1].file;break;case 12:node.read_all=NULL;break;
   case 13:node.size--;break;case 14:mounts[0].volume.mutation_depth=1;break;
   case 15:mounts[0].volume.image_size=8192;break;case 16:mounts[0].file_count=EXFAT_NODE_BLOCK+1;break;
   case 17:node.path=NULL;break;
  }
  data=(const uint8_t *)1;size=42;
  assert(!exfat_readonly_memory_view(&node,&data,&size) && data==(const uint8_t *)1 && size==42);
 }
 puts("exfat-memory-view: PASS immutable memory bytes only, completed mount/current identity, contiguous full VDL and range; mutable/physical/offline/retired/malformed refuse without callbacks");
}
