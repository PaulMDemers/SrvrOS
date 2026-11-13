/* Full exact bytes, uncached reads and bounded malformed-chain refusal. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_EXFAT_HOST_TEST
#define SRVROS_SYNC_CONTENT_DISABLED_HOST_TEST /* Direct fixture has no sync runtime. */
#include "../kernel/src/fs/exfat.c"
static uint8_t disk[65536],expected[20000];static unsigned reads,fail_read_call;static bool fail,oom,audit_owner;
bool scheduler_io_active(void){return audit_owner;}
static uint64_t last_sidecar[9];static unsigned sidecar_lines;
static uint64_t emitted;static unsigned line_bytes,max_line;
#include <stdarg.h>
void console_printf(const char *format,...) {
 va_list args;va_start(args,format);
 if(!strncmp(format,"exfat-sidecar: ",15)) {
  va_list values;va_copy(values,args);
  for(unsigned i=0;i<9;i++)last_sidecar[i]=va_arg(values,uint64_t);
  va_end(values);sidecar_lines++;
 }

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
bool block_read(struct block_device *d,uint64_t off,void *out,uint64_t n){(void)d;assert(off+n<=sizeof(disk)&&n<=16384);reads++;if(fail || reads==fail_read_call)return false;memcpy(out,disk+off,n);return true;}
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
 /* Complete diagnostic compares every field after the first mismatch, with
  * width changes, added/removed names, chunk boundaries and late I/O failure. */
 char old_meta[30000],new_meta[30000];uint64_t old_size=12,new_size=12;
 memcpy(old_meta,"SRVROSMETA1\n",12);memcpy(new_meta,"SRVROSMETA1\n",12);
 for(unsigned i=0;i<450;i++) {
  uint64_t values[8]={1000+i,33188,1,10,20,30,40,50};
  if(i!=350)old_size+=(uint64_t)snprintf(old_meta+old_size,sizeof(old_meta)-old_size,"/vol/%04u\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\n",i,
   (unsigned long long)values[0],(unsigned long long)values[1],(unsigned long long)values[2],(unsigned long long)values[3],(unsigned long long)values[4],(unsigned long long)values[5],(unsigned long long)values[6],(unsigned long long)values[7]);
  if(i<8)values[i]=i?values[i]+1:UINT64_MAX;
  if(i!=351)new_size+=(uint64_t)snprintf(new_meta+new_size,sizeof(new_meta)-new_size,"/vol/%04u\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\n",i,
   (unsigned long long)values[0],(unsigned long long)values[1],(unsigned long long)values[2],(unsigned long long)values[3],(unsigned long long)values[4],(unsigned long long)values[5],(unsigned long long)values[6],(unsigned long long)values[7]);
 }
 assert(old_size>16384 && old_size<30000 && new_size<30000);
 memcpy(disk+4096,old_meta,old_size);copy_cstr(metadata_audit_subject,sizeof(metadata_audit_subject),"/vol/0000");copy_cstr(metadata_audit_marker,sizeof(metadata_audit_marker),"/vol/0005");
 f.no_fat_chain=true;f.size=f.valid_size=old_size;f.allocated_clusters=clusters_for_size(&v,old_size);
 unsigned full_reads=reads;assert(metadata_full_equal_media(&f,(uint8_t *)new_meta,new_size)==METADATA_DIFFERENT);
 assert(reads-full_reads==f.allocated_clusters && metadata_audit_result.valid);
 assert(metadata_audit_result.old_rows==449 && metadata_audit_result.new_rows==449 && metadata_audit_result.matched==448 && metadata_audit_result.added==1 && metadata_audit_result.removed==1 && metadata_audit_result.changed==8);
 for(unsigned i=0;i<8;i++)assert(metadata_audit_result.fields[i]==1);
 assert(metadata_audit_result.subject_fields==1 && metadata_audit_result.marker_fields==32);
 metadata_audit_result=(struct exfat_metadata_audit){0};fail_read_call=reads+(unsigned)f.allocated_clusters;
 assert(metadata_full_equal_media(&f,(uint8_t *)new_meta,new_size)==METADATA_UNPROVED && !metadata_audit_result.valid);fail_read_call=0;
 disk[4096]='X';assert(metadata_full_equal_media(&f,(uint8_t *)new_meta,new_size)==METADATA_UNPROVED);disk[4096]='S';
 assert(metadata_full_equal_media(&f,(uint8_t *)old_meta,old_size)==METADATA_EQUAL && metadata_audit_result.changed==0 && metadata_audit_result.added==0 && metadata_audit_result.removed==0);
 /* The same full proof also crosses a real 16 KiB command boundary. */
 v.cluster_size=32768;f.allocated_clusters=1;full_reads=reads;
 assert(metadata_full_equal_media(&f,(uint8_t *)new_meta,new_size)==METADATA_DIFFERENT && reads-full_reads==2);
 assert(!exfat_metadata_audit_begin("/vol/a","/vol/b"));audit_owner=true;
 uint64_t audit_token=exfat_metadata_audit_begin("/vol/a","/vol/b");assert(audit_token && !exfat_metadata_audit_begin("/vol/a","/vol/b"));
 struct exfat_metadata_audit observation;
 assert(!exfat_metadata_audit_end(audit_token+1,&observation) && metadata_audit_armed);
 assert(metadata_equal_media(&f,(uint8_t *)old_meta,old_size)==METADATA_EQUAL);
 assert(exfat_metadata_audit_end(audit_token,&observation) && observation.valid && !exfat_metadata_audit_end(audit_token,&observation));
 uint64_t next_token=exfat_metadata_audit_begin("/vol/a","/vol/b");assert(next_token!=audit_token);
 assert(!exfat_metadata_audit_end(audit_token,&observation) && metadata_audit_armed);
 assert(!exfat_metadata_audit_end(next_token,&observation));audit_owner=false;
 v.cluster_size=4096;
 f.size=f.valid_size=sizeof(expected);f.allocated_clusters=clusters_for_size(&v,f.size);
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
 assert(view.retained_bytes==sidecar_stats.retained_bytes);
 assert(reads==old_reads && view.checks==UINT64_MAX && view.shape==UINT64_MAX && view.fields[9]==UINT64_MAX);
 exfat_print_sidecar_equality_at(UINT64_MAX);
 assert(!line_bytes && max_line<1024 && emitted==686);
 /* begin<=207, six leases<=732, complete<=175, wire<=112, audio<=96. */
 assert(emitted+207+732+175+112+96<=2048);
 /* Separate retained-snapshot rendering from the original row/reserve budget. */
 struct sidecar_equality_stats saved_stats=sidecar_stats;
 sidecar_stats.retained_bytes++;sidecar_stats.equal++;
 unsigned before_lines=sidecar_lines;
 exfat_print_sidecar_trace(12345,&view);
 assert(sidecar_lines==before_lines+1 && last_sidecar[0]==12345 && last_sidecar[2]==view.equal && last_sidecar[5]==view.retained_bytes);
 exfat_print_sidecar_trace(67890,NULL);assert(sidecar_lines==before_lines+1);
 sidecar_stats=saved_stats;


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
 puts("metadata-audit: PASS all8 fields, namespace additions/removals, width/chunk boundaries, late failed read, token/nesting/stale refusal");
 puts("sidecar-equality: PASS complete uncached bytes, last-byte mismatch, unavailable read/allocation, exact length/validity, FAT termination/cycle refusal and bounded scratch");
}
