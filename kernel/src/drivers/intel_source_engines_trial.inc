/* Dedicated v34 adapter. Existing full source-batch guards enclose all I/O;
 * each new load/store retains exact original source/root/requester admission. */
static bool source_engines_guard(void *ctx) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_source_engines_guard();
#endif
 struct nonzero_adapter *p=ctx;struct intel_source_engines *r=&source_engines_native.result;
 return p==&nonzero_native && !source_engines_native.uncertain && r->requester==&source_requester_native.result &&
  r->transfer==&source_transfer_native && r->source==&p->source && r->provider==p &&
  intel_source_engines_live(r) && source_requester_guard(p) && source_requester_native.issued==1;
}
static uint64_t source_engines_now(void *ctx) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t at;if(intel_display_test_source_engines_clock(&at))return at;
#endif
 return nonzero_now(ctx);
}
static bool source_engines_admit(struct nonzero_adapter *p) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_source_engines_admission();
#endif
 if(!source_engines_guard(p))return false;
 struct intel_source_engines *r=&source_engines_native.result;
 unsigned state=r->state,phase=r->phase,step=r->step,index=r->index,attempts=r->attempts,writes=r->writes;
 unsigned samples=r->samples,acks=r->acks,polls=r->polls,issued=source_engines_native.issued;
 unsigned counts[4];for(unsigned n=0;n<4;n++)counts[n]=r->counts[n];
 uint64_t phase_origin=step<10?r->phase_begin[step]:0;
 uint64_t at=source_engines_now(p);
 if(!intel_source_engines_deadline(r,at))return false;
 bool profile_changed=false;for(unsigned n=0;n<4;n++)if(counts[n]!=r->counts[n])profile_changed=true;
 if(profile_changed || (step<10 && r->phase_begin[step]!=phase_origin) || r->state!=state || r->phase!=phase || r->step!=step || r->index!=index || r->attempts!=attempts || r->writes!=writes ||
    r->samples!=samples || r->acks!=acks || r->polls!=polls || source_engines_native.issued!=issued || source_engines_native.uncertain) {
  r->reason|=ISE_SOURCE;r->state=ISE_FAULT;return false;
 }
 if(r->step<10 && r->attempts>r->acks && r->phase_begin[r->step] && at-r->phase_begin[r->step]>10000) {
  r->reason|=ISE_PENDING;r->state=ISE_FAULT;return false;
 }
 return true;
}
static bool source_engines_load(struct nonzero_adapter *p,unsigned e,uint32_t *out) {
 if(!out || e>=6 || !source_engines_admit(p))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_source_engines_read(e,out);
#else
 *out=*(volatile const uint32_t *)(WAKE_VA+(e<5?e+3:9)*4096+(e<5?0xd0:0x41c));bool ok=true;
#endif
 if(ok)++source_engines_native.reads;
 return ok;
}
static bool source_engines_sample(void *ctx,uint32_t out[6],unsigned *count) {
 struct nonzero_adapter *p=ctx;struct intel_source_engines *r=&source_engines_native.result;
 if(!out || !count || r->phase>=4 || out!=r->words[r->phase] || count!=&r->counts[r->phase] || *count)return false;
 for(unsigned e=0;e<6;e++) {
  if(!source_engines_load(p,e,&out[e]))return false;
  ++*count;
  uint32_t v=out[e];unsigned want=r->phase==1 || r->phase==2?3u:0u;
  if(e==5?v!=0:v==UINT32_MAX || (v&0xfffcu) || (v&3u)!=want)return false;
  if(r->phase && (v&~3u)!=(r->words[0][e]&~3u))return false;
  if((r->phase==2 && v!=r->words[1][e]) || (r->phase==3 && v!=r->words[0][e]))return false;
 }
 return true;
}
static bool source_engines_control(void *ctx,unsigned e,uint32_t *out) {
 struct intel_source_engines *r=&source_engines_native.result;
 return r->step<10 && e==r->index && source_engines_load(ctx,e,out);
}
static enum intel_gt_write source_engines_transition(void *ctx,unsigned e,uint32_t value,uint32_t *out) {
 struct nonzero_adapter *p=ctx;struct intel_source_engines *r=&source_engines_native.result;
 unsigned n=source_engines_native.issued;
 if(!out || n>=10 || r->step!=n || r->writes!=n || !intel_source_engines_write_ready(r,e,value))return GT_WRITE_NONE;
 /* The load below admits the original source/root/maps/requester and clock
  * before touching the control. A second full guard with no intervening I/O
  * adds no witness. The store retains its own full guard and final clock. */
 if(!source_engines_load(p,e,&source_engines_native.admission[n]))return GT_WRITE_NONE;
 ++source_engines_native.admit_reads;
 unsigned phase=n<5?0:2;
 if(source_engines_native.admission[n]!=r->words[phase][e])return GT_WRITE_NONE;
 const uint64_t phase_origin=r->phase_begin[n];
 source_engines_native.store_previous[n]=r->last;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_source_engines_store_admission(n);
#endif
 if(!source_engines_guard(p))return GT_WRITE_NONE;
 uint64_t at=source_engines_now(p);source_engines_native.store_at[n]=at;
 if(!intel_source_engines_deadline(r,at))return GT_WRITE_NONE;
 if(r->object!=r || r->requester!=&source_requester_native.result || r->transfer!=&source_transfer_native ||
    r->source!=&p->source || r->provider!=p || r->source->provider!=p || r->source->reset!=&p->r ||
    source_engines_native.issued!=n || source_engines_native.uncertain || r->step!=n || r->phase!=phase ||
    source_engines_native.admission[n]!=r->words[phase][e] || !intel_source_engines_write_ready(r,e,value) ||
    !phase_origin || r->phase_begin[n]!=phase_origin || at<phase_origin || at-phase_origin>10000)return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 enum intel_gt_write wrote=intel_display_test_source_engines_write(e,value);
#else
 volatile uint32_t *reg=(volatile uint32_t *)(WAKE_VA+(e+3)*4096+0xd0);
 __asm__ volatile("mfence":::"memory");*reg=value;__asm__ volatile("mfence":::"memory");
 enum intel_gt_write wrote=GT_WRITE_DONE;
#endif
 if(wrote==GT_WRITE_DONE)++source_engines_native.issued;
 if(wrote==GT_WRITE_UNCERTAIN)source_engines_native.uncertain=true;
 if(wrote!=GT_WRITE_DONE)return wrote;
 /* A first acknowledgement is fresh and independently admitted after the
  * store. Losing that admission means uncertain issuance, never cancellation. */
 if(!source_engines_load(p,e,out)){source_engines_native.uncertain=true;return GT_WRITE_UNCERTAIN;}
 return GT_WRITE_DONE;
}
static bool source_engines_child(void *ctx,struct intel_source_engines *r) {
 struct nonzero_adapter *p=ctx;
 if(r!=&source_engines_native.result || r->state!=ISE_HELD || !source_engines_guard(p) ||
    source_engines_native.issued!=5 || r->samples!=2 || !r->registered || !r->parent_blocked || !r->holds_blocked)return false;
 for(unsigned e=0;e<5;e++)if((r->words[1][e]&3u)!=3)return false;
 if(intel_source_requester_child_returned(&source_requester_native.result) ||
    intel_source_transfer_check(&source_transfer_native) || intel_source_transfer_close(&source_transfer_native) ||
    scanout_memory_hold_end(&buffers_native.hold[0]) || scanout_memory_hold_end(&buffers_native.hold[1]))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_source_engines_child(r);
#else
 return source_engines_guard(p);
#endif
}
static bool source_engines_run(struct nonzero_adapter *p) {
 bool ok=intel_source_engines_run(&source_engines_native.result,&source_requester_native.result,
  (struct intel_source_engines_io){p,source_engines_guard,source_engines_sample,source_engines_control,source_engines_transition,
   source_engines_now,nonzero_pause},source_engines_child,p);
 if(!ok){source_engines_native.uncertain=true;p->failed=true;p->a->uncertain=true;}
 return ok;
}
static void source_engines_report(void) {
 const struct intel_source_engines *r=&source_engines_native.result;
 static uint8_t raw[624];static char data[833];unsigned k=0,j=0;
 for(unsigned n=0;n<4;n++) {
  for(unsigned e=0;e<6;e++)for(unsigned b=0;b<4;b++)raw[k++]=(uint8_t)(r->words[n][e]>>(b*8));
  for(unsigned i=0;i<2;i++)for(unsigned b=0;b<8;b++)raw[k++]=(uint8_t)((i?r->sample_end[n]:r->sample_begin[n])>>(b*8));
  raw[k++]=(uint8_t)r->counts[n];
 }
 for(unsigned n=0;n<10;n++) {
  const uint32_t words[3]={r->ack[n],r->final[n],source_engines_native.admission[n]};
  for(unsigned i=0;i<3;i++)for(unsigned b=0;b<4;b++)raw[k++]=(uint8_t)(words[i]>>(b*8));
  const uint64_t times[4]={r->phase_begin[n],r->phase_end[n],source_engines_native.store_at[n],source_engines_native.store_previous[n]};
  for(unsigned i=0;i<4;i++)for(unsigned b=0;b<8;b++)raw[k++]=(uint8_t)(times[i]>>(b*8));
  raw[k++]=(uint8_t)r->phase_polls[n];raw[k++]=(uint8_t)(r->phase_polls[n]>>8);
 }
 static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 for(unsigned i=0;i<k;i+=3) {
  uint32_t v=(uint32_t)raw[i]<<16|(uint32_t)raw[i+1]<<8|raw[i+2];
  data[j++]=abc[v>>18];data[j++]=abc[(v>>12)&63];data[j++]=abc[(v>>6)&63];data[j++]=abc[v&63];
 }
 data[j]=0;console_printf("intel-source-engines-data: v=1 b=%s\n",data);
 console_printf("intel-source-engines: v=1 state=%u reason=%u epoch=%u kind=%u mode=%u origin=%u parent=%u begin=%u last=%u end=%u clock=%u previous=%u child_begin=%u child_end=%u phase=%u step=%u index=%u samples=%u reads=%u attempts=%u writes=%u acks=%u polls=%u borrows=%u returns=%u busy=%u registered=%u blocked=%u holds=%u child=%u child_done=%u restored=%u complete=%u issued=%u native_reads=%u admits=%u uncertain=%u parent_borrows=%u parent_returns=%u obligations=255 assurances=0 managed_pages=0 pte_writes=0 hardware_ownership=0\n",
  (uint64_t)r->state,(uint64_t)r->reason,(uint64_t)r->epoch,(uint64_t)r->kind,(uint64_t)r->post_mode,r->origin,r->parent_begin,r->begin,r->last,r->end,r->clock_at,r->clock_prev,r->child_begin,r->child_end,
  (uint64_t)r->phase,(uint64_t)r->step,(uint64_t)r->index,(uint64_t)r->samples,(uint64_t)r->reads,(uint64_t)r->attempts,(uint64_t)r->writes,(uint64_t)r->acks,(uint64_t)r->polls,(uint64_t)r->borrows,(uint64_t)r->returns,
  (uint64_t)r->busy,(uint64_t)r->registered,(uint64_t)r->parent_blocked,(uint64_t)r->holds_blocked,(uint64_t)r->child_entered,(uint64_t)r->child_complete,(uint64_t)r->restored,(uint64_t)r->complete,
  (uint64_t)source_engines_native.issued,(uint64_t)source_engines_native.reads,(uint64_t)source_engines_native.admit_reads,(uint64_t)source_engines_native.uncertain,
  (uint64_t)source_requester_native.result.child_borrows,(uint64_t)source_requester_native.result.child_returns);
}
