/* Source-transfer image only. Full hardware guards enclose this complete batch;
 * raw loads/stores additionally retain original root/map/clock admission. */
static bool source_requester_guard(void *ctx) {
 struct nonzero_adapter *p=ctx;const struct intel_source_requester *r=&source_requester_native.result;
 return p==&nonzero_native && !source_requester_native.uncertain &&
  p->source_batch_entered && !p->source_batch_checked && p->source_reads==6 && p->source.count==6 && p->source.stable &&
  r->transfer==&source_transfer_native && r->buffers==&buffers_native.binding &&
  r->source==&p->source && r->provider==p && r->input==&p->source_input &&
  intel_source_requester_live(r) && buffers_guard(p) && nonzero_guard(p);
}
static uint64_t source_requester_now(void *ctx) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t at;if(intel_display_test_source_requester_clock(&at))return at;
#endif
 return nonzero_now(ctx);
}
static bool source_requester_admit(struct nonzero_adapter *p) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_source_requester_admission();
#endif
 if(!source_requester_guard(p))return false;
 struct intel_source_requester *r=&source_requester_native.result;
 unsigned state=r->state,phase=r->phase,attempts=r->attempts,writes=r->writes;
 unsigned samples=r->samples,polls=r->polls,issued=source_requester_native.issued;
 uint64_t at=source_requester_now(p);
 if(!intel_source_requester_deadline(r,at))return false;
 if(r->state!=state || r->phase!=phase || r->attempts!=attempts || r->writes!=writes ||
    r->samples!=samples || r->polls!=polls || source_requester_native.issued!=issued || source_requester_native.uncertain) {
  r->reason|=ISR_SOURCE;r->state=ISR_FAULT;return false;
 }
 if(r->state==ISR_INHIBITED && !r->child_entered && r->drain_begin &&
    at-r->drain_begin>INTEL_SOURCE_REQUESTER_DRAIN_US) {
  r->reason|=ISR_PENDING;r->state=ISR_FAULT;return false;
 }
 return true;
}
static bool source_requester_read(struct nonzero_adapter *p,unsigned offset,uint32_t *out) {
 if(!out || !source_requester_admit(p))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_source_requester_read(offset,out);
#else
 *out=pci_read_config32(p->a->fw.gt.a.common.gpu,(uint8_t)offset);bool ok=true;
#endif
 if(ok)++source_requester_native.reads;
 return ok;
}
static bool source_requester_sample(void *ctx,uint32_t out[9],unsigned *count) {
 struct nonzero_adapter *p=ctx;struct intel_source_requester *r=&source_requester_native.result;
 if(!out || !count || r->phase>=4 || out!=r->words[r->phase] || count!=&r->counts[r->phase] || *count)return false;
 for(unsigned i=0;i<9;i++) {
  if(!intel_pci_requester_prefix(out,*count) || !source_requester_read(p,intel_pci_requester_offsets[i],&out[i]))return false;
  ++*count;if(!intel_pci_requester_prefix(out,*count))return false;
  if(i==1 && (out[i]&65535)!=(r->phase==1 || r->phase==2?3u:7u))return false;
  if(r->phase && i<8 && (out[i]&(i==1?~4u:UINT32_MAX))!=(r->words[0][i]&(i==1?~4u:UINT32_MAX)))return false;
 }
 return true;
}
static bool source_requester_pending(void *ctx,uint32_t out[2],unsigned *count) {
 struct nonzero_adapter *p=ctx;struct intel_source_requester *r=&source_requester_native.result;
 if(!out || !count || !r->polls || r->polls>32 || out!=r->poll_words[r->polls-1] || count!=&r->poll_counts[r->polls-1] || *count)return false;
 if(!source_requester_read(p,4,&out[0]))return false;
 ++*count;
 if(out[0]!=r->words[1][1] || !source_requester_read(p,0xa8,&out[1]))return false;
 ++*count;return true;
}
static enum intel_gt_write source_requester_command(void *ctx,uint16_t value) {
 struct nonzero_adapter *p=ctx;struct intel_source_requester *r=&source_requester_native.result;
 unsigned n=source_requester_native.issued;
 if(n>=2 || r->attempts!=n+1 || r->writes!=n || !source_requester_guard(p) ||
    !((n==0 && value==3 && r->state==ISR_INHIBITING) ||
      (n==1 && value==7 && r->state==ISR_RESTORING && r->child_complete)))return GT_WRITE_NONE;
 if(!source_requester_read(p,4,&source_requester_native.admission[n]))return GT_WRITE_NONE;
 ++source_requester_native.admit_reads;
 unsigned phase=n?2:0;
 if(source_requester_native.admission[n]!=r->words[phase][1] || !source_requester_guard(p))return GT_WRITE_NONE;
 source_requester_native.store_previous[n]=r->last;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_source_requester_store_admission(n);
#endif
 if(!source_requester_guard(p))return GT_WRITE_NONE;
 uint64_t at=source_requester_now(p);source_requester_native.store_at[n]=at;
 if(!intel_source_requester_deadline(r,at))return GT_WRITE_NONE;
 /* Only pure identity/count loads follow the last clock, then the halfword.
  * Adjacent PCI Status and AF control are never written. */
 if(r->object!=r || r->transfer!=&source_transfer_native || r->source!=&p->source ||
    r->source->provider!=p || r->source->reset!=&p->r || r->attempts!=n+1 || r->writes!=n ||
    source_requester_native.issued!=n || source_requester_native.uncertain || r->phase!=phase ||
    r->samples!=(n?3u:1u) || (n?(!r->child_complete || r->state!=ISR_RESTORING):r->state!=ISR_INHIBITING) ||
    source_requester_native.admission[n]!=r->words[phase][1])return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 enum intel_gt_write issued=intel_display_test_source_requester_command16(value);
#else
 pci_write_config16(p->a->fw.gt.a.common.gpu,4,value);enum intel_gt_write issued=GT_WRITE_DONE;
#endif
 if(issued==GT_WRITE_DONE)++source_requester_native.issued;
 if(issued==GT_WRITE_UNCERTAIN)source_requester_native.uncertain=true;
 return issued;
}
static bool source_requester_child(void *ctx,struct intel_source_requester *r) {
 struct nonzero_adapter *p=ctx;
 if(r!=&source_requester_native.result || r->state!=ISR_INHIBITED || !source_requester_guard(p) ||
    source_requester_native.issued!=1 || !r->registered || !r->parent_blocked || !r->holds_blocked)return false;
 /* No GPU memory operation. Exercise the actual inhibited child's registration
  * and closure interlocks; this is not a positive DMA or address assurance. */
 if(intel_source_transfer_check(&source_transfer_native) || intel_source_transfer_close(&source_transfer_native) ||
    scanout_memory_hold_end(&buffers_native.hold[0]) || scanout_memory_hold_end(&buffers_native.hold[1]))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_source_requester_child(r);
#else
 return source_requester_guard(p);
#endif
}
static bool source_requester_run(struct nonzero_adapter *p) {
 bool ok=intel_source_requester_run(&source_requester_native.result,&source_transfer_native,
  (struct intel_source_requester_io){p,source_requester_guard,source_requester_sample,source_requester_command,
   source_requester_pending,source_requester_now,nonzero_pause},source_requester_child,p);
 if(!ok){source_requester_native.uncertain=true;p->failed=true;p->a->uncertain=true;}
 return ok;
}

