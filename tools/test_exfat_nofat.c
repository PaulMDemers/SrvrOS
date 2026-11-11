/* Bitmap ownership, invalid stale FAT and required full-chain publication. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_SYNC_CONTENT_DISABLED_HOST_TEST
#include "../kernel/src/fs/exfat.c"
static uint8_t disk[4*1024*1024], before[sizeof(disk)];
static unsigned calls, reject_at, allocations;
static bool oom;
uint64_t timer_monotonic_us(void){return 0;}
void *kmalloc(uint64_t n){void *p=oom?NULL:malloc(n);if(p)allocations++;return p;}
void kfree(void *p){if(p){assert(allocations);allocations--;free(p);}}
bool block_read(struct block_device *d,uint64_t off,void *p,uint64_t n){
 (void)d;assert(off<=sizeof(disk)&&n<=sizeof(disk)-off);memcpy(p,disk+off,n);return true;
}
bool block_write(struct block_device *d,uint64_t off,const void *p,uint64_t n){
 (void)d;assert(off<=sizeof(disk)&&n<=sizeof(disk)-off);
 if(++calls==reject_at)return false;
 memcpy(disk+off,p,n);return true;
}
static void reset(struct exfat_volume *v,struct block_device *d,unsigned cluster){
 memset(disk,0xa5,sizeof(disk));memset(disk+131072,0,cluster);
 disk[131072]=1;calls=reject_at=0;oom=false;
 *v=(struct exfat_volume){.device=d,.image_size=sizeof(disk),.cluster_size=cluster,
 .cluster_count=512,.allocation_bitmap_cluster=2,.allocation_bitmap_size=64,
 .cluster_heap_offset_bytes=131072,.fat_offset_bytes=4096};
 memcpy(before,disk,sizeof(disk));
}
int main(void){
 struct block_device d={0};struct exfat_volume v;uint32_t first;bool nofat;
 for(unsigned c=0;c<2;c++)for(unsigned b=0;b<2;b++){
  d.block_size=b?4096:512;reset(&v,&d,c?131072:4096);v.sector_size=b?4096:512;
  assert(allocate_clusters(&v,130,&first,&nofat)&&first==3&&nofat&&calls==1&&!allocations);
  assert(!memcmp(disk+4096,before+4096,4096));
  for(unsigned i=0;i<512;i++)assert(bitmap_bit_is_set(disk+131072,i)==(i<131));
  struct exfat_file f={.volume=&v,.first_cluster=first,.allocated_clusters=130,.no_fat_chain=true};
  assert(free_file_clusters(&f)&&calls==2&&!allocations);
  assert(!memcmp(disk,before,sizeof(disk)));
  assert(allocate_contiguous_clusters(&v,1,&first)&&first==3);
  assert(read_u32(disk+4096+first*4)==0xffffffffu);
 }
 for(unsigned start=127;start<=128;start++)for(unsigned n=1;n<=130;n+=129){
  reset(&v,&d,4096);
  assert(write_linked_fat_range(&v,start,n,true));
  for(unsigned i=0;i<n;i++)assert(read_u32(disk+4096+(start+i)*4)==(i+1==n?0xffffffffu:start+i+1));
  unsigned begin=4096+start*4,end=begin+n*4;
  assert(!memcmp(disk,before,begin)&&!memcmp(disk+end,before+end,sizeof(disk)-end));
 }
 reset(&v,&d,4096);assert(write_linked_fat_range(&v,513,1,true));
 assert(read_u32(disk+4096+513*4)==0xffffffffu);
 assert(!write_linked_fat_range(&v,513,2,true)&&!write_linked_fat_range(&v,2,0,true));
 reset(&v,&d,4096);oom=true;
 assert(!allocate_clusters(&v,2,&first,&nofat)&&!calls&&!allocations&&!memcmp(disk,before,sizeof(disk)));
 reset(&v,&d,4096);reject_at=1;
 assert(!allocate_clusters(&v,2,&first,&nofat)&&v.write_blocked&&calls==1&&!allocations);
 assert(!allocate_clusters(&v,2,&first,&nofat)&&calls==1);
 for(unsigned fail=1;fail<=3;fail++){
  reset(&v,&d,4096);reject_at=fail;
  assert(!write_linked_fat_range(&v,127,130,true)&&v.write_blocked&&calls==fail);
  assert(!write_linked_fat_range(&v,2,1,true)&&calls==fail);
 }
 puts("exfat-nofat: PASS stale FAT preserved, exact bitmap, directory EOC, complete conversion links/terminal, neighbours, boundaries, OOM and required-write quarantine");
}
