/* Actual CPU owner + transfer/space/publication cores. Hardware ownership is
 * synthetic in this host fixture; this is never native acceptance. */
#define _POSIX_C_SOURCE 200809L
#include <srvros/intel_command_mapping.h>
#include <srvros/vmm.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixture_server.h"
static struct intel_command_memory cpu;
static struct intel_command_mapping mapping,copy;
static struct scanout_transfer transfer;
static struct scanout_interval target;
enum { target_slot = 8704, command_first = target_slot + 1 };
static struct scanout_transfer_pool transfers;
static struct scanout_space_pool spaces;
static struct display_access_token exclusion;
static struct scanout_transfer_token token;
static uint64_t ram[2048],cpu_maps[4],original[4];
static _Alignas(4096) uint64_t table[512];
#define gtt (table+1)
static unsigned parent,lifetime,other;
static struct command_map_io io(void);
static bool use(void *,struct intel_command_mapping *);
static struct {
 const char *mode;unsigned at,events,clocks,ownership,reads,attempts,writes,completions,readers,pauses,children,pending,domain,bit;
 uint64_t time,limit;bool active,hit,allocated,retired;
} m;
static unsigned index_for(uint32_t offset){const uint32_t base=0x800000+8*command_first;
 assert(offset>=base && offset<base+32 && !(offset&7));return (offset-base)/8;}
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
 *p=(struct command_map_proof){r,&cpu,&transfer,0x1000,1,0x1000000,m.time,command_first,4,COMMAND_MAP_REQUIRED};
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
 for(unsigned i=0;i<4;i++){uint64_t ga,pa;assert(intel_command_mapping_page(r,i,&ga,&pa));assert(ga==(uint64_t)(command_first+i)*4096 && pa==0x1000000ull+4096*i);}
 uint64_t ga,pa;assert(!intel_command_mapping_page(r,4,&ga,&pa));assert(!intel_command_mapping_page(r,0,&r->physical,&pa));assert(!intel_command_mapping_page(r,0,&cpu.physical,&pa));assert(!intel_command_mapping_page(r,0,&transfer.original[0],&pa));assert(!intel_command_mapping_page(r,0,&ga,&ga));
 if(mode("parent-close"))assert(!scanout_transfer_close(&transfer,&token));
 return !fault("child-false",m.children);
}
static struct vmm_reserved_guard window_guard={&window_guard,17};
static bool active_block, joint, slot_allocated;
static const uint64_t slot_original=0x8e201001;
static unsigned requests,releases,invalidations;
static uint32_t selector=0x40000000;
bool vmm_reserved_guard_register_pinned(const struct vmm_reserved_guard *g,uint64_t root,uint64_t aperture,
                                        uint64_t reg,uint64_t alias,const void *pin)
{
 return g==&window_guard && root==0x1000 && aperture==0xb2200000 && reg==0xc0811000 &&
        alias==(uintptr_t)table && pin==&parent;
}
bool vmm_reserved_guard_io_range_pinned(const struct vmm_reserved_guard *g,uint64_t root,uint64_t first,
                                        unsigned pages,const void *child)
{
 if(g!=&window_guard || root!=0x1000 || child!=&parent || fault("window-aperture",1))return false;
 if(joint && pages==1){
  const uint64_t port[]={0xc0002000,0xc0012000,0xc001c000,0xc001a000,0xc0022000,0xc0014000,0xc0065000,0xc006e000};
  const char *missing[]={"joint-rcs-port","joint-vcs0-port","joint-vcs1-port","joint-vecs-port","joint-bcs-port","joint-ring-port","joint-audio-port","joint-capture-port"};
  for(unsigned i=0;i<8;i++)if(first==port[i])return !fault(missing[i],invalidations);
 }
 return (first==0xb2201000 && pages==4) || (first==0xb2200000 && pages==1);
}
bool intel_cache_probe_uc_page(uint64_t physical)
{
 return physical==0xc0811000 && !fault("window-cache",1);
}
static uint64_t held_epoch(const struct scanout_transfer *t,const struct scanout_publication *p)
{
 assert(t==&transfer && p==&mapping.publication);
 if(active_block && mapping.attempts && fault("window-epoch",mapping.attempts))return 22;
 return active_block ? 21 : 0;
}
static enum intel_gt_write request(const struct scanout_transfer *t,const struct scanout_publication *p,uint64_t *epoch)
{
 assert(!held_epoch(t,p) && p->state==PUB_PREPARED && !requests);
 requests++; active_block=true; *epoch=21;
 return GT_WRITE_DONE;
}
static enum intel_gt_write unblock(const struct scanout_transfer *t,const struct scanout_publication *p,uint64_t epoch)
{
 assert(held_epoch(t,p)==epoch && p->state==PUB_RETIRED && invalidations==(joint?4u:2u) && !releases);
 for(unsigned i=0;i<4;i++)assert(gtt[i]==original[i]);
 if(joint)assert(table[0]==slot_original && !slot_allocated);
 active_block=false;releases++;return GT_WRITE_DONE;
}
static bool traffic_read(const struct scanout_transfer *t,uint32_t reg,uint32_t *v)
{
 assert(t==&transfer);
 switch(reg){
 case 0x40d0: case 0x8028: *v=active_block ? 0 : 0x5f;break;
 case 0x8038: *v=active_block ? !fault("window-no-l3",1) : fault("window-stale-l3",1);break;
 case 0x8000: case 0x8004: case 0x8008: case 0x800c: case 0x8010:
  *v=active_block && !(reg==0x8004 && fault("window-no-engine",1)) ? 0x20 : 0x10;break;
 case 0x8014:
  *v=!active_block || (mapping.attempts==m.at && fault("window-widi",mapping.attempts)) ||
     (invalidations && (fault("window-post-widi",invalidations) || fault("joint-widi",invalidations))) ? 0x10 : 0;
  break;
 case 0x8040: *v=1;break;
 case 0x70180: *v=1u<<31;break;
 case 0x7019c: case 0x701ac:
  *v=0x100000;
  if(mapping.attempts && fault("window-plane",mapping.attempts))*v+=4096;
  if(mapping.attempts && fault("window-foreign",mapping.attempts))gtt[(m.at-1)%4]=0xbadbe001;
  break;
 case 0x9120: *v=3u<<25;break;
 case 0xfdc: *v=selector;break;
 case 0x138064: *v=3;break;
 case 0xb11c: *v=fault("window-narrow-bank",1) && (selector>>24&15)==7 ? 16 : 0;break;
 case 0x101008: *v=fault("window-pending",mapping.attempts) ? 1 : 0;break;
 case 0x12870: case 0x12878: case 0x1c870: case 0x1c878: *v=0;break;
 default: return false;
 }
 return true;
}
static enum intel_gt_write steer(const struct scanout_transfer *t,uint32_t from,uint32_t to)
{
 assert(t==&transfer && from==selector && !((from^to)&~(15u<<24)));
 selector=to;return GT_WRITE_DONE;
}
static enum intel_gt_write invalidate(const struct scanout_transfer *t,const struct scanout_publication *p,
                                      uint64_t epoch,uint32_t offset,uint32_t value)
{
 assert(held_epoch(t,p)==epoch && offset==0x101008 && value==1 && invalidations<(joint?4u:2u));
 if(joint){
  assert(active_block && !releases);
  assert(table[0]==(invalidations==1 ? 0x2000001u : slot_original));
  for(unsigned i=0;i<4;i++)assert(gtt[i]==(invalidations==3?original[i]:((cpu.physical+4096*i)|1)));
 }
 invalidations++;
 return GT_WRITE_DONE;
}
static bool window_child(void *ctx,struct intel_command_mapping *r)
{
 assert(active_block && requests==1 && !releases && invalidations==1);
 for(unsigned i=0;i<4;i++)assert(gtt[i]==((cpu.physical+4096*i)|1));
 return use(ctx,r);
}
static uint64_t slot_allocate(void *ctx,uint32_t pages)
{
 assert(ctx==&m && pages==1 && !slot_allocated);
 slot_allocated=true;return 0x2000000;
}
static bool slot_validate(void *ctx,uint64_t pa,uint32_t pages)
{
 return ctx==&m && pages==1 && pa==0x2000000 && slot_allocated;
}
static bool slot_release(void *ctx,uint64_t pa,uint32_t pages)
{
 assert(slot_validate(ctx,pa,pages) && active_block && invalidations==3 && table[0]==slot_original);
 slot_allocated=false;return true;
}
static bool slot_owned(void *ctx,const struct scanout_transfer_manifest *p,enum scanout_transfer_phase phase)
{
 return ctx==&m && p->first==target_slot && p->pages==1 && p->source_epoch==1 &&
        p->assurances==HANDOFF_ALL && !p->obligations && (phase==TRANSFER_LIVE || table[0]==slot_original);
}
static bool slot_held(void *ctx){return ctx==&m && active_block && requests==1 && !releases;}

