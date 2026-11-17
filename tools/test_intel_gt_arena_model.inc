static uint64_t command_ram[2048],command_maps[4];
static bool command_allocated,cm_hit;
static unsigned cm_clocks,cm_backing,cm_alias,cm_cache,cm_allocs,cm_frees,cm_map,cm_unmap,cm_guard,cm_retire;
static unsigned cm_effects[7];
static uint64_t cm_held_mask,cm_hold_mask,cm_close_mask;
static bool cf(const char *name,unsigned n){bool yes=bf(name,n);if(yes && !cm_hit){
 cm_hit=true;cm_effects[0]=cm_frees;cm_effects[1]=cm_map;cm_effects[2]=cm_unmap;
 cm_effects[3]=bcommands;cm_effects[4]=bwrites;cm_effects[5]=memory_writes;cm_effects[6]=cm_alias;
 }return yes;}
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
 assert(wake_irq);
 ++cm_cache;
 *out=(struct x86_cache_snapshot){.cpu={{0x756e6547,0x49656e69,0x6c65746e},0x306d4,0x91000,39,0x80050033},
 .pat=0x0007040600070406ull,.cap=0x501,.def_type=0x800,.base={6},.mask={0x7fc0000800ull},
 .status=CACHE_OBSERVED,.reads=19,.count=1,.fixed_count=11,.begin_us=ticks,.end_us=ticks+1};ticks++;
 for(unsigned i=0;i<11;i++)out->fixed[i]=0x0606060606060606ull;
 if(cf("command-cache-parent-epoch",cm_cache) && gt_bootstrap_native.g)gt_bootstrap_native.g->engine_epoch++;
 if(cf("command-cache-drift",cm_cache))out->pat^=1;
 return !cf("command-cache",cm_cache);
}
static bool intel_display_test_command_alias(uint64_t root,uint64_t va,uint64_t pa,struct vmm_alias_observation *out) {
 assert(wake_irq && root==0x1000 && va==COMMAND_NATIVE_VA && pa==0x3000000);++cm_alias;
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
static uint64_t intel_display_test_command_dma(void){return 61+(command_allocated?4:0);}
static bool intel_display_test_command_retire(void){
 ++cm_retire;
 if(cf("command-retire-original",cm_retire))gt_bootstrap_native.arena_original=NULL;
 return !cf("command-retire",cm_retire);
}
static void intel_display_test_command_boundary(void) {
 ++cm_clocks;struct intel_command_memory *o=&command_native.owner;
 assert(cm_clocks<64);
 if(o->state==COMMAND_HELD)cm_held_mask|=1ull<<cm_clocks;
 if(o->hold.active)cm_hold_mask|=1ull<<cm_clocks;
 if(o->state==COMMAND_RETURNED && command_native.outer_safe)cm_close_mask|=1ull<<cm_clocks;
 if(cf("command-root",cm_clocks))o->root+=4096;
 if(cf("command-serial",cm_clocks))o->serial++;
 if(cf("command-io",cm_clocks))o->io.observe=NULL;
 if(cf("command-hold",cm_clocks))o->hold.lifetime=NULL;
 if(cf("command-hold-object",cm_clocks))o->hold.object=NULL;
 if(cf("command-hold-physical",cm_clocks))o->hold.physical+=4096;
 if(cf("command-hold-serial",cm_clocks))o->hold.serial++;
 if(cf("command-hold-active",cm_clocks))o->hold.active=false;
 if(cf("command-registration",cm_clocks) && o->state==COMMAND_HELD)o->hold.registered=NULL;
 if(cf("command-epoch",cm_clocks) && o->state==COMMAND_HELD)o->lease.epoch++;
 if(cf("command-time",cm_clocks) && o->state==COMMAND_HELD)ticks=o->lease.prepare+INTEL_DIAGNOSTIC_PREPARE_US+1;
}
static uint64_t intel_display_test_command_clock(void) {
 intel_display_test_command_boundary();
 if(command_native.owner.state==COMMAND_HELD && cf("command-parent-back",cm_clocks))return gt_bootstrap_native.last-1;
 if(command_native.owner.state==COMMAND_RETURNED && command_native.outer_safe && cf("command-close-parent-back",cm_clocks)){
  uint64_t at=command_native.outer_end;
  if(joint.scope.end_us>at)at=joint.scope.end_us;
  if(joint.scope.hda.end_us>at)at=joint.scope.hda.end_us;
  if(gt_bootstrap_native.g->last>at)at=gt_bootstrap_native.g->last;
  return at-1;
 }
 return ++ticks;
}
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
 if(cf("command-final",9))gt_bootstrap_native.arena_child=o;
 if(cf("command-final",10))gt_bootstrap_native.arena_borrows++;
 if(cf("command-final",11))gt_bootstrap_native.arena_returns=0;
 if(cf("command-final",12))command_native.parent=NULL;
 if(cf("command-final",13))o->io.observe=NULL;
}

