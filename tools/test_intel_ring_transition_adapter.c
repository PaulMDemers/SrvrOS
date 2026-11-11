/* Actual native adapter with modeled PCI/MMIO, one original root/exclusion. */
#include <stdint.h>
#include <stdbool.h>
static uint64_t intel_display_test_hda_irq_save(void);
static bool intel_display_test_hda_irq_off(void);
static void intel_display_test_hda_irq_restore(uint64_t);
static bool intel_display_test_hda_uc(uint64_t);
static bool intel_display_test_hda_read(unsigned,unsigned,uint32_t *);
static uint32_t intel_display_test_hda_config(unsigned);
static bool intel_display_test_hda_map(void);
static bool intel_display_test_hda_retire(void);
static void intel_display_test_joint_admission(void);
static void intel_display_test_joint_prepare(void);
static void intel_display_test_ring_admission(void *);
static bool intel_display_test_ring_read(uint32_t [8]);
#include <srvros/intel_gt_access.h>
static enum intel_gt_write intel_display_test_ring_write(uint32_t);
static bool intel_display_test_ring_reader(unsigned,unsigned,uint32_t *);
static bool intel_display_test_ring_census(unsigned,unsigned,uint32_t *);
static bool intel_display_test_ring_context(unsigned,uint32_t *);
static bool intel_display_test_ring_clock(uint64_t *);
static bool intel_display_test_ring_core_clock(uint64_t *);
static unsigned ring_core_clocks;
static bool ring_return_faulted;
static uint32_t ring_ctl=0xf001;
static unsigned ring_writes,ring_reads,ring_census_reads,ring_reader_calls,ring_clocks,ring_store_clock_fault;

struct pci_device;
static bool intel_display_test_hda_command16(const struct pci_device *,unsigned,uint16_t);
#define main original_display_fixture_main
#include "test_intel_display_probe.c"
#undef main
static unsigned hda_reads,hda_cache,hda_attempts,hda_stores,hda_endpoints;
static uint16_t hda_command=6;
static bool reentered;
static bool jf(const char *name,unsigned point) {
 const char *colon=strchr(engine_mode,':');
 return colon && (size_t)(colon-engine_mode)==strlen(name) && !strncmp(engine_mode,name,strlen(name)) &&
  point==(unsigned)atoi(colon+1);
}
static uint64_t intel_display_test_hda_irq_save(void){wake_irq=true;return 512;}
static bool intel_display_test_hda_irq_off(void){return wake_irq;}
static void intel_display_test_hda_irq_restore(uint64_t f){assert(f==512);wake_irq=false;}
static bool intel_display_test_hda_uc(uint64_t p){assert(p==INTEL_HDA_BASE && wake_irq && !display_access_allowed());return !jf("joint-cache",++hda_cache);}
static bool intel_display_test_hda_map(void){return !jf("joint-map",1);}
static bool intel_display_test_hda_retire(void){assert(!joint.scope.gpu->pinned && !joint.scope.gpu->borrows && !joint.scope.gpu->dependents);return !jf("joint-retire",1);}
static uint32_t intel_display_test_hda_config(unsigned off) {
 if(joint.scope.busy && joint.scope.state==IHG_ACTIVE && joint.scope.hda.busy && !reentered) {
  reentered=true;unsigned before=hda_endpoints;
  assert(!intel_hda_gpu_check(&joint.scope) && !intel_hda_gpu_close(&joint.scope,false));
  assert(!intel_hda_owner_unpin(&joint.scope.hda,&joint.scope.borrow));
  assert(!intel_hda_owner_validate(&joint.scope.hda,&joint.scope.borrow));
  assert(before==hda_endpoints);
 }
 if(off==0)++hda_endpoints;
 if(!strcmp(engine_mode,"blit-link-change") || !strcmp(engine_mode,"blit-ltr-change")) {
  if(off==0x40)return 0x00035001;
  if(off==0x50)return 0x00020010;
  if(off==0x5c)return ring_reader_calls>27 && !strcmp(engine_mode,"blit-link-change")?2:1;
  if(off==0x74)return ring_reader_calls>27 && !strcmp(engine_mode,"blit-ltr-change")?2:1;
 }
 if(off==0x44 && ring_reader_calls>27 && !strcmp(engine_mode,"blit-audio-change"))return 3;
 switch(off) {
 case 0:return jf("joint-endpoint",hda_endpoints)?0xffffffff:0x160c8086;
 case 4:return 0x00100000|hda_command;
 case 8:return 0x04030009;
 case 0x2c:return 0x011b106b;
 case 0x10:return 0xc1610004;
 case 0x34:return 0x40;
 case 0x40:return 0x00030001;
 default:return 0;
 }
}
static bool intel_display_test_hda_read(unsigned off,unsigned width,uint32_t *out) {
 assert(wake_irq && !display_access_allowed() && intel_hda_read_allowed(off,width));
 assert(joint.inspection && joint.h.mapped);++hda_reads;
 if(jf("joint-read",hda_reads))return false;
 *out=off==0?0x3001:off==8?1:off>=0x80 && off<0xe0 && (off-0x80)%32==2?4:0;
 if(jf("joint-drift",hda_reads))*out^=1;
 if(jf("joint-read-time",hda_reads))ticks+=400001;
 if(jf("joint-alias",hda_reads))pa[INTEL_CAPTURE_PAGES+15]^=4096;
 return true;
}
static void intel_display_test_joint_admission(void) {
 if(jf("joint-store-time",hda_attempts+1))ticks+=400001;}
