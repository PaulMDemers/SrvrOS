/* Exact image only: original candidate PTE through an already pinned RO/NX/UC
 * alias, after reset acknowledgement and before its provider-exit census. */
static bool einterval_guard(void *ctx) {
 struct engine_prepare_adapter *p=ctx;
 return intel_engine_prepare_reset_live(&p->trial->reset) && eprepare_guard(p)
#ifdef SRVROS_INTEL_REHOLD
  && erehold_child_guard(p)
#endif
  ;
}
static uint64_t einterval_now(void *ctx) {return eprepare_now(ctx);}
static bool einterval_read(void *ctx,uint32_t offset,uint32_t *out) {
 struct engine_prepare_adapter *p=ctx;
#ifdef SRVROS_INTEL_JOINT
 const struct intel_interval_claim *c=&p->trial->claim;
 bool closing=c->state==IIC_CLOSING;
 const struct intel_held_interval_result *r=closing?&c->closing:&c->opening;
 unsigned start=closing?6:0,limit=start+6;
 if(!out || p->interval_reads<start || p->interval_reads>=limit || r->attempts!=p->interval_reads-start+1 ||
#else
 const struct intel_held_interval_result *r=&p->trial->interval;
 if(!out || p->interval_reads>=6 || r->attempts!=p->interval_reads+1 ||
#endif
    offset!=INTEL_WINDOW_PAGE+(p->interval_reads%3==1?0:4) ||
    !einterval_guard(p) || !completion_mapping(p->a,13) || !engine_requesters(p->a))return false;
 /* Last same-origin deadline check immediately precedes the sole DWORD load.
  * Neither a fresh helper clock nor the longer GT-owner budget authorizes it. */
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_interval_admission();
#endif
 uint64_t at=eprepare_now(p);
#ifdef SRVROS_INTEL_REHOLD
 if(!erehold_time(p,at))return false;
#else
 if(!at || at<r->last_us || at<r->origin_us || at-r->origin_us>INTEL_ENGINE_PREPARE_TOTAL_US)return false;
#endif
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_interval_read(offset,out);
#else
 *out=*(volatile const uint32_t *)(WAKE_VA+13*4096+(offset&4095));bool ok=true;
#endif
 if(ok)++p->interval_reads;
 return ok;
}
#ifndef SRVROS_INTEL_JOINT
static bool einterval_run(struct engine_prepare_adapter *p) {
 struct scanout_handoff_snapshot snapshot={0};
 struct intel_held_interval_result *r=&p->trial->interval;
 bool saved=intel_window_handoff_snapshot(&snapshot);
 bool ok=intel_held_interval_observe(r,&p->trial->reset,saved?&snapshot:NULL,
  (struct intel_held_interval_io){p,einterval_guard,einterval_read,einterval_now});
 if(!ok)return false;
 struct scanout_transfer_manifest data;
 r->bound=intel_held_interval_manifest(r,&data) && data.original==&r->expected &&
  data.original[0]==snapshot.original[0] && data.source_epoch==p->g->engine_epoch &&
  data.first==INTEL_WINDOW_FIRST && data.pages==1 && !data.assurances && data.obligations==INTEL_COMPLETION_OPEN;
 if(!r->bound){r->reason|=IHI_PARENT;r->complete=false;return false;}
 return true;
}
#else
static bool einterval_claim_run(struct engine_prepare_adapter *p) {
 struct scanout_handoff_snapshot snapshot={0};
 struct intel_interval_claim *c=&p->trial->claim;
 if(!intel_window_handoff_snapshot(&snapshot) || !intel_hda_gpu_attach(&joint.scope,c))return false;
 struct intel_held_interval_io io={p,einterval_guard,einterval_read,einterval_now};
 if(!intel_interval_claim_open(c,&p->trial->rehold_child,&snapshot,io))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 if(!intel_display_test_joint_claim_active(c))return true;
#endif
#ifdef SRVROS_INTEL_GPU_HOLD
 if(!gpu_hold_run(p))return false;
#endif
 return intel_interval_claim_check(c) && intel_interval_claim_close(c);
}
#endif
static void einterval_report_named(const struct intel_held_interval_result *r,const char *prefix) {
 static const char hex[]="0123456789abcdef";
 for(unsigned n=0;n<2;n++) {
  char raw[25];unsigned count=r->counts[n]>3?3:r->counts[n];
  for(unsigned i=0;i<count;i++)for(unsigned j=0;j<8;j++)raw[i*8+j]=hex[(r->words[n][i]>>(28-j*4))&15];
  raw[count*8]=0;
  console_printf("%s-row: v=1 s=%u n=%u begin=%u end=%u h=%s\n",prefix,(uint64_t)n,(uint64_t)count,r->sample_begin[n],r->sample_end[n],count?raw:"-");
 }
 console_printf("%s-summary: v=1 origin=%u begin=%u end=%u epoch_before=%u epoch_after=%u first=%u pages=%u attempts=%u reads=%u samples=%u n0=%u n1=%u stable=%u bound=%u reason=%u clock_at=%u clock_prev=%u complete=%u obligations=255 assurances=0 pte_writes=0 managed_pages=0 hardware_ownership=0\n",prefix,r->origin_us,r->begin_us,r->end_us,(uint64_t)r->epoch_before,(uint64_t)r->epoch_after,(uint64_t)r->manifest.first,(uint64_t)r->manifest.pages,(uint64_t)r->attempts,(uint64_t)r->reads,(uint64_t)r->samples,(uint64_t)r->counts[0],(uint64_t)r->counts[1],(uint64_t)r->stable,(uint64_t)r->bound,(uint64_t)r->reason,r->clock_at,r->clock_prev,(uint64_t)r->complete);
}
static void einterval_report(const struct intel_held_interval_result *r) {einterval_report_named(r,"intel-held-interval");}
#ifdef SRVROS_INTEL_JOINT
static void einterval_claim_report(const struct intel_interval_claim *c) {
 if(c->object)einterval_report_named(&c->closing,"intel-claim-final");
 console_printf("intel-claim-summary: v=1 state=%u reason=%u pinned=%u origin=%u owner_origin=%u begin=%u end=%u last=%u epoch=%u expected=%u clock_at=%u clock_prev=%u complete=%u obligations=255 assurances=0 pte_writes=0 managed_pages=0 hardware_ownership=0\n",
  (uint64_t)c->state,(uint64_t)c->reason,(uint64_t)c->pinned,c->origin_us,c->owner_origin_us,c->begin_us,c->end_us,c->last_us,(uint64_t)c->epoch,c->expected,c->clock_at,c->clock_prev,
  (uint64_t)(c->state==IIC_CLOSED && !c->pinned && !c->reason && c->opening.complete && c->opening.bound && c->closing.complete && c->closing.bound));
}
#endif
/* All74 plane/HDA DWORDs survive in one canonical positional base64 row.
 * This image alone compacts them; the65,536-byte log tail never grows. */
static void einterval_preservation_report(const struct engine_prepare_trial *t) {
 static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 uint8_t bytes[296];char data[397];unsigned count=t->samples>2?2:t->samples,k=0;
 for(unsigned n=0;n<count;n++)for(unsigned i=0;i<37;i++) {
  uint32_t v=i<27?t->planes[n][i/9][i%9]:t->audio[n][i-27];
  for(unsigned j=0;j<4;j++)bytes[(n*37+i)*4+j]=(uint8_t)(v>>(j*8));
 }
 for(unsigned i=0;i<count*148;i+=3) {
  unsigned left=count*148-i;uint32_t v=(uint32_t)bytes[i]<<16;
  if(left>1)v|=(uint32_t)bytes[i+1]<<8;
  if(left>2)v|=bytes[i+2];
  data[k++]=alphabet[v>>18];data[k++]=alphabet[(v>>12)&63];
  data[k++]=left>1?alphabet[(v>>6)&63]:'=';data[k++]=left>2?alphabet[v&63]:'=';
 }
 data[k]=0;
 console_printf("intel-prepare-preservation: v=3 n=%u b=%s\n",(uint64_t)(count*37),count?data:"-");
}
