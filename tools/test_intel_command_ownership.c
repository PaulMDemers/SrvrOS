/* Actual native adapter and original source graph, modeled hardware only. */
#define _POSIX_C_SOURCE 200809L
#include <srvros/x86_cache.h>
#include <srvros/vmm.h>
static void intel_display_test_ownership_boundary(void);
static bool intel_display_test_ownership_load(unsigned,unsigned,unsigned,uint32_t *);
static bool intel_display_test_ownership_cpu(struct x86_cache_snapshot *,struct vmm_alias_observation *);
static bool intel_display_test_ownership_command(uint32_t *);
#define INTEL_COMMAND_FIXTURE_MAIN ownership_parent_fixture_main
#include "test_intel_command_native.c"
#undef INTEL_COMMAND_FIXTURE_MAIN
static unsigned own_ticks,own_reads,own_cpu;
static unsigned own_phase[23];
static bool own_hit;
static bool of(const char *mode,unsigned n){bool hit=jf(mode,n);if(hit)own_hit=true;return hit;}
static void intel_display_test_ownership_boundary(void) {
 ++own_ticks;
 if(command_ownership.groups<=22 && !own_phase[command_ownership.groups])own_phase[command_ownership.groups]=own_ticks;
 if(of("ownership-zero",own_ticks))ticks=UINT64_MAX;
 if(of("ownership-back",own_ticks))ticks=command_ownership.last-2;
 if(of("ownership-over",own_ticks))ticks=command_native.owner.lease.prepare+150000;
 if(of("ownership-root",own_ticks))command_ownership.root^=4096;
 if(of("ownership-hold",own_ticks))command_native.owner.hold.registered=NULL;
 if(of("ownership-time",own_ticks))command_ownership.last--;
 if(of("ownership-counter",own_ticks))command_ownership.reads++;
 if(of("ownership-dma",own_ticks))requester_command=7;
 if(of("ownership-hda",own_ticks))hda_command=6;
 if(of("ownership-epoch",own_ticks))command_native.owner.lease.epoch++;
 if(of("ownership-reenter",own_ticks)){
  assert(!command_ownership_run());assert(!intel_command_memory_close(&command_native.owner));
 }
}
static bool intel_display_test_ownership_load(unsigned kind,unsigned group,unsigned index,uint32_t *out) {
 assert(wake_irq && source_engines_native.result.state==ISE_HELD);
 /* Foreign DMA injected at the last clock can follow a valid COMMAND read.
  * RO observations grant no future authority; enclosing retirement must fail. */
 if(requester_command!=3)assert(!kind || (own_hit && strstr(engine_mode,"ownership-dma")));
 assert(intel_command_memory_consumer_live(&command_native.owner,&command_ownership));
 ++own_reads;
 if(!kind){*out=source_audit_native.pci[1][index];if(index==1)*out=(*out&0xffff0000u)|requester_command;}
 else if(kind==1)*out=nonzero_native.r.post.words[1][group][index];
 else if(kind==2){
  static const unsigned offsets[9]={0,1,2,5,6,7,8,9,10};*out=0;
  for(unsigned i=0;i<9;i++)if(index==offsets[i])*out=nonzero_native.r.readers.planes[group][i];
  if(index==4 || index==12)*out=group?0:nonzero_native.r.readers.planes[0][1];
 }else *out=(uint32_t)(window_result.sample[1].entry[index/3]>>(index%3==1?0:32));
 if(of("ownership-value",own_reads))*out^=4096;
 if(of("ownership-float",own_reads))*out=UINT32_MAX;
 return !of("ownership-read",own_reads);
}
static bool intel_display_test_ownership_command(uint32_t *out) {
 *out=(source_audit_native.pci[1][1]&0xffff0000u)|requester_command;return true;
}
static bool intel_display_test_ownership_cpu(struct x86_cache_snapshot *cache,struct vmm_alias_observation *alias) {
 ++own_cpu;*cache=command_native.cache;cache->begin_us=++ticks;cache->end_us=++ticks;
 *alias=(struct vmm_alias_observation){.pages=4,.leaves=4,.writable=1,.no_execute=1,.leaf_sizes=1};
 if(of("ownership-cache",own_cpu))cache->pat^=1;
 if(of("ownership-cache-context",own_cpu))cache->pat^=1ull<<56;
 if(of("ownership-alias",own_cpu))alias->writable=0;
 if(of("ownership-user",own_cpu))alias->user=1;
 if(of("ownership-nx",own_cpu))alias->no_execute=0;
 return !of("ownership-cpu",own_cpu);
}
static int ownership_case(int argc,char **argv) {
 int code=command_case(argc,argv);
 bool hardware_changed=own_hit && (strstr(argv[1],"ownership-dma") || strstr(argv[1],"ownership-hda"));
 if(command_ownership.complete && !hardware_changed){
  assert(command_ownership_returned() && !command_allocated && cm_frees==1);
  assert(own_reads==180 && own_cpu==2 && command_ownership.missing==25 && command_ownership.verified==6);
  assert(command_ownership.end<=source_engines_native.result.child_end);
 }else if(command_ownership.object){
  assert(own_hit && command_allocated && !cm_frees);
  if(!command_ownership.complete)assert(command_ownership.reason && command_ownership.registered);
  assert(!command_native.outer_safe && (requester_command==3 || requester_command==7));
 }
 if(own_hit && !hardware_changed && !strstr(argv[1],"ownership-reenter"))assert(!command_ownership.complete);
 printf("ownership-native-fixture: ticks=%u reads=%u cpu=%u hit=%u complete=%u registered=%u retired=%u\n",
 own_ticks,own_reads,own_cpu,own_hit,command_ownership.complete,command_ownership.registered,
 command_ownership.complete && command_native.owner.state==COMMAND_CLOSED && !command_allocated);
 printf("ownership-phases:");for(unsigned i=0;i<23;i++)printf(" phase%u=%u",i,own_phase[i]);puts("");
 return code;
}
int main(int argc,char **argv) {
#ifndef _WIN32
 if(argc==2 && !strcmp(argv[1],"--fixture-server"))return fixture_server(ownership_case);
#endif
 return ownership_case(argc,argv);
}
