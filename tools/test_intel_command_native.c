#define _POSIX_C_SOURCE 200809L
#include <srvros/x86_cache.h>
#include <srvros/vmm.h>
static uint64_t intel_display_test_command_alloc(void);
static bool intel_display_test_command_backing(uint64_t,uint32_t);
static bool intel_display_test_command_free(uint64_t);
static bool intel_display_test_command_map(uint64_t,uint64_t,uint64_t);
static bool intel_display_test_command_unmap(uint64_t,uint64_t,uint64_t *);
static bool intel_display_test_command_cache(struct x86_cache_snapshot *);
static bool intel_display_test_command_alias(uint64_t,uint64_t,uint64_t,struct vmm_alias_observation *);
static volatile uint64_t *intel_display_test_command_pointer(void);
static void intel_display_test_command_preserved(void);
static void intel_display_test_command_boundary(void);
static uint64_t intel_display_test_command_clock(void);
static void intel_display_test_command_guard(void);
static void intel_display_test_command_return(void);
static bool intel_display_test_command_retire(void);
static uint64_t intel_display_test_command_dma(void);
#define INTEL_GGTT_FIXTURE_MAIN command_parent_fixture_main
#include "test_intel_ggtt_native.c"
#undef INTEL_GGTT_FIXTURE_MAIN
static uint64_t command_ram[2048],command_maps[4];
static bool command_allocated,cm_hit;
static bool cf(const char *name,unsigned n){bool hit=jf(name,n);if(hit)cm_hit=true;return hit;}
static unsigned cm_clocks,cm_backing,cm_alias,cm_cache,cm_allocs,cm_frees,cm_map,cm_unmap,cm_guard,cm_retire;
static uint64_t intel_display_test_command_alloc(void) {
 ++cm_allocs;if(cf("command-alloc",cm_allocs))return 0;assert(!command_allocated);command_allocated=true;for(unsigned i=0;i<2048;i++)command_ram[i]=0xa5a5a5a5a5a5a5a5ull;return 0x3000000;
}
static bool intel_display_test_command_backing(uint64_t pa,uint32_t pages) {
 ++cm_backing;return command_allocated && pa==0x3000000 && pages==4 && !cf("command-backing",cm_backing);
}
static bool intel_display_test_command_free(uint64_t pa) {
 assert(pa==0x3000000 && command_allocated);for(unsigned i=0;i<4;i++)assert(!command_maps[i]);
 ++cm_frees;if(cf("command-free",cm_frees))return false;command_allocated=false;return true;
}
static bool intel_display_test_command_map(uint64_t root,uint64_t va,uint64_t pa) {
 assert(root==0x1000 && va>=COMMAND_NATIVE_VA && va<COMMAND_NATIVE_VA+16384);
 unsigned i=(va-COMMAND_NATIVE_VA)/4096;assert(pa==0x3000000+4096*i && !command_maps[i]);++cm_map;
 if(cf("command-map",cm_map))return false;
 command_maps[i]=pa;return !cf("command-partial-map",cm_map);
}
static bool intel_display_test_command_unmap(uint64_t root,uint64_t va,uint64_t *pa) {
 assert(root==0x1000 && va>=COMMAND_NATIVE_VA && va<COMMAND_NATIVE_VA+16384);
 unsigned i=(va-COMMAND_NATIVE_VA)/4096;assert(command_maps[i]);++cm_unmap;
 if(cf("command-unmap",cm_unmap))return false;
 *pa=command_maps[i];command_maps[i]=0;if(cf("command-unmap-wrong",cm_unmap))*pa^=4096;return true;
}
static bool intel_display_test_command_cache(struct x86_cache_snapshot *out) {
 ++cm_cache;
 *out=(struct x86_cache_snapshot){.cpu={{0x756e6547,0x49656e69,0x6c65746e},0x306d4,0x91000,39,0x80050033},
 .pat=0x0007040600070406ull,.cap=0x402,.def_type=0x806,.base={0xb0000001,0},.mask={0x7ff0000800ull,0},
 .status=CACHE_OBSERVED,.reads=10,.count=2,.begin_us=ticks,.end_us=ticks+1};ticks++;
 if(cf("command-cache-drift",cm_cache))out->pat^=1;
 return !cf("command-cache",cm_cache);
}
static bool intel_display_test_command_alias(uint64_t root,uint64_t va,uint64_t pa,struct vmm_alias_observation *out) {
 assert(root==0x1000 && va==COMMAND_NATIVE_VA && pa==0x3000000);++cm_alias;
 for(unsigned i=0;i<4;i++)assert(command_maps[i]==pa+4096*i);
 *out=(struct vmm_alias_observation){.pages=4,.leaves=4,.writable=1,.no_execute=1,.leaf_sizes=1};
 if(cf("command-readonly",cm_alias))out->writable=0;
 if(cf("command-user",cm_alias))out->user=1;
 if(cf("command-exec",cm_alias))out->no_execute=0;
 if(cf("command-pat",cm_alias))out->pat_index=3;
 if(cf("command-large",cm_alias))out->leaf_sizes=2;
 return !cf("command-alias",cm_alias);
}
static volatile uint64_t *intel_display_test_command_pointer(void){return command_ram;}
static void intel_display_test_command_preserved(void) {if(cf("command-content",1))command_ram[2047]=1;}
static uint64_t intel_display_test_command_dma(void){return pmm_tagged_frames(PMM_FRAME_DMA)+(command_allocated?4:0);}
static bool intel_display_test_command_retire(void){++cm_retire;return !cf("command-retire",cm_retire);}
static void intel_display_test_command_boundary(void) {
 ++cm_clocks;struct intel_command_memory *o=&command_native.owner;
 if(cf("command-root",cm_clocks))o->root+=4096;
 if(cf("command-serial",cm_clocks))o->serial++;
 if(cf("command-io",cm_clocks))o->io.observe=NULL;
 if(cf("command-hold",cm_clocks))o->hold.lifetime=NULL;
 if(cf("command-registration",cm_clocks) && o->state==COMMAND_HELD)o->hold.registered=NULL;
 if(cf("command-epoch",cm_clocks) && o->state==COMMAND_HELD)o->lease.epoch++;
 if(cf("command-time",cm_clocks) && o->state==COMMAND_HELD)ticks=o->lease.prepare+150001;
}
static uint64_t intel_display_test_command_clock(void) {intel_display_test_command_boundary();return ++ticks;}
static void intel_display_test_command_guard(void) {
 ++cm_guard;
 if(cf("command-parent",cm_guard))command_native.parent=NULL;
 if(cf("command-hda",cm_guard))hda_command=6;
}
static void intel_display_test_command_return(void) {
 static unsigned calls;if(++calls!=1)return;
 struct intel_command_memory *o=&command_native.owner;
 if(cf("command-final",1))o->complete=false;
 if(cf("command-final",2))o->maps--;
 if(cf("command-final",3))o->lease.owner=NULL;
 if(cf("command-final",4))o->hold.lifetime=NULL;
 if(cf("command-final",5))o->lease.prepare++;
 if(cf("command-final",6))o->end++;
 if(cf("command-final",7))command_native.cache_checks--;
 if(cf("command-final",8))o->alias.nx=0;
}
static unsigned cm_core_parent,cm_core_lifetime,cm_core_calls,cm_core_bad;
static uint64_t cm_core_time,cm_core_limit;
static bool cm_core_over,cm_core_frozen,cm_core_zero,cm_core_back;
static struct intel_command_lease cm_core_lease;
static bool command_core_guard(void *ctx,const struct intel_command_lease *l) {
 ++cm_guard;
 return ctx==&command_native && l==&command_native.owner.lease &&
 l->owner==&cm_core_parent && l->provider==&command_native && l->lifetime==&cm_core_lifetime &&
 l->root==0x1000 && l->epoch==7 && l->sequence==1 && l->prepare==cm_core_lease.prepare &&
 l->gt==cm_core_lease.gt && l->hda==cm_core_lease.hda && l->outer==cm_core_lease.outer;
}
static uint64_t command_core_clock(void *ctx) {
 assert(ctx==&command_native);++cm_core_calls;
 if(cm_core_calls==cm_core_bad){
  if(cm_core_zero)return 0;
  if(cm_core_back)return command_native.owner.last-1;
 }
 if(cm_core_frozen)return cm_core_time;
 if(cm_core_calls==10)cm_core_time=cm_core_limit+cm_core_over;
 else ++cm_core_time;
 return cm_core_time;
}
static bool command_core_retired(void *ctx,const struct intel_command_lease *l) {
 return ctx==&command_native && l->owner==&cm_core_parent && l->root==0x1000 && command_native.outer_safe;
}
static int command_core_case(const char *mode) {
 reset();assert(command_prepare());
 cm_core_lease=(struct intel_command_lease){&cm_core_parent,&command_native,&cm_core_lifetime,0x1000,7,1,1000000,1000000,1000000,1000000};
 unsigned domain=3;
 if(strstr(mode,"core-gt-")){domain=4;cm_core_lease.prepare=1250000;cm_core_lease.hda=1250000;cm_core_limit=1300000;}
 else if(strstr(mode,"core-hda-")){domain=5;cm_core_lease.prepare=1350000;cm_core_lease.gt=1250000;cm_core_limit=1400000;}
 else if(strstr(mode,"core-outer-")){domain=6;cm_core_lease.prepare=1450000;cm_core_lease.gt=1300000;cm_core_lease.hda=1300000;cm_core_limit=1500000;}
 else cm_core_limit=1000000+INTEL_DIAGNOSTIC_PREPARE_US;
 cm_core_over=strstr(mode,"-over")!=NULL;cm_core_frozen=strstr(mode,"core-frozen")!=NULL;
 cm_core_zero=strstr(mode,"core-zero-")!=NULL;cm_core_back=strstr(mode,"core-back-")!=NULL;
 if(cm_core_zero || cm_core_back){const char *last=strrchr(mode,'-');assert(last && sscanf(last+1,"%u",&cm_core_bad)==1 && cm_core_bad>=1 && cm_core_bad<=10);}
 cm_core_time=cm_core_frozen?1000001:cm_core_limit-10;
 struct intel_command_memory *o=&command_native.owner;
 o->io.guard=o->original_io.guard=command_core_guard;o->io.retired=o->original_io.retired=command_core_retired;
 o->io.now=o->original_io.now=command_core_clock;
 bool ok=intel_command_memory_run(o,&cm_core_lease);
 if(cm_core_over || cm_core_frozen || cm_core_bad){
  assert(!ok && o->retained && !o->complete && (o->reason&COMMAND_CLOCK) && !cm_frees && command_allocated);
  if(cm_core_over)assert(o->clock_at==cm_core_limit+1 && o->clock_reason==domain);
  if(cm_core_frozen)assert(o->clock_reason==9);
  if(cm_core_zero)assert(!o->clock_at && o->clock_reason==1);
  if(cm_core_back)assert(o->clock_reason==2);
 } else {
  assert(ok && o->end==cm_core_limit && intel_command_memory_returned(o));
  command_native.outer_safe=true;assert(intel_command_memory_close(o) && !command_allocated && cm_frees==1);
 }
 printf("command-core-boundary: PASS policy=%llu domain=%u refused=%u clocks=%u; synthetic parent only\n",
 (unsigned long long)INTEL_DIAGNOSTIC_PREPARE_US,domain,!ok,cm_core_calls);return 0;
}
static int command_case(int argc,char **argv) {
 if(argc==2 && strstr(argv[1],"command-core-"))return command_core_case(argv[1]);
 int code=ggtt_case(argc,argv);struct intel_command_memory *o=&command_native.owner;
 if(o->state==COMMAND_CLOSED){assert(!command_allocated && cm_allocs==1 && cm_frees==1 && cm_map==4 && cm_unmap==4);
  assert(o->maps==4 && o->retired==4 && o->observations==4 && o->guards==2 && o->checks==1 && o->complete);
  assert(cm_guard==2 && cm_retire==2 && cm_alias==4 && cm_cache==5 && command_native.zeros==1 && command_native.word_checks==2048);
  assert(!cm_hit);
  for(unsigned i=0;i<2048;i++)assert(!command_ram[i]);}
 if(o->state==COMMAND_FAULT && o->physical && o->retired<4)assert(command_allocated && !cm_frees);
 printf("command-native-fixture: clocks=%u backing=%u aliases=%u cache=%u maps=%u unmaps=%u guards=%u allocated=%u frees=%u state=%u complete=%u retained=%u\n",
 cm_clocks,cm_backing,cm_alias,cm_cache,cm_map,cm_unmap,cm_guard,cm_allocs,cm_frees,o->state,o->complete,o->retained);
 return code;
}
int main(int argc,char **argv) {
#ifndef _WIN32
 if(argc==2 && !strcmp(argv[1],"--fixture-server"))return fixture_server(command_case);
#endif
 return command_case(argc,argv);
}
