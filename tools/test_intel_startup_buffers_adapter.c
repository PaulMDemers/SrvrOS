#include <stdint.h>
#include <stdbool.h>
static void *intel_display_test_buffer_pointer(uint64_t);
static bool intel_display_test_buffer_translation(uint64_t,uint64_t *);
static void intel_display_test_buffer_admission(void);
static bool intel_display_test_buffer_clock(uint64_t *);
#define INTEL_STARTUP_FIXTURE_MAIN original_startup_fixture_main
#include "test_intel_startup_source_adapter.c"
static unsigned char buffer_ram[12*1024*1024];
static struct {uint64_t physical,pages;bool active;} buffer_spans[2];
static unsigned buffer_allocations,buffer_clocks,buffer_guards;
static bool tag_lost;
static uint64_t buffer_offset;
uint64_t pmm_alloc_frames_tagged(uint64_t pages,enum pmm_frame_tag tag) {
 assert(tag==PMM_FRAME_DMA);
 if(jf("buffer-alloc",buffer_allocations+1))return 0;
 assert(buffer_allocations<2 && buffer_offset+(pages+1)*4096<=sizeof buffer_ram);
 uint64_t physical=0x1000000+buffer_offset;
 buffer_spans[buffer_allocations].physical=physical;buffer_spans[buffer_allocations].pages=pages;buffer_spans[buffer_allocations++].active=true;
 buffer_offset+=(pages+1)*4096;return physical;
}
static void *intel_display_test_buffer_pointer(uint64_t pa) {
 assert(pa>=0x1000000 && pa<0x1000000+sizeof buffer_ram);return buffer_ram+pa-0x1000000;
}
static bool intel_display_test_buffer_translation(uint64_t pa,uint64_t *actual){*actual=pa;return true;}
bool pmm_frame_has_tag(uint64_t pa,enum pmm_frame_tag tag) {
 assert(tag==PMM_FRAME_DMA);if(tag_lost)return false;
 for(unsigned i=0;i<buffer_allocations;i++)if(buffer_spans[i].active && buffer_spans[i].physical<=pa && pa<buffer_spans[i].physical+buffer_spans[i].pages*4096)return true;
 return false;
}
void pmm_free_frame_range(uint64_t pa,uint64_t pages) {
 for(unsigned i=0;i<buffer_allocations;i++)if(buffer_spans[i].physical==pa){assert(buffer_spans[i].active && buffer_spans[i].pages==pages);buffer_spans[i].active=false;return;}
 assert(false);
}
uint64_t pmm_tagged_frames(enum pmm_frame_tag tag) {
 assert(tag==PMM_FRAME_DMA);uint64_t pages=17;
 for(unsigned i=0;i<buffer_allocations;i++)if(buffer_spans[i].active)pages+=buffer_spans[i].pages;
 return pages;
}
static bool intel_display_test_buffer_clock(uint64_t *at) {
 ++buffer_clocks;
 struct intel_source_buffers *b=&buffers_native.binding;
 if(jf("buffer-zero",buffer_clocks)){*at=0;return true;}
 if(jf("buffer-backward",buffer_clocks)){*at=1;return true;}
 if(jf("buffer-time",buffer_clocks))ticks=b->origin+100001;
 if(jf("buffer-parent-time",buffer_clocks))ticks=b->parent_begin+300001;
 if(jf("buffer-epoch",buffer_clocks))b->epoch++;
 if(jf("buffer-provider",buffer_clocks))b->io.ctx=0;
 if(jf("buffer-source",buffer_clocks))b->source=0;
 if(jf("buffer-post",buffer_clocks))nonzero_native.r.post.words[0][0][0]^=1;
 if(jf("buffer-route",buffer_clocks))nonzero_native.r.entry_kind=3-nonzero_native.r.entry_kind;
 if(jf("buffer-mode",buffer_clocks))nonzero_native.r.startup=false;
 if(jf("buffer-registration",buffer_clocks))nonzero_native.source.registered_child=0;
 if(jf("buffer-hold",buffer_clocks))buffers_native.hold[0].memory.epoch++;
 if(jf("buffer-physical",buffer_clocks))buffers_native.memory[0].physical+=4096;
 if(jf("buffer-layout",buffer_clocks))buffers_native.memory[1].layout.pages--;
 if(jf("buffer-tag",buffer_clocks))tag_lost=true;
 if(jf("buffer-root",buffer_clocks))buffers_native.hold[0].lifetime=0;
 if(jf("buffer-owner",buffer_clocks))for(unsigned i=0;i<2;i++) {
  ++buffers_native.memory[i].owner;++buffers_native.hold[i].memory.owner;
 }
 if(jf("buffer-memory-object",buffer_clocks)) {
  static struct scanout_memory copy;copy=buffers_native.memory[0];copy.object=&copy;
  buffers_native.hold[0].memory.object=&copy;
 }
 if(jf("buffer-plan",buffer_clocks) && b->planned)b->page[0].replacement^=4096;
 return false;
}
static void intel_display_test_buffer_admission(void) {
 ++buffer_guards;
 struct intel_source_buffers *b=&buffers_native.binding;
 assert(intel_source_buffers_live(b));
 assert(!scanout_memory_hold_end(&buffers_native.hold[0]) && !scanout_memory_hold_end(&buffers_native.hold[1]));
 struct scanout_memory_cpu cpu;
 assert(!scanout_memory_cpu_begin(&buffers_native.memory[0],&buffers_native.token[0],&cpu));
 struct intel_source_buffers copy=*b;assert(!intel_source_buffers_live(&copy));
 static struct scanout_memory_hold held_copy;held_copy=buffers_native.hold[0];assert(!scanout_memory_hold_end(&held_copy));
 if(jf("buffer-guard",buffer_guards))b->hold[0]=&held_copy;
}
int main(int argc,char **argv) {
 puts("init: deferred=startup-buffers-preinit; terminal automatic capture, no user services");
 int code=original_startup_fixture_main(argc,argv);
 printf("source-buffers-fixture: clocks=%u guards=%u allocations=%u retained=%u\n",buffer_clocks,buffer_guards,buffer_allocations,(unsigned)buffers_native.retained);
 return code;
}
