/* Actual map/cleanup/image-transfer code, full65536-entry capacity; fake frames. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PROCESS_SERVICE_HOST_TEST
#include "../kernel/src/process.c"
static struct process image,other;
static unsigned allocations,frees,frames,retired,checkpoints;
static bool fail_storage,fail_frame,fail_map,initializing;
static unsigned char page[4096];
void *kmalloc(size_t n) {
 assert(n==PROCESS_MAX_MAPPINGS*sizeof(struct process_mapping));allocations++;
 if(fail_storage)return NULL;
 void *p=malloc(n);assert(p);memset(p,0xa5,n);initializing=true;return p;
}
void kfree(void *p){assert(p && !image.mapping_count && !image.mappings);frees++;free(p);}
void scheduler_io_checkpoint(void) {
 checkpoints++;
 if(initializing)assert(!image.mappings && !image.mapping_count);
}
void scheduler_wake_all(struct scheduler_wait_queue *q){(void)q;abort();}
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t min,enum pmm_frame_tag tag) {
 assert(n==1 && min==PROCESS_USER_PAGE_MIN_PHYSICAL && tag==PMM_FRAME_USER);
 assert(image.mappings);initializing=false;
 if(!image.mapping_count)for(unsigned i=0;i<PROCESS_MAX_MAPPINGS;i++)assert(!image.mappings[i].virtual_address && !image.mappings[i].physical_address && !image.mappings[i].flags);
 return fail_frame?0:4096ull*++frames;
}
void *pmm_phys_to_virt(uint64_t p){assert(p && !(p&4095));return page;}
void pmm_free_frame(uint64_t p){assert(p && !(p&4095));retired++;}
bool vmm_map_page_in_space(uint64_t root,uint64_t va,uint64_t pa,uint64_t flags) {
 assert(root==7 && va && pa && flags);return !fail_map;
}
bool vmm_unmap_page_in_space(uint64_t root,uint64_t va,uint64_t *out) {
 assert(root==7 && image.mapping_count);struct process_mapping *last=&image.mappings[image.mapping_count-1];
 assert(last->virtual_address==va);*out=last->physical_address;return true;
}
void console_printf(const char *s,...){(void)s;}
int main(void) {
 assert(PROCESS_MAX_MAPPINGS==65536 && PROCESS_MAX_PROCESSES==64);
 assert(sizeof(processes)<16*1024*1024);image.address_space=7;
 fail_storage=true;assert(!map_process_page(&image,0x1000,VMM_PAGE_USER));
 assert(!image.mappings && !image.mapping_count && !frames && !frees);
 fail_storage=false;fail_frame=true;
 assert(!map_process_page(&image,0x1000,VMM_PAGE_USER));
 assert(image.mappings && !image.mapping_count && checkpoints==24 && !frames);
 fail_frame=false;fail_map=true;
 assert(!map_process_page(&image,0x1000,VMM_PAGE_USER) && !image.mapping_count && retired==1);
 cleanup_process_mappings(&image);assert(!image.mappings && frees==1);
 frames=retired=0;fail_map=false;
 for(unsigned i=0;i<PROCESS_MAX_MAPPINGS;i++)assert(map_process_page(&image,0x10000000ull+i*4096,VMM_PAGE_USER));
 assert(image.mapping_count==65536 && frames==65536);
 unsigned attempts=allocations;assert(!map_process_page(&image,0x80000000,VMM_PAGE_USER) && frames==65536 && allocations==attempts);
 struct process_mapping *owned=image.mappings;struct process_mapping last=owned[65535];
 move_process_image(&other,&image);assert(other.mappings==owned && other.mapping_count==65536 && !image.mappings && !image.mapping_count);
 assert(!memcmp(&other.mappings[65535],&last,sizeof(last)));
 move_process_image(&image,&other);cleanup_process_mappings(&image);
 assert(!image.mappings && !image.mapping_count && retired==65536 && frees==2);
 attempts=allocations;assert(!map_process_page_mode(&image,0x1000,VMM_PAGE_USER,false) && allocations==attempts && !image.mappings);
 assert(map_process_page(&image,0x1000,VMM_PAGE_USER));cleanup_process_mappings(&image);
 assert(frees==3);cleanup_process_mappings(&image);assert(frees==3);
 puts("process-mapping-storage PASS: deferred full capacity, private zeroing, failure before page publication, 65536 entries, atomic exec transfer, cleanup and reuse");
}
