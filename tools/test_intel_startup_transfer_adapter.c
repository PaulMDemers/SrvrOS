#include <srvros/intel_source_transfer.h>
static bool intel_display_test_transfer_clock(uint64_t *);
static bool intel_display_test_transfer_admission(void);
static bool intel_display_test_transfer_provider(void *,const struct scanout_transfer_manifest *,enum scanout_transfer_phase);
static void intel_display_test_transfer_evidence(struct intel_source_transfer_evidence *);
static void intel_display_test_transfer_active(struct intel_source_transfer *);
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
int transfer_fixture_main(int argc,char **argv) {
#ifdef SRVROS_INTEL_SOURCE_REQUESTER
 puts("init: deferred=source-requester-preinit; terminal automatic capture, no user services");
#else
 puts("init: deferred=startup-transfer-preinit; terminal automatic capture, no user services");
#endif
 int code=original_startup_fixture_main(argc,argv);
 printf("source-buffers-fixture: clocks=%u guards=%u allocations=%u retained=%u\n",buffer_clocks,buffer_guards,buffer_allocations,(unsigned)buffers_native.retained);
 return code;
}

static unsigned transfer_clocks,transfer_guards,transfer_phases[3];
static bool transfer_model,transfer_drain,transfer_reenter,transfer_stall;
static uint32_t model_assurances=0,model_obligations=255;
static void intel_display_test_transfer_evidence(struct intel_source_transfer_evidence *e) {
 e->assurances=model_assurances;e->obligations=model_obligations;
}
static bool intel_display_test_transfer_clock(uint64_t *at) {
 ++transfer_clocks;struct intel_source_transfer *t=&source_transfer_native;
 if(transfer_stall){*at=t->begin;return true;}
 if(jf("transfer-zero",transfer_clocks)){*at=0;return true;}
 if(jf("transfer-backward",transfer_clocks)){*at=1;return true;}
 if(jf("transfer-time",transfer_clocks))ticks=t->origin+100001;
 if(jf("transfer-parent-time",transfer_clocks))ticks=t->parent_begin+300001;
 if(jf("transfer-source",transfer_clocks))t->source=0;
 if(jf("transfer-provider",transfer_clocks))t->provider=0;
 if(jf("transfer-epoch",transfer_clocks))t->epoch++;
 if(jf("transfer-mode",transfer_clocks))t->post_mode^=3;
 if(jf("transfer-physical",transfer_clocks))t->physical[0]+=4096;
 if(jf("transfer-owner",transfer_clocks))t->memory[0].owner++;
 if(jf("transfer-token",transfer_clocks))t->memory[0].epoch++;
 if(jf("transfer-hold",transfer_clocks))t->hold[0]=0;
 if(jf("transfer-serial",transfer_clocks))t->serial[0]++;
 if(jf("transfer-root",transfer_clocks))t->lifetime=0;
 if(jf("transfer-registration",transfer_clocks))t->buffers->registered_child=0;
 if(jf("transfer-io",transfer_clocks))t->io.ctx=0;
 if(jf("transfer-busy",transfer_clocks))t->busy=false;
 if(jf("transfer-state",transfer_clocks))t->state=IST_ACTIVE;
 return false;
}
static bool intel_display_test_transfer_admission(void) {
 ++transfer_guards;struct intel_source_transfer *t=&source_transfer_native;
 assert(intel_source_transfer_live(t) && t->busy);
 static struct intel_source_transfer copy;copy=*t;assert(!intel_source_transfer_live(&copy));
 assert(!intel_source_transfer_check(t) && !intel_source_transfer_close(t));
 assert(!intel_source_transfer_open(t,t->buffers,&t->exclusion,(struct intel_source_transfer_evidence){0,255},t->io));
 assert(t->buffers->dependent==t && !scanout_memory_hold_end(t->hold[0]));
 return !jf("transfer-guard",transfer_guards);
}
static bool intel_display_test_transfer_provider(void *ctx,const struct scanout_transfer_manifest *m,enum scanout_transfer_phase phase) {
 assert(transfer_model && ctx==&nonzero_native && phase<=TRANSFER_RETIRE);
 struct intel_source_transfer *t=&source_transfer_native;
 assert(m->source_epoch==t->source->epoch && m->original[0]==t->source->expected && m->pages==1);
 transfer_phases[phase]++;
 if(transfer_reenter)assert(!intel_source_transfer_check(t) && !intel_source_transfer_close(t));
 return !jf("transfer-provider-fail",(unsigned)phase+1);
}
static void intel_display_test_transfer_active(struct intel_source_transfer *t) {
 assert(transfer_model && t->opened && !t->refused && t->owner.pinned && t->owner.manifest.pages==1);
 uint64_t original;assert(scanout_transfer_original(&t->owner,&t->token,8704,&original) && original==t->source->expected);
 if(transfer_drain) {
  struct scanout_space_token lease;
  assert(scanout_space_reserve(&t->owner.space,t->owner.owner,t->owner.arena_epoch,1,1,&lease));
  assert(!intel_source_transfer_close(t) && t->state==IST_CLOSING && t->registered && t->buffers->dependent==t);
  struct scanout_space_token other;assert(!scanout_space_reserve(&t->owner.space,t->owner.owner,t->owner.arena_epoch,1,1,&other));
  assert(intel_source_transfer_check(t));
  assert(scanout_space_release(&t->owner.space,&lease));
 }
}
#ifndef INTEL_TRANSFER_FIXTURE_MAIN
#define INTEL_TRANSFER_FIXTURE_MAIN main
#endif
int INTEL_TRANSFER_FIXTURE_MAIN(int argc,char **argv) {
 if(argc==2) {
  transfer_stall=strstr(argv[1],"transfer-stall")!=0;
  transfer_model=strstr(argv[1],"transfer-model") || strstr(argv[1],"transfer-provider-fail");
  transfer_drain=strstr(argv[1],"transfer-model-drain")!=0;
  transfer_reenter=strstr(argv[1],"transfer-model-reenter")!=0;
  if(transfer_model){model_assurances=HANDOFF_ALL;model_obligations=0;}
  const char *p=strstr(argv[1],"transfer-missing-assurance:");if(p){model_assurances=HANDOFF_ALL^(1u<<atoi(p+27));model_obligations=0;}
  p=strstr(argv[1],"transfer-open-obligation:");if(p){model_assurances=HANDOFF_ALL;model_obligations=1u<<atoi(p+25);}
 }
 int code=transfer_fixture_main(argc,argv);
 printf("source-transfer-fixture: clocks=%u guards=%u admit=%u live=%u retire=%u model=%u\n",transfer_clocks,transfer_guards,transfer_phases[0],transfer_phases[1],transfer_phases[2],(unsigned)transfer_model);
 if(source_transfer_native.closed) {
  assert(!intel_source_transfer_check(&source_transfer_native) && !intel_source_transfer_close(&source_transfer_native));
  assert(source_transfer_native.state==IST_CLOSED && source_transfer_native.returns==1);
 }
 if(transfer_model && !source_transfer_native.reason)assert(source_transfer_native.closed && !source_transfer_native.owner.pinned && source_transfer_native.callbacks>=3);
 return code;
}
