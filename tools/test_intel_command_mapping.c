/* Actual CPU owner + transfer/space/publication cores. Hardware ownership is
 * synthetic in this host fixture; this is never native acceptance. */
#define _POSIX_C_SOURCE 200809L
#include <srvros/intel_command_mapping.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixture_server.h"
static struct intel_command_memory cpu;
static struct intel_command_mapping mapping,copy;
static struct scanout_transfer transfer;
static struct scanout_transfer_pool transfers;
static struct scanout_space_pool spaces;
static struct display_access_token exclusion;
static struct scanout_transfer_token token;
static uint64_t ram[2048],cpu_maps[4],gtt[4],original[4];
static unsigned parent,lifetime,other;
static struct command_map_io io(void);
static bool use(void *,struct intel_command_mapping *);
static struct {
 const char *mode;unsigned at,events,clocks,ownership,reads,attempts,writes,completions,readers,pauses,children,pending,domain,bit;
 uint64_t time,limit;bool active,hit,allocated,retired;
} m;
static unsigned index_for(uint32_t offset){assert(offset>=0x811000 && offset<0x811020 && !(offset&7));return (offset-0x811000)/8;}
static bool mode(const char *s){return !strcmp(m.mode,s);}
static bool fault(const char *s,unsigned n){bool hit=mode(s) && n==m.at;if(hit)m.hit=true;return hit;}
static void hook(void) {
 if(!m.active)return;
 ++m.events;if(m.events!=m.at)return;
 if(mode("reenter")){
  assert(!intel_command_mapping_run(&mapping,&cpu,&transfer,&token,io(),use,&m));
  assert(!intel_command_memory_close(&cpu) && !intel_command_memory_consumer_begin(&cpu,&other));
  assert(!intel_command_memory_consumer_end(&cpu,&other));m.hit=true;
 }
 if(mode("registration")){cpu.hold.registered=NULL;m.hit=true;}
 if(mode("hold")){cpu.hold.serial++;m.hit=true;}
 if(mode("cpu-source")){cpu.lease.lifetime=&other;m.hit=true;}
 if(mode("cpu-io")){cpu.io.initialize=NULL;m.hit=true;}
 if(mode("map-source")){mapping.lease.root^=4096;m.hit=true;}
 if(mode("map-io")){mapping.io.read=NULL;m.hit=true;}
 if(mode("map-counter")){mapping.proofs++;m.hit=true;}
 if(mode("map-time")){mapping.last--;m.hit=true;}
 if(mode("map-vector")){mapping.replacement[3]^=4096;m.hit=true;}
 if(mode("publication")){mapping.publication.reads++;m.hit=true;}
 if(mode("publication-vector")){mapping.publication.after[3]^=4096;m.hit=true;}
 if(mode("transfer")){transfer.manifest.source_epoch++;m.hit=true;}
 if(mode("transfer-vector")){transfer.original[3]^=4096;m.hit=true;}
 if(mode("range")){transfer.space.leases[mapping.range.slot].serial++;m.hit=true;}
}
static uint64_t now(void *ctx) {
 assert(ctx==&m);hook();if(m.active)++m.clocks;
 if(m.active && fault("clock-zero",m.clocks))return 0;
 if(m.active && fault("clock-back",m.clocks))return mapping.last-1;
 if(m.active && (fault("clock-limit",m.clocks) || fault("clock-over",m.clocks))){m.time=m.limit+mode("clock-over");return m.time;}
 if(m.active && mode("frozen"))return m.time;
 return ++m.time;
}
static uint64_t bad_now(void *ctx){(void)ctx;return 0;}
static uint64_t phase_clock(void *ctx){
 assert(ctx==&m);++m.clocks;
 if(mode("transfer-phase-zero"))return 0;
 if(mode("transfer-phase-back"))return transfer.last_us-1;
 if(mode("transfer-phase-frozen"))return m.time;
 if(m.clocks==3){m.time=transfer.begin_us+100000+mode("transfer-phase-over");return m.time;}
 return ++m.time;
}
static uint64_t allocate(void *ctx,uint32_t pages){assert(ctx==&m && pages==4 && !m.allocated);m.allocated=true;return 0x1000000;}
static bool backing(void *ctx,uint64_t pa,uint32_t pages){return ctx==&m && pa==0x1000000 && pages==4 && m.allocated;}
static bool release(void *ctx,uint64_t pa,uint32_t pages){assert(backing(ctx,pa,pages) && m.retired);for(unsigned i=0;i<4;i++)assert(!cpu_maps[i]);m.allocated=false;return true;}
static bool cpu_map(void *ctx,uint64_t root,uint64_t va,uint64_t pa){assert(ctx==&m && root==0x1000 && va>=0xffffc001f5160000ull && va<0xffffc001f5164000ull);unsigned i=(va-0xffffc001f5160000ull)/4096;assert(!cpu_maps[i] && pa==0x1000000+4096*i);cpu_maps[i]=pa;return true;}
static bool cpu_unmap(void *ctx,uint64_t root,uint64_t va,uint64_t *pa){assert(ctx==&m && root==0x1000);unsigned i=(va-0xffffc001f5160000ull)/4096;assert(i<4 && cpu_maps[i]);*pa=cpu_maps[i];cpu_maps[i]=0;return true;}
static bool cpu_observe(void *ctx,uint64_t root,uint64_t va,uint64_t pa,struct intel_command_alias *out){assert(ctx==&m && root==0x1000 && va==0xffffc001f5160000ull && pa==0x1000000);for(unsigned i=0;i<4;i++)assert(cpu_maps[i]==pa+4096*i);*out=(struct intel_command_alias){4,4,0,1,0,1,1,6,6,6};return true;}
static bool initialize(void *ctx,uint64_t va,uint32_t bytes){assert(ctx==&m && va==0xffffc001f5160000ull && bytes==16384);memset(ram,0,sizeof(ram));return true;}
static bool preserved(void *ctx,uint64_t va,uint32_t bytes){assert(ctx==&m && va==0xffffc001f5160000ull && bytes==16384);for(unsigned i=0;i<2048;i++)assert(!ram[i]);return true;}
static bool cpu_guard(void *ctx,const struct intel_command_lease *l){return ctx==&m && l==&cpu.lease && l->owner==&parent && l->provider==&m && l->lifetime==&lifetime && !m.retired;}
static bool cpu_retired(void *ctx,const struct intel_command_lease *l){return ctx==&m && l==&cpu.lease && m.retired;}
static bool transferred(void *ctx,const struct scanout_transfer_manifest *p,enum scanout_transfer_phase phase){
 assert(ctx==&m);hook();
 if(p->source_epoch!=1 || p->assurances!=HANDOFF_ALL || p->obligations)return false;
 if(phase!=TRANSFER_LIVE)for(unsigned i=0;i<4;i++)if(gtt[i]!=original[i])return false;
 return true;
}
static bool ownership(void *ctx,const struct intel_command_mapping *r,enum command_map_phase phase,struct command_map_proof *p){
 assert(ctx==&m && r==&mapping && phase==mapping.phase);hook();++m.ownership;
 *p=(struct command_map_proof){r,&cpu,&transfer,0x1000,1,0x1000000,m.time,8704,4,COMMAND_MAP_REQUIRED};
 if(fault("proof-source",m.ownership))p->object=&copy;
 if(fault("proof-cpu",m.ownership))p->cpu=NULL;
 if(fault("proof-transfer",m.ownership))p->transfer=NULL;
 if(fault("proof-root",m.ownership))p->root^=4096;
 if(fault("proof-epoch",m.ownership))p->epoch++;
 if(fault("proof-physical",m.ownership))p->physical+=4096;
 if(fault("proof-first",m.ownership))p->first++;
 if(fault("proof-pages",m.ownership))p->pages--;
 if(fault("proof-stale",m.ownership))p->checked_at=m.time-1;
 if(fault("proof-future",m.ownership))p->checked_at=UINT64_MAX;
 if(fault("proof-required",m.ownership))p->requirements^=1u<<m.bit;
 return !fault("proof-false",m.ownership);
}
static bool read_pte(void *ctx,uint32_t offset,uint64_t *out){assert(ctx==&m);unsigned i=index_for(offset);hook();++m.reads;*out=gtt[i];if(fault("read-changed",m.reads))*out^=4096;return !fault("read-false",m.reads);}
static enum intel_gt_write write_pte(void *ctx,uint32_t offset,uint64_t value){
 assert(ctx==&m);unsigned i=index_for(offset);hook();++m.attempts;
 uint64_t expected=mapping.publication.state==PUB_RESTORING?original[i]:(0x1000000ull+4096*i)|1;
 assert(value==expected && mapping.publication.exposed && transfer.space.leases[mapping.range.slot].use==SPACE_EXPOSED);
 if(fault("write-none",m.attempts))return GT_WRITE_NONE;
 if(fault("write-unknown",m.attempts)){gtt[i]=value^1;return GT_WRITE_UNCERTAIN;}
 gtt[i]=value;++m.writes;return GT_WRITE_DONE;
}
static bool complete(void *ctx,const struct intel_command_mapping *r,enum command_map_phase phase){
 assert(ctx==&m && r==&mapping);hook();++m.completions;
 for(unsigned i=0;i<4;i++)assert(gtt[i]==(phase==COMMAND_MAP_ORIGINAL?original[i]:(0x1000000ull+4096*i)|1));
 return !fault("complete-false",m.completions);
}
static bool readers(void *ctx,const struct intel_command_mapping *r){
 assert(ctx==&m && r==&mapping);hook();++m.readers;
 if(fault("reader-returned",m.readers))return false;
 return m.readers>m.pending;
}
static void pause_once(void *ctx){assert(ctx==&m);hook();++m.pauses;}
static struct command_map_io io(void){return (struct command_map_io){&m,ownership,read_pte,write_pte,complete,readers,now,pause_once};}
static bool use(void *ctx,struct intel_command_mapping *r){
 assert(ctx==&m && r==&mapping);hook();++m.children;
 if(m.hit)return true; /* Deliberate callback corruption; core must reject it. */
 assert(!intel_command_memory_returned(&cpu) && !intel_command_memory_close(&cpu));
 assert(!intel_command_memory_consumer_begin(&cpu,&other) && !intel_command_memory_consumer_end(&cpu,&other));
 assert(!intel_command_mapping_run(r,&cpu,&transfer,&token,io(),use,&m));
 copy=*r;assert(!intel_command_mapping_page(&copy,0,&m.time,&m.limit));copy.object=NULL;assert(!intel_command_mapping_run(&copy,&cpu,&transfer,&token,io(),use,&m));
 for(unsigned i=0;i<4;i++){uint64_t ga,pa;assert(intel_command_mapping_page(r,i,&ga,&pa));assert(ga==(8704ull+i)*4096 && pa==0x1000000ull+4096*i);}
 uint64_t ga,pa;assert(!intel_command_mapping_page(r,4,&ga,&pa));assert(!intel_command_mapping_page(r,0,&r->physical,&pa));assert(!intel_command_mapping_page(r,0,&cpu.physical,&pa));assert(!intel_command_mapping_page(r,0,&transfer.original[0],&pa));assert(!intel_command_mapping_page(r,0,&ga,&ga));
 if(mode("parent-close"))assert(!scanout_transfer_close(&transfer,&token));
 return !fault("child-false",m.children);
}
static void setup(void){
 m.time=1000;
 for(unsigned i=0;i<4;i++)gtt[i]=original[i]=0x128c000001ull+4096*i;
 assert(intel_command_memory_prepare(&cpu,0x1000,0xffffc001f5160000ull,(struct intel_command_io){{&m,allocate,backing,release},cpu_map,cpu_unmap,cpu_observe,initialize,preserved,cpu_guard,cpu_retired,now}));
 m.time=3000000;
 uint64_t gt=m.time,hda=m.time,outer=m.time;
 if(m.domain==1){gt-=250000;outer=gt;}
 if(m.domain==2){gt-=100000;hda-=350000;outer=hda;}
 if(m.domain==3){gt-=150000;hda-=250000;outer-=450000;}
 struct intel_command_lease lease={&parent,&m,&lifetime,0x1000,1,1,m.time,gt,hda,outer};
 assert(intel_command_memory_run(&cpu,&lease) && intel_command_memory_returned(&cpu));
 const uint64_t origins[]={lease.prepare,lease.gt,lease.hda,lease.outer},caps[]={INTEL_DIAGNOSTIC_PREPARE_US,300000,400000,500000};m.limit=origins[m.domain]+caps[m.domain];
 assert(display_access_acquire(9,&exclusion));
 struct scanout_transfer_manifest manifest={.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,.fence_end=0x2200000,.source_epoch=1,.first=8704,.pages=4,.assurances=HANDOFF_ALL,.original=original};
 assert(scanout_transfer_open(&transfer,&transfers,&spaces,&exclusion,&manifest,(struct scanout_transfer_io){&m,transferred,now},&token));
}
static int run_case(int argc,char **argv){
 assert(argc==2);const char *arg=argv[1];if(!strncmp(arg,"--wake-engine-",14))arg+=14;
 char text[96];assert(strlen(arg)<sizeof(text));strcpy(text,arg);char *colon=strchr(text,':');m.at=1;
 if(colon){*colon++=0;m.at=(unsigned)strtoul(colon,&colon,10);if(*colon==':'){unsigned extra=(unsigned)strtoul(colon+1,NULL,10);if(!strcmp(text,"proof-required"))m.bit=extra;else m.domain=extra;}}
 m.mode=text;assert(m.domain<4);setup();
 if(!strncmp(text,"transfer-phase-",15)){
  assert(!scanout_transfer_check_clocked(&transfer,&token,NULL,&m));
  assert(!scanout_transfer_check_clocked(&transfer,&token,phase_clock,NULL));
  assert(!transfer.busy && transfer.state==TRANSFER_ACTIVE);
  bool ok=scanout_transfer_check_clocked(&transfer,&token,phase_clock,&m);
  assert(ok==mode("transfer-phase-limit"));
  if(ok){assert(transfer.last_us-transfer.begin_us==100000);assert(scanout_transfer_close(&transfer,&token) && display_access_release(&exclusion,true));}
  else assert(transfer.error==TRANSFER_CLOCK && transfer.state==TRANSFER_FAULT && transfer.pinned && !display_access_release(&exclusion,true));
  assert(transfer.io.now==now && transfer.io.ctx==&m && transfer.io.revalidate==transferred);
  puts("command-mapping-transfer-clock: PASS original100ms phase; no replaced I/O table");return 0;
 }
 if(mode("pending"))m.pending=m.at;
 if(mode("stuck"))m.pending=UINT32_MAX;
 struct command_map_io provider=io();
 if(mode("invalid")){
  switch(m.at){case 1:cpu.lease.root++;break;case 2:transfer.manifest.assurances=0;break;case 3:transfer.manifest.obligations=255;break;case 4:transfer.manifest.pages=3;break;case 5:provider.ctx=&other;break;case 6:provider.now=bad_now;break;case 7:provider.read=NULL;break;case 8:provider.write=NULL;break;case 9:provider.ownership=NULL;break;case 10:provider.complete=NULL;break;case 11:provider.readers_retired=NULL;break;case 12:provider.pause=NULL;break;case 13:token.epoch++;break;case 14:mapping.state=COMMAND_MAP_CLOSED;break;default:assert(0);}
  assert(!intel_command_mapping_run(&mapping,&cpu,&transfer,&token,provider,use,&m) && !mapping.object && !m.events && !m.writes);puts("command-mapping: PASS invalid no callbacks");return 0;
 }
 m.active=true;bool ok=intel_command_mapping_run(&mapping,&cpu,&transfer,&token,provider,use,&m);
 bool normal=mode("report") || mode("reenter") || mode("parent-close") || (mode("pending") && m.pending<COMMAND_MAP_POLLS) || mode("clock-limit");
 if(normal){
  assert(ok && mapping.complete && !mapping.retained && !mapping.busy && !mapping.registered && mapping.state==COMMAND_MAP_CLOSED);
  assert(mapping.attempts==8 && mapping.writes==8 && m.attempts==8 && m.writes==8 && m.completions==2 && mapping.completions==2 && m.children==1);
  assert(mapping.publication.state==PUB_RETIRED && mapping.publication.reads==20 && mapping.publication.flushes==2);
  assert(m.readers==mapping.polls+1 && mapping.reader_checks==m.readers);
  for(unsigned i=0;i<4;i++)assert(gtt[i]==original[i]);
  assert(intel_command_memory_returned(&cpu) && !display_access_release(&exclusion,true));
  assert(transfer.io.now==now && transfer.io.ctx==&m && transfer.io.revalidate==transferred);
  m.active=false;assert(scanout_transfer_close(&transfer,&token) && display_access_release(&exclusion,true));
  m.retired=true;assert(intel_command_memory_close(&cpu) && !m.allocated);
 }else{
  assert(!ok && !mapping.complete && mapping.retained && mapping.registered && mapping.state==COMMAND_MAP_FAULT && !mapping.busy);
  assert(m.allocated && !intel_command_memory_close(&cpu) && !display_access_release(&exclusion,true));
  assert(mapping.cpu==&cpu);assert(m.hit || mode("stuck") || mode("pending") || mode("frozen"));
  if(mode("reader-returned")){
   assert(m.at==2 && m.readers==2 && mapping.writes==8 && mapping.completions==2);
   for(unsigned i=0;i<4;i++)assert(gtt[i]==original[i]);
   assert(mapping.publication.state==PUB_FAULT && mapping.range.object==&transfer.space);
  }
 }
 printf("command-mapping: PASS complete=%u events=%u clocks=%u proofs=%u reads=%u attempts=%u writes=%u completions=%u readers=%u polls=%u held=%u prepare_us=%llu; synthetic hardware only\n",ok,m.events,m.clocks,m.ownership,m.reads,m.attempts,m.writes,m.completions,m.readers,mapping.polls,m.allocated,(unsigned long long)INTEL_DIAGNOSTIC_PREPARE_US);
 return 0;
}
int main(int argc,char **argv){
#ifndef _WIN32
 if(argc==2 && !strcmp(argv[1],"--fixture-server"))return fixture_server(run_case);
#endif
 return run_case(argc,argv);
}
