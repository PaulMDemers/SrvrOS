/* Native provider admission dry run. Real observations, no positive GPU
 * transfer: pipeline/prefetch/firmware/reader completion is still unproved. */
static struct {
 const void *object;
 uint64_t begin,last,end,clock,previous,root,physical,serial;
 unsigned state,reason,samples,reads,groups,verified,missing,guard_reads,bad_command;
 bool registered,complete;
 struct {
  uint64_t begin,end,pte[4],cache_begin,cache_end,cache_reads;
  uint32_t pci[9],engine[5][6],plane[3][13],alias[7],cache[3],backing;
 } sample[2];
} command_ownership;
static bool command_ownership_fail(unsigned why) {
 command_ownership.reason|=why;command_ownership.state=3;
 nonzero_native.failed=true;nonzero_native.a->uncertain=true;return false;
}
static bool command_ownership_live(void) {
 const struct intel_command_memory *c=&command_native.owner;
 return command_ownership.object==&command_ownership && command_ownership.state==1 &&
 command_ownership.registered && !command_ownership.reason && !command_ownership.complete &&
 !command_ownership.end && command_ownership.samples<=2 && command_ownership.reads<=180 &&
 command_ownership.groups<=22 && command_ownership.guard_reads<=162 && !command_ownership.bad_command && command_ownership.root==c->root &&
 command_ownership.physical==c->physical && command_ownership.serial==c->serial &&
 intel_command_memory_consumer_live(c,&command_ownership) && command_lease_identity(&c->lease) &&
 source_engines_native.issued==5 && source_requester_native.issued==1 &&
 ggtt_native.complete && ggtt_completed_record() && vtd_completed_record();
}
static bool command_ownership_tick(void) {
 unsigned char saved[offsetof(__typeof__(command_ownership),sample)];
 const unsigned char *bytes=(const unsigned char *)&command_ownership;
 for(unsigned i=0;i<sizeof(saved);i++)saved[i]=bytes[i];
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_ownership_boundary();
#endif
 if(!command_ownership_live())return command_ownership_fail(1);
 uint64_t previous=command_ownership.last,at=source_engines_now(&nonzero_native);
 for(unsigned i=0;i<sizeof(saved);i++)if(saved[i]!=bytes[i])return command_ownership_fail(1);
 if(!command_ownership_live())return command_ownership_fail(1);
 const struct intel_command_lease *l=&command_native.owner.lease;
 if(!at || at<previous || at-l->prepare>INTEL_DIAGNOSTIC_PREPARE_US ||
 at-l->gt>300000 || at-l->hda>400000 || at-l->outer>500000 ||
 !intel_source_engines_deadline(&source_engines_native.result,at)) {
  command_ownership.clock=at;command_ownership.previous=previous;return command_ownership_fail(2);
 }
 command_ownership.last=at;return true;
}
static bool command_ownership_guard(unsigned page) {
 if(!command_ownership_tick() || !source_engines_guard(&nonzero_native) ||
 (page!=UINT32_MAX && !completion_mapping(nonzero_native.a,page)))return command_ownership_fail(4);
 return command_ownership_tick();
}
static bool command_ownership_group(unsigned n,unsigned kind,unsigned group,unsigned count,uint32_t *out) {
 static const unsigned plane_offsets[13]={0x180,0x19c,0x1ac,0x1b0,0x1bc,0x80,0x84,0xac,0x280,0x29c,0x2ac,0x2b0,0x2bc};
 static const unsigned engine_offsets[6]={0x30,0x34,0x38,0x3c,0x234,0x238};
 unsigned page=kind==0?UINT32_MAX:kind==1?3+group:kind==2?10+group:13;
 if(n!=command_ownership.samples || n>=2 || !out || kind>3 ||
 (kind==0?(group || count!=9):kind==1?(group>=5 || count!=6):kind==2?(group>=3 || count!=13):(group || count!=12)) ||
 !command_ownership_guard(page))return command_ownership_fail(8);
 for(unsigned i=0;i<count;i++) {
  if(!kind && !intel_pci_requester_prefix(out,i))return command_ownership_fail(32);
  if(!command_ownership_tick())return false;
  if(kind){
   uint32_t command=UINT32_MAX;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   bool known=intel_display_test_ownership_command(&command);
#else
   command=pci_read_config32(nonzero_native.a->fw.gt.a.common.gpu,4);bool known=true;
#endif
   ++command_ownership.guard_reads;
   if(!known || command!=source_audit_native.pci[1][1] || (command&65535)!=3){
    command_ownership.bad_command=command;return command_ownership_fail(32);
   }
   if(!command_ownership_live())return command_ownership_fail(1);
   /* Account config-read latency before the following MMIO load. This clock
    * cannot restart any original parent or preparation deadline. */
   if(!command_ownership_tick())return false;
  }
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  bool ok=intel_display_test_ownership_load(kind,group,i,&out[i]);
#else
  bool ok=true;
  if(!kind)out[i]=pci_read_config32(nonzero_native.a->fw.gt.a.common.gpu,(uint8_t)intel_pci_requester_offsets[i]);
  else out[i]=*(volatile const uint32_t *)(WAKE_VA+page*4096+
   (kind==1?engine_offsets[i]:kind==2?plane_offsets[i]:8*(i/3)+(i%3==1?0:4)));
#endif
  if(!ok)return command_ownership_fail(8);
  ++command_ownership.reads;
  if(!command_ownership_tick())return false;
  if(!kind && (!intel_pci_requester_prefix(out,i+1) ||
    (i<8 && out[i]!=source_audit_native.pci[1][i]) || (i==1 && (out[i]&65535)!=3) ||
    (i==8 && out[i])))return command_ownership_fail(32);
 }
 (void)plane_offsets;(void)engine_offsets;
 ++command_ownership.groups;return command_ownership_guard(page);
}
static bool command_ownership_cpu(unsigned n) {
 struct x86_cache_snapshot cache;struct vmm_alias_observation alias;
 struct intel_command_memory *c=&command_native.owner;
 if(!command_ownership_guard(UINT32_MAX))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_ownership_cpu(&cache,&alias);
#else
 bool ok=intel_cache_probe_context(&cache) &&
 vmm_diagnostic_kernel_alias(c->root,COMMAND_NATIVE_VA,c->physical,4,cache.cpu.physical_bits,&alias,NULL);
#endif
 if(!ok || !x86_cache_equal(&cache,&command_native.cache) || cache.status!=CACHE_OBSERVED ||
 cache.count>X86_CACHE_VARIABLES || cache.fixed_count>X86_CACHE_FIXED || cache.reads!=6+2*cache.count+cache.fixed_count ||
 cache.begin_us<command_ownership.sample[n].begin || cache.end_us<=cache.begin_us ||
 !command_ownership_tick() || cache.end_us>command_ownership.last ||
 alias.pages!=4 || alias.leaves!=4 || alias.pat_index || alias.writable!=1 || alias.user ||
 alias.no_execute!=1 || alias.leaf_sizes!=1 || !command_backing(&command_native,c->physical,4))return command_ownership_fail(16);
 __typeof__(command_ownership.sample[0]) *s=&command_ownership.sample[n];
 const uint32_t a[]={alias.pages,alias.leaves,alias.pat_index,alias.writable,alias.user,alias.no_execute,alias.leaf_sizes};
 for(unsigned i=0;i<7;i++)s->alias[i]=a[i];
 unsigned checks=0;
 if(!x86_cache_range(&cache,c->physical,4,0,&s->cache[0],&s->cache[1],&s->cache[2],&checks) ||
 !checks || s->cache[0]!=6 || s->cache[1]!=6 || s->cache[2]!=6)return command_ownership_fail(16);
 s->cache_begin=cache.begin_us;s->cache_end=cache.end_us;s->cache_reads=cache.reads;s->backing=1;
 ++command_ownership.groups;return command_ownership_guard(UINT32_MAX);
}
static bool command_ownership_run(void) {
 struct intel_command_memory *c=&command_native.owner;
 if(command_ownership.object || !intel_command_memory_returned(c) || !command_lease_identity(&c->lease))return false;
 command_ownership.object=&command_ownership;command_ownership.root=c->root;
 command_ownership.physical=c->physical;command_ownership.serial=c->serial;
 command_ownership.begin=command_ownership.last=c->end;command_ownership.state=1;
 if(!intel_command_memory_consumer_begin(c,&command_ownership))return command_ownership_fail(1);
 command_ownership.registered=true;
 if(intel_command_memory_returned(c) || intel_command_memory_close(c))return command_ownership_fail(1);
 static const unsigned old_plane[9]={0,1,2,5,6,7,8,9,10};
 for(unsigned n=0;n<2;n++) {
  __typeof__(command_ownership.sample[0]) *s=&command_ownership.sample[n];
  if(!command_ownership_guard(UINT32_MAX))return false;
  s->begin=command_ownership.last;
  if(!command_ownership_group(n,0,0,9,s->pci))return false;
  if(!intel_pci_requester_prefix(s->pci,9) || (s->pci[1]&65535)!=3 || s->pci[8])return command_ownership_fail(32);
  for(unsigned i=0;i<9;i++)if(s->pci[i]!=source_audit_native.pci[1][i])return command_ownership_fail(32);
  for(unsigned e=0;e<5;e++) {
   if(!command_ownership_group(n,1,e,6,s->engine[e]))return false;
   for(unsigned i=0;i<6;i++)if(s->engine[e][i]!=nonzero_native.r.post.words[1][e][i])return command_ownership_fail(32);
  }
  for(unsigned p=0;p<3;p++) {
   if(!command_ownership_group(n,2,p,13,s->plane[p]))return false;
   for(unsigned i=0;i<13;i++)if(s->plane[p][i]==UINT32_MAX)return command_ownership_fail(32);
   for(unsigned i=0;i<9;i++)if(s->plane[p][old_plane[i]]!=nonzero_native.r.readers.planes[p][i])return command_ownership_fail(32);
  }
  uint32_t pte[12];
  if(!command_ownership_group(n,3,0,12,pte))return false;
  for(unsigned i=0;i<4;i++){
   if(pte[i*3]!=pte[i*3+2])return command_ownership_fail(32);
   s->pte[i]=(uint64_t)pte[i*3+1]|(uint64_t)pte[i*3]<<32;
   if(s->pte[i]!=window_result.sample[1].entry[i])return command_ownership_fail(32);
  }
  if(!command_ownership_cpu(n) || !command_ownership_tick())return false;
  s->end=command_ownership.last;
  if(s->end<=s->begin)return command_ownership_fail(2);
  ++command_ownership.samples;
 }
 const __typeof__(command_ownership.sample[0]) *a=&command_ownership.sample[0],*b=&command_ownership.sample[1];
 for(unsigned i=0;i<9;i++)if(a->pci[i]!=b->pci[i])return command_ownership_fail(32);
 for(unsigned e=0;e<5;e++)for(unsigned i=0;i<6;i++)if(a->engine[e][i]!=b->engine[e][i])return command_ownership_fail(32);
 for(unsigned p=0;p<3;p++)for(unsigned i=0;i<13;i++)if(a->plane[p][i]!=b->plane[p][i])return command_ownership_fail(32);
 for(unsigned i=0;i<4;i++)if(a->pte[i]!=b->pte[i])return command_ownership_fail(32);
 if(!command_ownership_guard(UINT32_MAX) || command_ownership.reads!=180 || command_ownership.groups!=22 || command_ownership.guard_reads!=162 ||
 !intel_command_memory_consumer_end(c,&command_ownership))return command_ownership_fail(1);
 /* RAM and actual CPU alias/cache policy only. No positive interval, GPU
  * DMA/cache or all-reader completion witness can currently be issued. */
 command_ownership.verified=6;command_ownership.missing=25;
 command_ownership.end=command_ownership.last;command_ownership.registered=false;
 command_ownership.state=2;command_ownership.complete=true;return true;
}
static bool command_ownership_returned(void) {
 return command_ownership.object==&command_ownership && command_ownership.state==2 &&
 command_ownership.complete && !command_ownership.registered && !command_ownership.reason &&
 command_ownership.samples==2 && command_ownership.reads==180 && command_ownership.groups==22 && command_ownership.guard_reads==162 && !command_ownership.bad_command &&
 command_ownership.verified==6 && command_ownership.missing==25 &&
 command_ownership.end==command_ownership.last && command_ownership.end>command_ownership.begin &&
 command_ownership.root==command_native.owner.root && command_ownership.physical==command_native.owner.physical &&
 command_ownership.serial==command_native.owner.serial && !command_ownership.clock && !command_ownership.previous &&
 command_ownership.end<=source_engines_native.result.child_end;
}
static void command_ownership_report(void) {
 static uint8_t raw[1824];static char encoded[2433];unsigned k=0;
 static const struct intel_command_lease empty;
 const struct intel_command_lease *l=command_ownership.object?&command_native.owner.lease:&empty;
 const uint64_t header[]={command_ownership.state,command_ownership.reason,command_ownership.registered,command_ownership.complete,
 command_ownership.begin,command_ownership.last,command_ownership.end,command_ownership.clock,command_ownership.previous,
 command_ownership.root,command_ownership.physical,command_ownership.serial,l->epoch,command_ownership.object?source_engines_native.result.kind:0,
 l->prepare,l->gt,l->hda,l->outer,command_ownership.samples,command_ownership.reads,command_ownership.groups,
 command_ownership.verified,command_ownership.missing,INTEL_DIAGNOSTIC_PREPARE_US,8704,4,command_ownership.guard_reads,command_ownership.bad_command,
 command_ownership.object?source_engines_native.issued:0,0,0,0};
 _Static_assert(sizeof(header)==256,"command ownership header");
 for(unsigned i=0;i<32;i++)source_audit_u64(raw,&k,header[i]);
 for(unsigned n=0;n<2;n++) {
  const __typeof__(command_ownership.sample[0]) *s=&command_ownership.sample[n];
  source_audit_u64(raw,&k,s->begin);source_audit_u64(raw,&k,s->end);
  for(unsigned i=0;i<9;i++)source_audit_u64(raw,&k,s->pci[i]);
  for(unsigned e=0;e<5;e++)for(unsigned i=0;i<6;i++)source_audit_u64(raw,&k,s->engine[e][i]);
  for(unsigned p=0;p<3;p++)for(unsigned i=0;i<13;i++)source_audit_u64(raw,&k,s->plane[p][i]);
  for(unsigned i=0;i<4;i++)source_audit_u64(raw,&k,s->pte[i]);
  for(unsigned i=0;i<7;i++)source_audit_u64(raw,&k,s->alias[i]);
  for(unsigned i=0;i<3;i++)source_audit_u64(raw,&k,s->cache[i]);
  source_audit_u64(raw,&k,s->backing);source_audit_u64(raw,&k,s->cache_begin);
  source_audit_u64(raw,&k,s->cache_end);source_audit_u64(raw,&k,s->cache_reads);
 }
 entry_requester_base64(raw,k,encoded);console_printf("intel-command-ownership: v=1 b=%s\n",encoded);
 console_printf("intel-command-provider: state=%u reason=%u samples=%u reads=%u groups=%u checked=%u missing=%u complete=%u clock=%u previous=%u; no GPU grant\n",
 (uint64_t)command_ownership.state,(uint64_t)command_ownership.reason,
 (uint64_t)command_ownership.samples,(uint64_t)command_ownership.reads,(uint64_t)command_ownership.groups,
 (uint64_t)command_ownership.verified,(uint64_t)command_ownership.missing,(uint64_t)command_ownership.complete,
 command_ownership.clock,command_ownership.previous);
}
