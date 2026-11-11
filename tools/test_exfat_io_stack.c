/* Actual exFAT + block cache + file-backed device, with unchanged16KiB cap.
 * Host scheduler/clock model measures completed callback work, not native time. */
#define _POSIX_C_SOURCE 200809L
#include <srvros/block.h>
#define block_read fixture_media_read
#define block_write fixture_media_write
#define block_parent fixture_parent_unused
#define block_device_size fixture_size
#define main fixture_legacy_main
#include "exfat_host_driver.c"
#undef main
#undef block_read
#undef block_write
#undef block_parent
#undef block_device_size
static uint64_t block_test_irq_save(void){return 0;}
static void block_test_irq_restore(uint64_t flags){(void)flags;}
#define SRVROS_BLOCK_HOST_TEST
#include "../kernel/src/block.c"
uint64_t timer_monotonic_us(void){return read_calls+writes;}
bool timer_reference_ready(void){return true;}
bool scheduler_io_yield(void){return false;}

static bool media_read(struct block_device *d,uint64_t off,void *p,uint64_t n){
 assert(n && n<=BLOCK_MAX_BATCH_BYTES && !(off%d->block_size) && !(n%d->block_size));
 return fixture_media_read(d,off,p,n);
}
static bool media_write(struct block_device *d,uint64_t off,const void *p,uint64_t n){
 assert(n && n<=BLOCK_MAX_BATCH_BYTES && !(off%d->block_size) && !(n%d->block_size));
 return fixture_media_write(d,off,p,n);
}
int main(int argc,char **argv){
 assert(argc==3);fd=open(argv[1],O_RDWR);assert(fd>=0);
 struct stat st;assert(fstat(fd,&st)==0);
 struct block_device d={.used=true,.name="io-stack",.block_size=512,
  .block_count=(uint64_t)st.st_size/512,.max_transfer_bytes=BLOCK_MAX_BATCH_BYTES,
  .read=media_read,.write=media_write,.flush=flush};
 const char *block=getenv("SRVROS_TEST_BLOCK_BYTES");
 if(block){uint64_t n=strtoull(block,NULL,10);assert(n==512||n==4096);d.block_size=n;d.block_count=(uint64_t)st.st_size/n;}
 assert(exfat_mount_block_device("/vol",&d));
 enum{SIZE=1366*768*4+54,CHUNK=65536};
 unsigned char *data=malloc(SIZE);assert(data);
 for(unsigned i=0;i<SIZE;i++)data[i]=(uint8_t)(i*37+11);
 uint64_t heap=live_bytes;peak_bytes=heap;read_calls=read_bytes=write_bytes=0;writes=flushes=0;
 block_forget(&d);
 if(!strcmp(argv[2],"growth")){
  for(uint64_t size=0;size<SIZE;){
   size+=SIZE-size>CHUNK?CHUNK:SIZE-size;mock_realtime+=1000000;
   assert(exfat_write_file("/vol/work/capture.bmp",data,size));
   assert(exfat_write_file("/vol/work/other.txt",(const uint8_t *)"marker",6));
  }
 }else if(!strcmp(argv[2],"replace")){
  for(unsigned i=0;i<16;i++){
   mock_realtime+=1000000;data[0]=(uint8_t)i;
   assert(exfat_write_file("/vol/work/capture.bmp",data,CHUNK));
  }
 }else if(!strcmp(argv[2],"metadata")){
  for(unsigned i=0;i<8;i++){
   char from[80],to[80];snprintf(from,sizeof(from),"/vol/work/item-%u",i);
   snprintf(to,sizeof(to),"/vol/work/moved-%u",i);mock_realtime+=1000000;
   assert(exfat_write_file(from,NULL,0));assert(exfat_rename(from,to));assert(exfat_delete_file(to));
  }
 }else assert(0);
 printf("io-stack: reads=%llu read_bytes=%llu writes=%u write_bytes=%llu flushes=%u heap_peak_delta=%llu\n",
  (unsigned long long)read_calls,(unsigned long long)read_bytes,writes,
  (unsigned long long)write_bytes,flushes,(unsigned long long)(peak_bytes-heap));
 /* Discard every clean write-through copy before independent readback. */
 block_forget(&d);
 if(strcmp(argv[2],"metadata"))equal("/vol/work/capture.bmp",data,!strcmp(argv[2],"growth")?SIZE:CHUNK);
 equal("/vol/work/anchor.txt",(const uint8_t *)"unchanged",9);
 block_forget(&d);struct exfat_check_result check={0};assert(exfat_check("/vol",&check));
 assert(!check.errors&&!check.leaked_clusters&&!check.fat_bitmap_mismatches);
 assert(exfat_unmount("/vol"));free(data);close(fd);puts("io-stack-validation: PASS");return 0;
}
