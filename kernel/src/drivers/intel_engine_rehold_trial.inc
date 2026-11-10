/* Separate image only. All aliases already pinned; full reset-provider entry
 * and exit still bracket this scope. No additional GT or MMIO domain. */
static bool erehold_guard(void *ctx) {
 struct engine_prepare_adapter *p=ctx;
 return p->trial->reset.rehold==&p->trial->rehold &&
  intel_engine_prepare_reset_live(&p->trial->reset) && eprepare_guard(p);
}
static bool erehold_time(const struct engine_prepare_adapter *p,uint64_t at) {
 const struct intel_engine_rehold *r=&p->trial->rehold;
 return at && at>=r->last_us && at>=r->origin_us && at-r->origin_us<=INTEL_ENGINE_PREPARE_TOTAL_US &&
  r->owner_origin_us && p->g->begin==r->owner_origin_us && at>=p->g->last &&
  at>=r->owner_origin_us && at-r->owner_origin_us<=GT_ACCESS_TOTAL_US;
}
static bool erehold_access(struct engine_prepare_adapter *p,unsigned slot) {
 if(!erehold_guard(p) || !completion_mapping(p->a,slot) || !engine_requesters(p->a))return false;
 uint64_t at=eprepare_now(p);
 return erehold_time(p,at);
}
static bool erehold_word(struct engine_prepare_adapter *p,unsigned i,uint32_t *out) {
 static const uint32_t regs[6]={0x20d0,0x120d0,0x1a0d0,0x1c0d0,0x220d0,0x941c};
 if(i>=6 || !out || !erehold_access(p,i==5?9:i+3))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_rehold_read(i,regs[i],out);
#else
 *out=*(volatile const uint32_t *)(WAKE_VA+(i==5?9:i+3)*4096+(regs[i]&4095));return true;
#endif
}
static bool erehold_sample(void *ctx,uint32_t out[6],unsigned *count) {
 struct engine_prepare_adapter *p=ctx;
 if(!out || !count || *count || p->rehold_reads>=24 || p->rehold_reads%6)return false;
 for(unsigned i=0;i<6;i++) {
  if(!erehold_word(p,i,&out[i]))return false;
  ++*count;++p->rehold_reads;
  if(out[i]==UINT32_MAX)return false;
 }
 return true;
}
static bool erehold_control(void *ctx,uint32_t *out) {return erehold_word(ctx,4,out);}
static enum intel_gt_write erehold_transition(void *ctx,uint32_t value,uint32_t *out) {
 struct engine_prepare_adapter *p=ctx;struct intel_engine_rehold *r=&p->trial->rehold;
 unsigned n=p->rehold_stores;
 if(!out || n>=2 || r->attempts!=n+1 || r->child ||
    r->stage!=(n?4u:2u) || value!=(n?INTEL_ENGINE_PREPARE_CANCEL:INTEL_ENGINE_PREPARE_REQUEST) ||
    !erehold_access(p,7))return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_rehold_admission();
#endif
 uint64_t at=eprepare_now(p);
 if(!erehold_time(p,at))return GT_WRITE_NONE;
 ++p->rehold_stores;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_rehold_transition(value,out);
#else
 volatile uint32_t *reg=(volatile uint32_t *)(WAKE_VA+7*4096+0xd0);
 __asm__ volatile("mfence":::"memory");*reg=value;__asm__ volatile("mfence":::"memory");
 if(!engine_requesters(p->a))return GT_WRITE_UNCERTAIN;
 *out=*reg;return GT_WRITE_DONE;
#endif
}
static bool erehold_child_guard(struct engine_prepare_adapter *p) {
 struct engine_prepare_trial *t=p->trial;
 if(t->live_failed || !intel_engine_rehold_child_live(&t->rehold_child))return false;
 ++t->live_checks;t->live_count=0;
 for(unsigned i=0;i<5;i++) {
  uint32_t value;
  if(!erehold_word(p,i,&value)){t->live_failed=true;return false;}
  t->live_words[i]=value;++t->live_count;++t->live_reads;
  if(value!=t->rehold.words[1][i] || (value&0xffffu)!=3){t->live_failed=true;return false;}
 }
 return true;
}
#ifdef SRVROS_INTEL_JOINT
static bool einterval_claim_run(struct engine_prepare_adapter *);
#else
static bool einterval_run(struct engine_prepare_adapter *);
#endif
static bool erehold_observe(void *ctx,struct intel_engine_rehold *r) {
 struct engine_prepare_adapter *p=ctx;
 if(r!=&p->trial->rehold || !intel_engine_rehold_borrow(r,&p->trial->rehold_child))return false;
#ifdef SRVROS_INTEL_JOINT
 if(!einterval_claim_run(p))return false;
#else
 if(!einterval_run(p))return false;
#endif
 return intel_engine_rehold_return(&p->trial->rehold_child);
}
static bool erehold_run(struct engine_prepare_adapter *p) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_rehold_source(p);
#endif
 return intel_engine_rehold_run(&p->trial->rehold,&p->trial->reset,
  (struct intel_engine_rehold_io){p,erehold_guard,erehold_sample,erehold_control,erehold_transition,erehold_observe,eprepare_now,eprepare_pause});
}
static void erehold_report(const struct engine_prepare_trial *t) {
 const struct intel_engine_rehold *r=&t->rehold;static const char hex[]="0123456789abcdef";
 for(unsigned n=0;n<5;n++) {
  unsigned count=n<4?r->counts[n]:t->live_count;if(count>(n<4?6u:5u))count=0;
  char raw[49];for(unsigned i=0;i<count;i++)for(unsigned j=0;j<8;j++)raw[i*8+j]=hex[((n<4?r->words[n][i]:t->live_words[i])>>(28-j*4))&15];raw[count*8]=0;
  console_printf("intel-rehold-row: v=1 s=%u n=%u h=%s\n",(uint64_t)n,(uint64_t)count,count?raw:"-");
 }
 for(unsigned n=0;n<r->writes && n<2;n++)console_printf("intel-rehold-ack: v=1 n=%u d=%u,%u,%u begin=%u end=%u\n",(uint64_t)n,(uint64_t)r->ack[n],(uint64_t)r->final[n],(uint64_t)r->phase_polls[n],r->phase_begin[n],r->phase_end[n]);
 console_printf("intel-rehold-summary: v=%u origin=%u owner_origin=%u begin=%u end=%u epoch=%u attempts=%u writes=%u reads=%u polls=%u acks=%u samples=%u child_borrows=%u child_returns=%u child_started=%u child_completed=%u child_retained=%u restored=%u uncertain=%u stage=%u reason=%u clock_at=%u clock_prev=%u live_checks=%u live_reads=%u live_failed=%u complete=%u obligations=255 assurances=0 pte_writes=0 managed_pages=0 hardware_ownership=0\n",
#ifdef SRVROS_INTEL_JOINT
 (uint64_t)2,
#else
 (uint64_t)1,
#endif
 r->origin_us,r->owner_origin_us,r->begin_us,r->end_us,(uint64_t)r->epoch,(uint64_t)r->attempts,(uint64_t)r->writes,(uint64_t)r->reads,(uint64_t)r->polls,(uint64_t)r->acks,(uint64_t)r->samples,(uint64_t)r->child_borrows,(uint64_t)r->child_returns,(uint64_t)r->child_started,(uint64_t)r->child_completed,(uint64_t)(r->child!=NULL),(uint64_t)r->restored,(uint64_t)r->uncertain,(uint64_t)r->stage,(uint64_t)r->reason,r->clock_at,r->clock_prev,(uint64_t)t->live_checks,(uint64_t)t->live_reads,(uint64_t)t->live_failed,(uint64_t)r->complete);
}