static int arena_fixture_verify(int code) {
 const struct intel_command_memory *o=&command_native.owner;
 if(intel_display_test_bootstrap_expected()) {
  assert(gt_bootstrap_native.complete && gt_bootstrap_native.returned && !gt_bootstrap_native.failed);
  assert(bootstrap_memory_returned() && memory_writes==2 && memory_maps==2 && memory_unmaps==2);
  assert(o->state==COMMAND_CLOSED && o->complete && !o->retained && !command_allocated);
  assert(cm_allocs==1 && cm_frees==1 && cm_map==4 && cm_unmap==4 && cm_guard==2 && cm_retire==2);
  assert(command_outer_retired() && command_native.allocated==4 && command_native.released==4);
  assert(display_access_allowed() && !joint_terminal() && gpu_command==7);
 } else {
  assert(hit && (intel_gt_wake_retained() || (!command_allocated && !o->object && !gt_bootstrap_native.object)));
  if(cm_hit){
   assert(cm_frees==cm_effects[0] && cm_map==cm_effects[1] && cm_unmap==cm_effects[2]);
   assert(bcommands==cm_effects[3] && bwrites==cm_effects[4] && memory_writes==cm_effects[5]);
   if(!strcmp(bootstrap_mode,"command-cache-parent-epoch"))assert(cm_alias==cm_effects[6]);
  } else {
   assert(command_allocated && !cm_frees && !command_native.released);
   assert(bcommands==fault_commands && bwrites==fault_writes);
   assert(memory_writes==memory_fault_writes && memory_reads==memory_fault_reads && memory_unmaps==memory_fault_unmaps);
  }
  if(cm_hit && o->state!=COMMAND_PREPARED && gt_bootstrap_native.object && !gt_bootstrap_native.complete)
   assert(gt_bootstrap_native.g->pinned && gt_bootstrap_native.failed);
 }
 printf("native-arena-fixture: clocks=%u backing=%u cache=%u alias=%u alloc=%u free=%u map=%u unmap=%u guard=%u retire=%u complete=%u hit=%u held=%llu hold=%llu close=%llu\n",
 cm_clocks,cm_backing,cm_cache,cm_alias,cm_allocs,cm_frees,cm_map,cm_unmap,cm_guard,cm_retire,o->state==COMMAND_CLOSED,cm_hit,
 (unsigned long long)cm_held_mask,(unsigned long long)cm_hold_mask,(unsigned long long)cm_close_mask);
 return code;
}
/* Actual command core with an explicitly synthetic parent, to distinguish
 * each selected aggregate boundary from the native parent's independent guard. */
static uint64_t arena_core_time,arena_core_limit;
static unsigned arena_core_clocks;
static bool arena_core_over,arena_core_frozen;
static struct intel_command_lease arena_core_lease;
static unsigned arena_core_owner,arena_core_lifetime;
static uint64_t arena_core_clock(void *ctx) {
 assert(ctx==&command_native);++arena_core_clocks;
 if(!arena_core_frozen){
  if(arena_core_clocks==10)arena_core_time=arena_core_limit+arena_core_over;
  else ++arena_core_time;
 }
 return arena_core_time;
}
static bool arena_core_guard(void *ctx,const struct intel_command_lease *l) {
 return ctx==&command_native && l==&command_native.owner.lease &&
  l->owner==&arena_core_owner && l->provider==&command_native && l->lifetime==&arena_core_lifetime &&
  l->root==0x1000 && l->epoch==7 && l->sequence==1 &&
  l->prepare==arena_core_lease.prepare && l->gt==arena_core_lease.gt &&
  l->hda==arena_core_lease.hda && l->outer==arena_core_lease.outer;
}
static bool arena_core_alias(void *ctx,uint64_t root,uint64_t va,uint64_t pa,struct intel_command_alias *out) {
 assert(ctx==&command_native && root==0x1000 && va==COMMAND_NATIVE_VA && pa==0x3000000);
 *out=(struct intel_command_alias){4,4,0,1,0,1,1,6,6,6};return true;
}
static int arena_core_case(const char *mode) {
 reset();assert(command_prepare());
 arena_core_lease=(struct intel_command_lease){&arena_core_owner,&command_native,&arena_core_lifetime,
  0x1000,7,1,1000000,1000000,1000000,1000000};
 unsigned domain=3;arena_core_limit=1250000;
 if(strstr(mode,"core-gt-")){
  domain=4;arena_core_lease.prepare=1400000;arena_core_lease.hda=1200000;arena_core_limit=1500000;
 } else if(strstr(mode,"core-hda-")){
  domain=5;arena_core_lease.prepare=1600000;arena_core_lease.gt=1400000;arena_core_limit=1750000;
 } else if(strstr(mode,"core-outer-")){
  domain=6;arena_core_lease.prepare=1850000;arena_core_lease.gt=1700000;arena_core_lease.hda=1500000;arena_core_limit=2000000;
 }
 arena_core_time=arena_core_lease.prepare;arena_core_over=strstr(mode,"-over")!=NULL;
 arena_core_frozen=strstr(mode,"-frozen")!=NULL;
 struct intel_command_memory *o=&command_native.owner;
 o->io.guard=arena_core_guard;o->io.observe=arena_core_alias;o->io.now=arena_core_clock;o->original_io=o->io;
 bool ok=intel_command_memory_run(o,&arena_core_lease);
 if(arena_core_over || arena_core_frozen){
  assert(!ok && o->state==COMMAND_FAULT && o->retained && !cm_frees);
  assert(o->clock_reason==(arena_core_frozen?9:domain));
 } else assert(ok && o->end==arena_core_limit && intel_command_memory_returned(o));
 assert(!gt_bootstrap_native.object && !bcommands && !memory_writes && command_allocated && !cm_frees);
 printf("arena-core-boundary: PASS policy=%llu domain=%u refused=%u clocks=%u; synthetic parent only\n",
  (unsigned long long)INTEL_DIAGNOSTIC_PREPARE_US,domain,!ok,arena_core_clocks);return 0;
}
