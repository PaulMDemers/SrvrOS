/* Actual FAT writer: bounded unpublished links, neighbours and quarantine. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/fs/exfat.c"
static uint8_t media[8192],before[sizeof(media)];
static unsigned calls,reject_at;static bool tear;
uint64_t timer_monotonic_us(void){return 0;}
bool block_write(struct block_device *d,uint64_t off,const void *data,uint64_t n){
 (void)d;assert(n && n<=512 && off/512==(off+n-1)/512 && off+n<=sizeof(media));calls++;
 if(calls==reject_at){if(tear)memcpy(media+off,data,(n+1)/2);return false;}
 memcpy(media+off,data,n);return true;
}
int main(void){
 struct block_device d={.block_size=4096};
 struct exfat_volume v={.device=&d,.image_size=sizeof(media),.cluster_count=1022,.fat_offset_bytes=512};
 const unsigned starts[]={2,127,128,129,1023},counts[]={0,1,65,130};
 for(unsigned i=0;i<5;i++)for(unsigned j=0;j<4;j++){
  memset(media,0x5a,sizeof(media));memcpy(before,media,sizeof(media));calls=0;v.write_blocked=false;
  unsigned first=starts[i],n=counts[j];bool valid=n<=1023-first;
  assert(link_fat_range(&v,first,n)==valid);
  if(!valid){assert(!calls && !memcmp(media,before,sizeof(media)));continue;}
  unsigned begin=512+first*4,end=begin+n*4;
  assert(calls==(n?(end-1)/512-begin/512+1:0));
  for(unsigned k=0;k<n;k++)for(unsigned b=0;b<4;b++)assert(media[begin+k*4+b]==(uint8_t)((first+k+1)>>(b*8)));
  assert(!memcmp(media,before,begin)&&!memcmp(media+end,before+end,sizeof(media)-end));
 }
 calls=0;assert(!link_fat_range(NULL,2,1)&&!link_fat_range(&v,1,1)&&!link_fat_range(&v,2,UINT64_MAX)&&!calls);
 v.fat_offset_bytes=513;assert(!link_fat_range(&v,2,1)&&!calls);
 v.fat_offset_bytes=UINT64_MAX-3;assert(!link_fat_range(&v,2,1)&&!calls);v.fat_offset_bytes=512;
 for(unsigned t=0;t<2;t++){
  memset(media,0x5a,sizeof(media));calls=0;reject_at=2;tear=t;v.write_blocked=false;
  assert(!link_fat_range(&v,127,130)&&calls==2&&v.write_blocked);
  assert(read_u32(media+512+127*4)==128);
  assert(!link_fat_range(&v,2,1)&&calls==2);
 }
 puts("exfat-link-batch: PASS exact next links, 512-byte boundaries, 4KiB sectors, neighbours, range/overflow refusal, partial failure quarantine and no retry");
}
