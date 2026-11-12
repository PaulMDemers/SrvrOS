/* Full exact bytes, uncached reads and bounded malformed-chain refusal. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_EXFAT_HOST_TEST
#define SRVROS_SYNC_CONTENT_DISABLED_HOST_TEST /* Direct fixture has no sync runtime. */
#include "../kernel/src/fs/exfat.c"
static uint8_t disk[65536],expected[20000];static unsigned reads;static bool fail,oom;
static uint64_t emitted;static unsigned line_bytes,max_line;
#include <stdarg.h>
void console_printf(const char *format,...) {
 va_list args;va_start(args,format);
 for(const char *p=format;*p;p++) {
  unsigned n=1;
  if(*p=='%'){p++;assert(*p=='u');char b[32];n=(unsigned)snprintf(b,sizeof(b),"%llu",(unsigned long long)va_arg(args,uint64_t));}
  emitted+=n;line_bytes+=n;
  if(*p=='\n'){if(line_bytes>max_line)max_line=line_bytes;line_bytes=0;}
 }
 va_end(args);
}
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
 /* A difference label describes the expected field at the first changed
  * byte. It must not turn an unreadable or partial proof into equality. */
 sidecar_stats.active=1;sidecar_differences=(struct sidecar_difference_stats){0};
 const uint8_t record[]="SRVROSMETA1\n/vol/a\t12\t33152\t1\t2\t3\t4\t5\t6\n";
 for(unsigned field=0;field<9;field++) {
  unsigned at=sizeof("SRVROSMETA1\n")-1;for(unsigned n=0;n<field;n++){while(record[at]!='\t')at++;at++;}
  sidecar_byte_difference(record,at);assert(sidecar_differences.field[field]==1);
 }
 sidecar_byte_difference(record,0);assert(sidecar_differences.field[9]==1);
 f.valid_size--;assert(metadata_equal_media(&f,expected,sizeof(expected))==METADATA_DIFFERENT);
 assert(sidecar_differences.shape==1);f.valid_size++;
 /* Worst-width diagnostics plus worst-width control/lease/audio lines stay
  * in the original 2048-byte reserve, without enlarging any row or receipt. */
 sidecar_stats=(struct sidecar_equality_stats){UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX};
 sidecar_differences.shape=sidecar_differences.last_offset=UINT64_MAX;
 for(unsigned i=0;i<10;i++)sidecar_differences.field[i]=UINT64_MAX;
 struct exfat_sidecar_trace view;unsigned old_reads=reads;exfat_sidecar_snapshot(&view);
 assert(reads==old_reads && view.checks==UINT64_MAX && view.shape==UINT64_MAX && view.fields[9]==UINT64_MAX);
 exfat_print_sidecar_equality_at(UINT64_MAX);
 assert(!line_bytes && max_line<1024 && emitted==686);
 /* begin<=207, six leases<=732, complete<=175, wire<=112, audio<=96. */
 assert(emitted+207+732+175+112+96<=2048);
 /* Pointer scratch is reserved before mutation, bounded by existing node
  * limits and retained on allocation refusal without publishing media. */
 struct exfat_mount m={0};oom=true;assert(!reserve_metadata(&m,1));assert(!m.metadata_nodes && !m.metadata_buffer);
 oom=false;assert(reserve_metadata(&m,1));assert(m.metadata_node_capacity==EXFAT_NODE_BLOCK);
 const struct vfs_node **saved=m.metadata_nodes;uint64_t capacity=m.metadata_capacity;
 oom=true;assert(!reserve_metadata(&m,EXFAT_NODE_BLOCK+1));assert(m.metadata_nodes==saved && m.metadata_capacity==capacity);
 oom=false;assert(!reserve_metadata(&m,UINT64_MAX));assert(reserve_metadata(&m,EXFAT_MAX_FILES+EXFAT_MAX_DIRS));
 /* Reverse, equal and zero-length pointer sets exercise heap boundaries. */
 struct vfs_node nodes[3]={{.path="/vol/z"},{.path="/vol"},{.path="/vol/a"}};
 const struct vfs_node *order[]={nodes,nodes+1,nodes+2};metadata_sort(order,3);
 assert(order[0]==nodes+1 && order[1]==nodes+2 && order[2]==nodes);
 metadata_sort(order,3);metadata_sort(order,0);metadata_sort(order,1);
 unsigned count=EXFAT_MAX_FILES+EXFAT_MAX_DIRS;
 struct vfs_node *many=calloc(count,sizeof(*many));char (*paths)[32]=calloc(count,sizeof(*paths));assert(many && paths);
 for(unsigned i=0;i<count;i++) {
  unsigned value=(i*7919u)%count;snprintf(paths[i],32,"/vol/%08u",value);
  many[i].path=paths[i];many[i].metadata.inode=value+1;m.metadata_nodes[i]=many+i;
 }
 metadata_sort(m.metadata_nodes,count);
 for(unsigned i=0;i<count;i++)assert(m.metadata_nodes[i]->metadata.inode==i+1);
 free(many);free(paths);
 kfree(m.metadata_nodes);kfree(m.metadata_buffer);
 sidecar_stats.checks=UINT64_MAX;sidecar_add(&sidecar_stats.checks,1);assert(sidecar_stats.invalid&&sidecar_stats.checks==UINT64_MAX);
 puts("sidecar-equality: PASS complete uncached bytes, last-byte mismatch, unavailable read/allocation, exact length/validity, FAT termination/cycle refusal and bounded scratch");
}