static void source_requester_report(void) {
 const struct intel_source_requester *r=&source_requester_native.result;
 /* Counts and partial raw buffers survive failures; unused suffixes stay zero. */
 static uint8_t raw[756];static char data[1009];unsigned k=0,j=0;
 for(unsigned n=0;n<4;n++) {
  for(unsigned i=0;i<9;i++)for(unsigned b=0;b<4;b++)raw[k++]=(uint8_t)(r->words[n][i]>>(b*8));
  for(unsigned i=0;i<2;i++)for(unsigned b=0;b<8;b++)raw[k++]=(uint8_t)((i?r->sample_end[n]:r->sample_begin[n])>>(b*8));
  raw[k++]=(uint8_t)r->counts[n];
 }
 for(unsigned n=0;n<32;n++) {
  for(unsigned i=0;i<2;i++)for(unsigned b=0;b<4;b++)raw[k++]=(uint8_t)(r->poll_words[n][i]>>(b*8));
  for(unsigned b=0;b<8;b++)raw[k++]=(uint8_t)(r->poll_at[n]>>(b*8));
  raw[k++]=(uint8_t)r->poll_counts[n];
 }
 static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 for(unsigned i=0;i<k;i+=3) {
  unsigned left=k-i;uint32_t v=(uint32_t)raw[i]<<16;
  if(left>1)v|=(uint32_t)raw[i+1]<<8;
  if(left>2)v|=raw[i+2];
  data[j++]=abc[v>>18];data[j++]=abc[(v>>12)&63];data[j++]=left>1?abc[(v>>6)&63]:'=';data[j++]=left>2?abc[v&63]:'=';
 }
 data[j]=0;console_printf("intel-source-requester-data: v=1 b=%s\n",data);
 console_printf("intel-source-requester: v=%u state=%u reason=%u epoch=%u kind=%u post_mode=%u origin=%u parent_begin=%u begin=%u last=%u end=%u clock_at=%u clock_prev=%u drain_begin=%u child_begin=%u child_end=%u phase=%u samples=%u reads=%u attempts=%u writes=%u polls=%u borrows=%u returns=%u busy=%u registered=%u parent_blocked=%u holds_blocked=%u child_entered=%u child_complete=%u restored=%u complete=%u issued=%u native_reads=%u admit_reads=%u admit0=%u admit1=%u store0=%u previous0=%u store1=%u previous1=%u uncertain=%u transfer_borrows=%u transfer_returns=%u batch_entered=%u batch_checked=%u obligations=%u assurances=%u managed_pages=%u pte_writes=%u hardware_ownership=%u\n",
  1ull,(uint64_t)r->state,(uint64_t)r->reason,(uint64_t)r->epoch,(uint64_t)r->kind,(uint64_t)r->post_mode,(uint64_t)r->origin,
  (uint64_t)r->parent_begin,(uint64_t)r->begin,(uint64_t)r->last,(uint64_t)r->end,(uint64_t)r->clock_at,(uint64_t)r->clock_prev,(uint64_t)r->drain_begin,
  (uint64_t)r->child_begin,(uint64_t)r->child_end,(uint64_t)r->phase,(uint64_t)r->samples,(uint64_t)r->reads,(uint64_t)r->attempts,(uint64_t)r->writes,
  (uint64_t)r->polls,(uint64_t)r->borrows,(uint64_t)r->returns,(uint64_t)r->busy,(uint64_t)r->registered,(uint64_t)r->parent_blocked,(uint64_t)r->holds_blocked,
  (uint64_t)r->child_entered,(uint64_t)r->child_complete,(uint64_t)r->restored,(uint64_t)r->complete,(uint64_t)source_requester_native.issued,(uint64_t)source_requester_native.reads,(uint64_t)source_requester_native.admit_reads,
  (uint64_t)source_requester_native.admission[0],(uint64_t)source_requester_native.admission[1],source_requester_native.store_at[0],source_requester_native.store_previous[0],source_requester_native.store_at[1],source_requester_native.store_previous[1],(uint64_t)source_requester_native.uncertain,
  (uint64_t)source_transfer_native.child_borrows,(uint64_t)source_transfer_native.child_returns,(uint64_t)nonzero_native.source_batch_entered,(uint64_t)nonzero_native.source_batch_checked,255ull,0ull,0ull,
  0ull,0ull);
}