static void test_joint(struct command_map_io provider,struct command_map_window *window)
{
 static struct scanout_transfer second;
 static struct scanout_publish_pool publications;
 static struct scanout_memory memory;
 struct scanout_memory_pool pool={0};
 struct scanout_memory_layout layout;
 struct command_map_slot slot={.transfer=&second,.pool=&publications,.memory=&memory,
     .io={.ctx=&m,.guard=slot_held,.flush=slot_held,.readers_retired=slot_held,.now=now}};
 struct scanout_transfer_manifest manifest=transfer.manifest;
 manifest.first=target_slot;manifest.pages=1;manifest.original=&slot_original;
 joint=true;table[0]=slot_original;table[5]=0xbad5001;
 scanout_interval_release(&target);
 assert(scanout_interval_protect_boot_framebuffer(manifest.mmio,manifest.aperture,manifest.aperture_bytes,manifest.fence_end));
 assert(scanout_transfer_open(&second,&transfers,&spaces,&exclusion,&manifest,(struct scanout_transfer_io){&m,slot_owned,now},&slot.grant));
 assert(scanout_memory_layout(16,1,64,false,&layout) && scanout_memory_create(&memory,&pool,second.owner,&layout,
      0x100000000ull,(struct scanout_memory_io){&m,slot_allocate,slot_validate,slot_release},&slot.token));
 bool ok=intel_command_mapping_run_slot(&mapping,&cpu,&transfer,&token,provider,window,&slot);
 assert(table[5]==0xbad5001 && selector==0x40000000 && requests==(mode("joint-good") || m.at!=0));
 if(mode("joint-good")){
  assert(ok && mapping.complete && memory.state==MEM_RELEASED && !slot_allocated && !active_block && releases==1 && invalidations==4);
  assert(scanout_transfer_close(&second,&slot.grant) && scanout_transfer_close(&transfer,&token) && display_access_release(&exclusion,true));
  m.retired=true;assert(intel_command_memory_close(&cpu));
 }else{
  assert(!ok && m.hit && mapping.retained && slot_allocated && active_block==(requests!=0) && !releases && invalidations==m.at);
  assert(table[0]==(m.at==2?0x2000001:slot_original) && !intel_command_memory_close(&cpu));
 }
 printf("command-mapping: PASS joint complete=%u invalidations=%u releases=%u\n",ok,invalidations,releases);
}
static void test_window(struct command_map_io provider)
{
 struct command_map_window window={&window_guard,&parent,17,(uintptr_t)table,0xb2200000,
   {.read=traffic_read,.held_epoch=held_epoch,.request=request,.release=unblock,
    .select_l3=steer,.invalidate_ggtt=invalidate}};
 provider.read=NULL;provider.write=NULL;m.active=true;
 if(!strncmp(m.mode,"joint-",6)){test_joint(provider,&window);return;}
 table[0]=0xbad1001;table[5]=0xbad5001;
 bool ok=intel_command_mapping_run_window(&mapping,&cpu,&transfer,&token,provider,&window,window_child,&m);
 assert(table[0]==0xbad1001 && table[5]==0xbad5001 && selector==0x40000000);
 if(mode("window-good")){
  assert(ok && mapping.complete && mapping.writes==8 && !active_block && releases==1 && m.children==1);
  m.active=false;assert(scanout_transfer_close(&transfer,&token) && display_access_release(&exclusion,true));
  m.retired=true;assert(intel_command_memory_close(&cpu));
 }else{
  assert(!ok && m.hit && mapping.retained && !releases && active_block==(requests!=0) &&
         !intel_command_memory_close(&cpu) && !scanout_transfer_close(&transfer,&token));
  if(mode("window-foreign"))assert(gtt[(m.at-1)%4]==0xbadbe001);
  if(mode("window-pending") || mode("window-widi") || mode("window-foreign") || mode("window-plane") || mode("window-epoch"))assert(mapping.writes==m.at-1);
  if(mode("window-post-widi"))assert(mapping.writes==4*m.at);
  if(mode("window-aperture") || mode("window-cache") || mode("window-narrow-bank"))assert(!requests);
 }
 printf("command-mapping: PASS native-access=1 complete=%u stores=%u requests=%u releases=%u\n",ok,mapping.writes,requests,releases);
}

