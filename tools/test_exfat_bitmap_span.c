/* Exercise actual allocation/free paths and preserve all neighbouring bytes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_EXFAT_HOST_TEST
#include "../kernel/src/fs/exfat.c"
#define CLUSTER (128u*1024u)
static unsigned char media[4*CLUSTER],before[sizeof(media)];
static unsigned calls;static uint64_t last_offset,last_length;static bool fail;
void *kmalloc(uint64_t n){return malloc(n);}
void kfree(void *p){free(p);}
bool block_read(struct block_device *d,uint64_t off,void *p,uint64_t n){(void)d;assert(off+n<=sizeof(media));memcpy(p,media+off,n);return true;}
bool block_write(struct block_device *d,uint64_t off,const void *p,uint64_t n){(void)d;assert(off+n<=sizeof(media));calls++;last_offset=off;last_length=n;memcpy(media+off,p,fail?(n+1)/2:n);return !fail;}
static void init(struct exfat_volume *v,struct block_device *d){
 memset(media,0,sizeof(media));memset(media+CLUSTER,0xa5,CLUSTER);
 memset(media+CLUSTER,0,8);media[CLUSTER]=1;
 *v=(struct exfat_volume){.device=d,.image_size=sizeof(media),.cluster_size=CLUSTER,
 .cluster_count=64,.allocation_bitmap_cluster=2,.allocation_bitmap_size=8,
 .cluster_heap_offset_bytes=CLUSTER,.fat_offset_bytes=512};
 calls=0;fail=false;mutation_reads_reset(NULL);
}
int main(void){
 struct exfat_volume v;struct block_device d={0};uint32_t cluster=0;bool chain=false;
 init(&v,&d);media[CLUSTER]=0x7f;memcpy(before,media,sizeof(media));
 assert(allocate_contiguous_clusters_impl(&v,3,&cluster)&&cluster==9);
 assert(last_offset==CLUSTER && last_length==2 && media[CLUSTER]==0xff && media[CLUSTER+1]==3);
 assert(!memcmp(media+CLUSTER+2,before+CLUSTER+2,CLUSTER-2));
 struct exfat_file f={.volume=&v,.first_cluster=cluster,.allocated_clusters=3,.no_fat_chain=true};
 assert(free_file_clusters_impl(&f)&&media[CLUSTER]==0x7f&&!media[CLUSTER+1]&&last_length==2);
 init(&v,&d);memset(media+CLUSTER,0xff,8);bitmap_clear_bit(media+CLUSTER,1);bitmap_clear_bit(media+CLUSTER,9);bitmap_clear_bit(media+CLUSTER,63);
 assert(allocate_fragmented_clusters_impl(&v,3,&cluster,&chain)&&cluster==3&&!chain);
 assert(last_offset==CLUSTER&&last_length==8&&media[CLUSTER+8]==0xa5);
 f=(struct exfat_file){.volume=&v,.first_cluster=cluster,.allocated_clusters=3,.no_fat_chain=false};
 assert(free_file_clusters_impl(&f)&&!bitmap_bit_is_set(media+CLUSTER,1)&&!bitmap_bit_is_set(media+CLUSTER,9)&&!bitmap_bit_is_set(media+CLUSTER,63));
 /* Descending chains must not infer the dirty range from their endpoints. */
 init(&v,&d);media[CLUSTER]=0xff;write_u32(media+512+7*4,3);write_u32(media+512+3*4,5);write_u32(media+512+5*4,0xffffffffu);
 f=(struct exfat_file){.volume=&v,.first_cluster=7,.allocated_clusters=3};
 assert(free_file_clusters_impl(&f)&&media[CLUSTER]==0xd5&&last_length==1);
 unsigned char bits[8];memset(bits,0x55,sizeof(bits));unsigned n=calls;
 assert(!write_bitmap_span(&v,CLUSTER,bits,9,8)&&calls==n);
 assert(!write_bitmap_span(&v,CLUSTER,bits,0,64)&&calls==n);
 assert(write_bitmap_span(&v,CLUSTER,bits,63,63)&&last_offset==CLUSTER+7&&last_length==1);
 fail=true;assert(!write_bitmap_span(&v,CLUSTER,bits,0,15)&&v.write_blocked);
 n=calls;assert(!write_bitmap_span(&v,CLUSTER,bits,0,0)&&calls==n);
 puts("exfat-bitmap-span: PASS crossing byte, final bit, fragmented/descending chains, unchanged padding, invalid range and partial-failure quarantine");
}
