/* Full exact bytes, uncached reads and bounded malformed-chain refusal. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_SYNC_CONTENT_DISABLED_HOST_TEST /* Direct fixture has no sync runtime. */
#include "../kernel/src/fs/exfat.c"
static uint8_t disk[65536],expected[20000];static unsigned reads;static bool fail,oom;
uint64_t timer_monotonic_us(void){return 0;}
void *kmalloc(uint64_t n){return oom?NULL:malloc(n);}
void kfree(void *p){free(p);}
bool block_read(struct block_device *d,uint64_t off,void *out,uint64_t n){(void)d;assert(off+n<=sizeof(disk)&&n<=16384);reads++;if(fail)return false;memcpy(out,disk+off,n);return true;}
int main(void){
 struct block_device d={0};struct exfat_volume v={.device=&d,.image_size=sizeof(disk),.cluster_size=4096,.cluster_count=14,.cluster_heap_offset_bytes=4096,.fat_offset_bytes=512};
 struct exfat_file f={.volume=&v,.first_cluster=2,.size=sizeof(expected),.valid_size=sizeof(expected),.allocated_clusters=5,.no_fat_chain=true};
 for(unsigned i=0;i<sizeof(expected);i++)expected[i]=(uint8_t)(i*31+17);
 memcpy(disk+4096,expected,sizeof(expected));assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_EQUAL&&reads==5);
 /* A matching transaction-cache row cannot hide changed or unreadable media. */
 mutation_reads_reset(&v);v.mutation_depth=1;mutation_reads.rows[0].offset=4096;mutation_reads.rows[0].length=4096;memcpy(mutation_reads.rows[0].bytes,expected,4096);
 disk[4096]^=1;assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_DIFFERENT);disk[4096]^=1;
 disk[4096+sizeof(expected)-1]^=1;assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_DIFFERENT);disk[4096+sizeof(expected)-1]^=1;
 fail=true;assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_UNPROVED);fail=false;
 oom=true;assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_UNPROVED);oom=false;
 f.no_fat_chain=false;
 for(unsigned i=2;i<7;i++)write_u32(disk+512+i*4,i==6?0xffffffffu:i+1);
 assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_EQUAL);
 memcpy(disk+4096+(8-2)*4096,expected+4096,4096);write_u32(disk+512+2*4,8);write_u32(disk+512+8*4,4);
 assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_EQUAL);
 write_u32(disk+512+6*4,2);assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_UNPROVED);
 write_u32(disk+512+2*4,0xffffffffu);assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_UNPROVED);
 unsigned before=reads;f.valid_size--;assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_DIFFERENT&&reads==before);f.valid_size++;
 assert(metadata_equal_media(&f,expected,sizeof(expected)-1)==METADATA_DIFFERENT);
 assert(metadata_equal_media(&f,NULL,sizeof(expected))==METADATA_UNPROVED);
 sidecar_stats.checks=UINT64_MAX;sidecar_add(&sidecar_stats.checks,1);assert(sidecar_stats.invalid&&sidecar_stats.checks==UINT64_MAX);
 puts("sidecar-equality: PASS complete uncached bytes, last-byte mismatch, unavailable read/allocation, exact length/validity, FAT termination/cycle refusal and bounded scratch");
}