static void setup(void){
 m.time=1000;
 for(unsigned i=0;i<4;i++)gtt[i]=original[i]=0x128c000001ull+4096*i;
 assert(intel_command_memory_prepare(&cpu,0x1000,0xffffc001f5160000ull,(struct intel_command_io){{&m,allocate,backing,release},cpu_map,cpu_unmap,cpu_observe,initialize,preserved,cpu_guard,cpu_retired,now}));
 m.time=3000000;
 uint64_t gt=m.time,hda=m.time,outer=m.time;
#ifdef SRVROS_INTEL_GT_PRACTICAL
 if(m.domain==1){gt-=300000;hda=outer=gt;}
 if(m.domain==2){gt-=275000;hda-=550000;outer=hda;}
 if(m.domain==3){gt-=262500;hda-=525000;outer-=800000;}
#else
 if(m.domain==1){gt-=250000;outer=gt;}
 if(m.domain==2){gt-=100000;hda-=350000;outer=hda;}
 if(m.domain==3){gt-=150000;hda-=250000;outer-=450000;}
#endif
 if(mode("late")){gt=m.time-160000;hda=m.time-180000;outer=m.time-232000;}
 struct intel_command_lease lease={&parent,&m,&lifetime,0x1000,1,1,m.time,gt,hda,outer};
 assert(intel_command_memory_run(&cpu,&lease) && intel_command_memory_returned(&cpu));
 if(mode("late"))m.time=lease.prepare+220000;
 const uint64_t origins[]={lease.prepare,lease.gt,lease.hda,lease.outer},caps[]={INTEL_DIAGNOSTIC_PREPARE_US,INTEL_DIAGNOSTIC_GT_TOTAL_US,INTEL_DIAGNOSTIC_HDA_US,INTEL_DIAGNOSTIC_OUTER_US};m.limit=origins[m.domain]+caps[m.domain];
 assert(display_access_acquire(9,&exclusion));
 struct scanout_transfer_manifest manifest={.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,.fence_end=0x2200000,.source_epoch=1,.first=command_first,.pages=4,.assurances=HANDOFF_ALL,.original=original};
 assert(scanout_interval_claim(&target,manifest.mmio,manifest.aperture,manifest.aperture_bytes,manifest.fence_end,
  (struct scanout_space_range){target_slot,1}));
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
 if(!strncmp(text,"window-",7) || !strncmp(text,"joint-",6)){test_window(provider);return 0;}
 if(mode("invalid")){
  switch(m.at){case 1:cpu.lease.root++;break;case 2:transfer.manifest.assurances=0;break;case 3:transfer.manifest.obligations=255;break;case 4:transfer.manifest.pages=3;break;case 5:provider.ctx=&other;break;case 6:provider.now=bad_now;break;case 7:provider.read=NULL;break;case 8:provider.write=NULL;break;case 9:provider.ownership=NULL;break;case 10:provider.complete=NULL;break;case 11:provider.readers_retired=NULL;break;case 12:provider.pause=NULL;break;case 13:token.epoch++;break;case 14:mapping.state=COMMAND_MAP_CLOSED;break;default:assert(0);}
  assert(!intel_command_mapping_run(&mapping,&cpu,&transfer,&token,provider,use,&m) && !mapping.object && !m.events && !m.writes);puts("command-mapping: PASS invalid no callbacks");return 0;
 }
 m.active=true;bool ok=intel_command_mapping_run(&mapping,&cpu,&transfer,&token,provider,use,&m);
 bool normal=mode("report") || mode("reenter") || mode("parent-close") || (mode("pending") && m.pending<COMMAND_MAP_POLLS) || mode("clock-limit") || (mode("late") && INTEL_DIAGNOSTIC_PREPARE_US==250000);
 assert(scanout_interval_held(&target,0xc0000000,0xb0000000,0x10000000,0x2200000,
  (struct scanout_space_range){target_slot,1}));
 if(normal){
  assert(ok && mapping.complete && !mapping.retained && !mapping.busy && !mapping.registered && mapping.state==COMMAND_MAP_CLOSED);
  assert(mapping.attempts==8 && mapping.writes==8 && m.attempts==8 && m.writes==8 && m.completions==2 && mapping.completions==2 && m.children==1);
  assert(mapping.publication.state==PUB_RETIRED && mapping.publication.reads==20 && mapping.publication.flushes==2);
  assert(m.readers==mapping.polls+1 && mapping.reader_checks==m.readers);
  for(unsigned i=0;i<4;i++)assert(gtt[i]==original[i]);
  assert(intel_command_memory_returned(&cpu) && !display_access_release(&exclusion,true));
  assert(transfer.io.now==now && transfer.io.ctx==&m && transfer.io.revalidate==transferred);
  m.active=false;assert(scanout_transfer_close(&transfer,&token) && display_access_release(&exclusion,true));
  m.retired=true;assert(intel_command_memory_close(&cpu) && !m.allocated);scanout_interval_release(&target);
 }else{
  assert(!ok && !mapping.complete && mapping.retained && mapping.registered && mapping.state==COMMAND_MAP_FAULT && !mapping.busy);
  assert(m.allocated && !intel_command_memory_close(&cpu) && !display_access_release(&exclusion,true));
  assert(mapping.cpu==&cpu);assert(m.hit || mode("stuck") || mode("pending") || mode("frozen") || mode("late"));
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