static void intel_display_test_joint_prepare(void) {if(jf("joint-upper",1))backing.pte[0]|=1ull<<32;}
static bool intel_display_test_hda_command16(const struct pci_device *d,unsigned off,uint16_t value) {
 assert(d==&devices[3] && off==4 && wake_irq && !display_access_allowed() && !joint.inspection);
 ++hda_attempts;assert(hda_attempts<=2 && value==(hda_attempts==1?2:6));
 if(value==6)assert(!joint.scope.gpu->pinned && !joint.scope.gpu->borrows && !joint.scope.gpu->dependents);
 if(jf("joint-none",hda_attempts))return false;
 hda_command=value;++hda_stores;
 return !jf("joint-uncertain",hda_attempts);
}
static bool intel_display_test_ring_read(uint32_t out[8]) {
 ++ring_reads;assert(wake_irq && wake_ack==1 && !display_access_allowed() && hda_command==2);
 if(ring_reads==1) {
  struct wake_adapter *a=ring_native.a;struct intel_gt_access *g=ring_native.g;const struct intel_completion_result *c=ring_native.c;
  assert(!ring_run(NULL,NULL,NULL,true));assert(ring_native.a==a && ring_native.g==g && ring_native.c==c);
  assert(!intel_gt_access_close(g) && ring_native.result.borrow.active);
 }
 const uint32_t words[8]={0,0,0x3eef000,ring_ctl,1,0,512,0};for(unsigned i=0;i<8;i++)out[i]=words[i];
 if(jf("ring-read",ring_reads))return false;
 if(jf("ring-drift",ring_reads))out[7]^=1;
 if(jf("ring-busy",ring_reads))out[0]=8;
 if(jf("ring-float",ring_reads))out[0]=UINT32_MAX;
 if(jf("ring-time",ring_reads))ticks+=150001;
 if(jf("ring-epoch",ring_reads))ring_native.g->engine_epoch++;
 if(jf("ring-borrow",ring_reads))ring_native.g->dependents=NULL;
 if(jf("ring-alias",ring_reads))pa[INTEL_CAPTURE_PAGES+8]^=4096;
 if(jf("ring-source-begin",ring_reads))((struct intel_completion_result *)ring_native.c)->begin_us++;
 if(jf("ring-source-last",ring_reads))((struct intel_completion_result *)ring_native.c)->last_us++;
 if(jf("ring-extra-borrow",ring_reads))ring_native.g->borrows++;
 if(jf("ring-source-age",ring_reads))ticks=ring_native.result.source_end+100001;
 if(jf("ring-copy-census",ring_reads)) {
  static struct intel_completion_result copy;copy=*ring_native.c;copy.object=&copy;ring_native.result.source=&copy;
 }
 if(jf("ring-copy-owner",ring_reads)) {
  static struct intel_gt_access copy;copy=*ring_native.g;copy.object=&copy;ring_native.result.parent=&copy;
 }
 if(jf("ring-copy-provider",ring_reads)) {
  static struct wake_adapter copy;copy=*ring_native.a;ring_native.a=&copy;
 }
 if(jf("ring-source-time",ring_reads))((struct intel_completion_result *)ring_native.c)->end_us++;
 if(ring_reads==1)for(unsigned i=0;i<92;i++)if(jf("ring-source-word",i+1)) {
  struct intel_completion_result *c=(struct intel_completion_result *)ring_native.c;
  if(i<80)c->words[i/40][(i%40)/8][i%8]^=1;else c->context[(i-80)/6][(i-80)%6]^=1;
 }
 return true;
}
static enum intel_gt_write intel_display_test_ring_write(uint32_t value) {
 assert(value==0xf000 && !ring_writes && ring_ctl==0xf001 && ring_native.result.epoch_after==1 && ring_native.g->engine_epoch==1 && ring_native.result.borrow.active);
 if(!strcmp(engine_mode,"ring-store-none"))return GT_WRITE_NONE;
 ++ring_writes;ring_ctl=value;
 return !strcmp(engine_mode,"ring-store-uncertain")?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
static void intel_display_test_ring_admission(void *ctx) {
 assert(ctx==&ring_native);
 if(!strcmp(engine_mode,"ring-store-zero"))ring_store_clock_fault=1;
 if(!strcmp(engine_mode,"ring-store-backward"))ring_store_clock_fault=2;
 if(!strcmp(engine_mode,"ring-store-core"))ring_store_clock_fault=3;
 if(!strcmp(engine_mode,"ring-store-phase"))ring_store_clock_fault=4;
 if(!strcmp(engine_mode,"ring-store-owner"))ring_store_clock_fault=5;
 if(!strcmp(engine_mode,"ring-store-age"))ring_store_clock_fault=6;
 if(!strcmp(engine_mode,"ring-store-source"))((struct intel_completion_result *)ring_native.c)->words[0][4][2]^=4096;
 if(!strcmp(engine_mode,"ring-store-copy")) {
  static struct intel_completion_result copy;copy=*ring_native.c;copy.object=&copy;ring_native.result.source=&copy;
 }
}
static bool intel_display_test_ring_census(unsigned e,unsigned i,uint32_t *out) {
 ++ring_census_reads;
 bool ok=i<6?intel_display_test_engine_read(e,i,out):(*out=(!strcmp(engine_mode,"flip-wait") && i==6)?0x100000:(!strcmp(engine_mode,"tlb-pending") && i==7)?1:0,true);
 if(jf("ring-census-read",ring_census_reads))return false;
 if(jf("ring-census-drift",ring_census_reads))*out^=1;
 if(jf("ring-census-time",ring_census_reads))ticks+=150001;
 return ok;
}
static bool intel_display_test_ring_context(unsigned i,uint32_t *out) {
 (void)i;++ring_census_reads;
 if(jf("ring-census-read",ring_census_reads))return false;
 if(jf("ring-census-drift",ring_census_reads))*out^=1;
 if(jf("ring-census-time",ring_census_reads))ticks+=150001;
 return true;
}
static bool intel_display_test_ring_reader(unsigned p,unsigned i,uint32_t *out) {
 ++ring_reader_calls;assert(wake_irq && hda_command==2 && ring_native.result.borrow.active);
 *out=(!p && !i)?0xd8000400u:0;
 if(jf("ring-reader-read",ring_reader_calls))return false;
 if(jf("ring-reader-drift",ring_reader_calls))*out^=1;
 if(jf("ring-reader-time",ring_reader_calls))ticks+=150001;
 if(ring_native.result.reader_attempts==2 && !p && !i)for(unsigned k=0;k<92;k++)if(jf("ring-post-word",k+1)) {
  struct intel_completion_result *c=&ring_native.result.post;
  if(k<80)c->words[k/40][(k%40)/8][k%8]^=1;else c->context[(k-80)/6][(k-80)%6]^=1;
 }
 return true;
}
static bool intel_display_test_ring_core_clock(uint64_t *out) {
 ++ring_core_clocks;
 if(jf("ring-core-zero",ring_core_clocks)){*out=0;return true;}
 if(jf("ring-core-backward",ring_core_clocks)){*out=1;return true;}
 if(jf("ring-core-time",ring_core_clocks))ticks+=150001;
 return false;
}
static bool intel_display_test_ring_clock(uint64_t *out) {
 const struct intel_ring_transition *r=&ring_native.result;
 if(ring_store_clock_fault && ring_native.store_checks) {
  unsigned f=ring_store_clock_fault;ring_store_clock_fault=0;
  if(f<3){*out=f==1?0:1;return true;}
  ticks=f==6?r->source_end+100000:f==3?r->bcs.begin_us+100000:f==4?r->begin+150000:ring_native.g->begin+300000;
 }
 if(r->returned && !r->end && !ring_return_faulted && !strcmp(engine_mode,"ring-return-zero")) {
  ring_return_faulted=true;*out=0;return true;
 }
 if(r->borrow.active || (r->returned && !r->end && ring_native.g->state==GT_ACCESS_AWAKE)) {
  ++ring_clocks;
  if(jf("ring-clock-zero",ring_clocks)){*out=0;return true;}
  if(jf("ring-clock-backward",ring_clocks)){*out=1;return true;}
  if(jf("ring-clock-time",ring_clocks))ticks+=150001;
 }
 return false;
}
__attribute__((noinline)) static void churn_stack(void) {
 volatile uint8_t bytes[32768];
 for(unsigned i=0;i<sizeof(bytes);i++)bytes[i]=(uint8_t)(i*17+3);
}
int main(int argc,char **argv) {
 if(argc==2 && (!strcmp(argv[1],"--wake-engine-ring-off") || !strcmp(argv[1],"--wake-engine-ring-zero")))ring_ctl=0;
 int code=original_display_fixture_main(argc,argv);churn_stack();
 if(joint.entry)assert(joint.wake && joint.snapshot);
 if(joint.scope.object) {
  const struct intel_gt_access *g=joint.scope.gpu;
  assert(g && (!g->object || (g->object==g && g->io.context==joint.wake)));
  assert(!joint.scope.claim && !joint.scope.attachments && !joint.scope.gpu_complete && !joint.scope.complete);
 }
 const struct intel_ring_transition *r=&ring_native.result;
 if(r->object) {
  assert(r->object==r && (strstr(engine_mode,"ring-copy-owner:") || r->parent==joint.scope.gpu) && (!strcmp(engine_mode,"ring-store-copy") || strstr(engine_mode,"ring-copy-census:") || r->source==ring_native.c));
  if(r->borrow.object)assert(r->borrow.object==&r->borrow);
  if(r->uncertain)assert(joint.scope.gpu->pinned && !display_access_allowed());
 }
 if(!strcmp(engine_mode,"report"))assert(r->complete && ring_writes==1 && ring_reads==4 && r->epoch_after==1 && r->post.engine_epoch==1 && r->returned && !r->borrow.active);
 printf("ring-fixture: hda_reads=%u endpoints=%u cache=%u attempts=%u stores=%u bcs_reads=%u bcs_stores=%u census_reads=%u reader_reads=%u clocks=%u core_clocks=%u\n",hda_reads,hda_endpoints,hda_cache,hda_attempts,hda_stores,ring_reads,ring_writes,ring_census_reads,ring_reader_calls,ring_clocks,ring_core_clocks);
 return code;
}
