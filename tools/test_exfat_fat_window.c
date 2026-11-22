/* Actual transaction-owned FAT window and uncached full prefix reads. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_EXFAT_HOST_TEST
#include "../kernel/src/fs/exfat.c"
static uint8_t disk[34*1024*1024],out[16384];
static unsigned reads,fat_reads,writes;static uint64_t last_offset,last_bytes,prefix_bytes;
static bool read_fail,write_fail,reenter;
uint64_t timer_monotonic_us(void){return 0;}
bool block_read(struct block_device *d,uint64_t off,void *p,uint64_t n){
 (void)d;assert(off<=sizeof(disk)&&n<=sizeof(disk)-off&&n<=16384);
 reads++;last_offset=off;last_bytes=n;if(off>=4096&&off<8192)fat_reads++;
 if(sync_prefix_proof)prefix_bytes+=n;
 if(read_fail)return false;
 memcpy(p,disk+off,n);if(reenter){reenter=false;mutation_reads_reset(NULL);}return true;
}
bool block_write(struct block_device *d,uint64_t off,const void *p,uint64_t n){
 (void)d;assert(off+n<=sizeof(disk));writes++;memcpy(disk+off,p,write_fail?n/2:n);return !write_fail;
}
int main(void){
 struct block_device d={.used=true,.block_size=512};
 struct exfat_volume v={.device=&d,.image_size=sizeof(disk),.sector_size=512,
  .fat_offset_bytes=4096,.cluster_heap_offset_bytes=32768,.cluster_size=131072,.cluster_count=256};
 memset(disk,0xa5,sizeof(disk));
 for(unsigned c=2;c<258;c++)write_u32(disk+4096+c*4,c==257?0xffffffffu:c+1);
 v.mutation_depth=1;mutation_reads_reset(&v);uint32_t next;
 assert(next_cluster(&v,2,&next)&&next==3&&fat_reads==1&&last_bytes==512);
 for(unsigned c=3;c<100;c++)assert(next_cluster(&v,c,&next)&&next==c+1&&fat_reads==1);
 sync_prefix_proof=true;
 for(unsigned i=0;i<8;i++)assert(volume_read(&v,32768+i*sizeof(out),out,sizeof(out)));
 sync_prefix_proof=false;
 assert(prefix_bytes==8*sizeof(out)&&next_cluster(&v,100,&next)&&next==101&&fat_reads==1);
 assert(write_fat_entry(&v,100,150)&&next_cluster(&v,100,&next)&&next==150&&fat_reads==1);
 assert(next_cluster(&v,128,&next)&&next==129&&fat_reads==2);
 assert(next_cluster(&v,257,&next)&&next==0xffffffffu&&fat_reads==3&&last_offset==5120&&last_bytes==8);
 /* Required terminal/data reads fail without publishing a cached success. */
 mutation_reads_reset(&v);read_fail=true;unsigned before=reads;
 assert(!next_cluster(&v,2,&next)&&reads==before+1);read_fail=false;
 assert(next_cluster(&v,2,&next)&&next==3&&reads==before+2);
 assert(write_fat_entry(&v,3,0x12345678));write_fail=true;
 assert(!write_fat_entry(&v,3,0xaabbccdd)&&v.write_blocked);write_fail=false;
 assert(next_cluster(&v,3,&next)&&next==0x1234ccdd&&last_bytes==4);
 v.write_blocked=false;mutation_reads_reset(&v);reenter=true;before=reads;
 assert(next_cluster(&v,2,&next)&&next==3);
 assert(!mutation_reads.volume&&next_cluster(&v,2,&next)&&reads==before+2);
 v.mutation_depth=0;mutation_reads_reset(NULL);before=reads;
 assert(next_cluster(&v,2,&next)&&next_cluster(&v,2,&next)&&reads==before+2&&last_bytes==4);
 puts("exfat-fat-window: PASS exact links/terminal, transaction-only512B reuse, unchanged full prefix reads, partial FAT patching, final-table bounds, failed/reentrant I/O invalidation and no cross-transaction reuse");
}
