/* RAM-only exact raw serialization. Lossless zero runs reduce wire size;
 * decoding reconstructs every byte before any proof validation. No raw hash
 * substitutes, C padding, addresses of objects or live hardware operations. */
#include <srvros/air61_read_qualification.h>
static uint8_t raw[AIR61_READ_RAW_CAP];
static unsigned total;static bool failed,encoded;
static void put(uint64_t value,unsigned n){
 if(failed || n>AIR61_READ_RAW_CAP-total){failed=true;return;}
 for(unsigned i=0;i<n;i++)raw[total++]=(uint8_t)(value>>(8*i));
}
static void bytes(const uint8_t *p,unsigned n){for(unsigned i=0;i<n;i++)put(p[i],1);}
static void words(const uint64_t *p,unsigned n){for(unsigned i=0;i<n;i++)put(p[i],8);}
static void delta(const uint64_t *before,const uint64_t *after) {
 unsigned count=0;for(unsigned i=0;i<512;i++)if(before[i]!=after[i])count++;
 put(count,2);for(unsigned i=0;i<512;i++)if(before[i]!=after[i]){put(i,2);put(after[i],8);}
}
static void cache(const struct x86_cache_snapshot *s) {
 for(unsigned i=0;i<3;i++)put(s->cpu.vendor[i],4);
 put(s->cpu.signature,4);put(s->cpu.features,4);put(s->cpu.physical_bits,4);
 const uint64_t v[]={s->cpu.cr0,s->pat,s->cap,s->def_type,s->begin_us,s->end_us};words(v,6);
 put(s->status,4);put(s->reads,4);put(s->count,4);put(s->fixed_count,4);
 for(unsigned i=0;i<16;i++){put(s->base[i],8);put(s->mask[i],8);}words(s->fixed,11);
}
static void gate(const struct ecam_boot_gate_result *g){
  const unsigned counters[]={g->phases,g->firmware_bytes,g->config_reads,g->selector_writes,g->efer_reads,g->cache_reads[0],g->cache_reads[1]};for(unsigned i=0;i<7;i++)put(counters[i],4);
  words(g->efer,2);words(g->mcfg_pa,2);bytes(g->mcfg[0],60);bytes(g->mcfg[1],60);
  for(unsigned phase=0;phase<2;phase++) {
   const struct ecam_boot_policy *p=g->policy+phase;
   put(p->extended_max,4);put(p->nx_edx,4);put(p->apic,4);
   for(unsigned i=0;i<4;i++)put(p->ids[i],4);
   put(p->present_mask,4);put(p->launch_mask,4);
   const uint64_t controls[]={p->cr4,p->irq,p->revision,p->flags,p->count,p->bsp};words(controls,6);
  }
  for(unsigned p=0;p<2;p++)put(g->firmware[p].status,4);
  for(unsigned p=0;p<2;p++)cache(g->cache+p);
  for(unsigned p=0;p<2;p++)for(unsigned q=0;q<2;q++) {
   const struct hfw_sample *f=&g->firmware[p].sample[q];for(unsigned i=0;i<5;i++)put(f->identity[i],4);put(f->count,4);put(f->table_pa,8);put(f->begin,8);put(f->end,8);bytes(f->dmar,136);
   for(unsigned i=0;i<f->count;i++){put(f->entries[i].base,8);put(f->entries[i].length,8);put(f->entries[i].type,8);}
  }
}

static bool session_valid(const char *s){
 if(!s)return false;
 const char prefix[]="session-";for(unsigned i=0;i<8;i++)if(s[i]!=prefix[i])return false;
 for(unsigned i=8;i<27;i++)if(!s[i] || (i==24?s[i]!='-':!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f'))))return false;
 return !s[27];
}
static void patch32(unsigned at,unsigned n){if(failed)return;for(unsigned i=0;i<4;i++)raw[at+i]=(uint8_t)(n>>(8*i));}
static bool packed_byte(uint8_t *out,unsigned cap,unsigned *at,uint8_t b){if(*at>=cap || *at>=AIR61_READ_RECEIPT_CAP)return false;out[(*at)++]=b;return true;}
static bool packed_u16(uint8_t *out,unsigned cap,unsigned *at,unsigned n){return packed_byte(out,cap,at,n&255) && packed_byte(out,cap,at,n>>8);}
static unsigned zeroes(unsigned at){unsigned n=0;while(at+n<total && !raw[at+n] && n<32767)n++;return n;}
static bool pack(uint8_t *out,unsigned cap,unsigned *written){
 unsigned dest=0;const char magic[]=
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
 "Z2AIR610";
#else
 "Z1AIR610";
#endif
 for(unsigned i=0;i<8;i++)if(!packed_byte(out,cap,&dest,magic[i]))return false;
 for(unsigned i=0;i<4;i++)if(!packed_byte(out,cap,&dest,total>>(8*i)))return false;
 for(unsigned at=0;at<total;){
  unsigned zeros=zeroes(at);
  if(zeros>=4){if(!packed_u16(out,cap,&dest,0x8000|zeros))return false;at+=zeros;continue;}
  unsigned begin=at;
  do{at++;}while(at<total && at-begin<32767 && zeroes(at)<4);
  if(!packed_u16(out,cap,&dest,at-begin))return false;
  for(unsigned i=begin;i<at;i++)if(!packed_byte(out,cap,&dest,raw[i]))return false;
 }
 *written=dest;return true;
}
bool haswell_air61_read_encode(const char *session,uint8_t *out,unsigned cap,unsigned *written){
 if(!written)return false;
 *written=0;
 if(encoded || !out || !cap || !session_valid(session))return false;
 const struct air61_read_series_result *series=vmm_air61_read_window_result();
 if(series->state!=AIR61_READ_COMPLETE || series->busy || !series->attempted || series->fault || series->started!=2 || series->retired!=2 || !air61_read_gates_equal())return false;
 encoded=true;total=0;failed=false;
 #ifdef SRVROS_AIR61_REGISTER_SAMPLE
 bytes((const uint8_t *)"R2AIR610",8);
#else
 bytes((const uint8_t *)"R1AIR610",8);
#endif
 bytes((const uint8_t *)session,27);
 const uint64_t series_values[]={series->owner,series->epoch,series->root,series->begin,series->end,series->gap_us};words(series_values,6);
 put(series->started,4);put(series->retired,4);put(series->gap_spins,4);
 for(unsigned pass=0;pass<2;pass++){
  const struct air61_read_pass_result *v=vmm_air61_read_window_pass(pass);
  const struct air61_window_result *r=&v->window;
  const struct air61_window_evidence *e=&v->evidence;
  const struct hac_result *c=&v->census;
  const struct ecam_boot_gate_result *g=pass?air61_read_gate_result1():air61_read_gate_result0();
  if(r->state!=AIR61_WINDOW_RETIRED || r->claimed || r->fault || !e->linked || !e->retired || v->slots!=12 || v->source_coalescings!=1 || g->phases!=2 || g->firmware_bytes!=3996 || c->status!=1 || c->pages>64 || c->bytes[0]>4096 || c->bytes[1]>4096)return false;
  for(unsigned p=0;p<2;p++)for(unsigned q=0;q<2;q++)if(g->firmware[p].sample[q].count>128)return false;
  unsigned length_at=total;put(0,4);unsigned begin=total;
  #ifdef SRVROS_AIR61_REGISTER_SAMPLE
  if(v->attempts!=12 || v->reads!=12)return false;
  bytes((const uint8_t *)"R2PASS61",8);
#else
  bytes((const uint8_t *)"R1PASS61",8);
#endif
  put(r->state,4);
  const uint64_t values[]={r->token.owner,r->token.epoch,r->token.root,r->pdpt,r->frames[0],r->frames[1],r->hhdm,r->begin,r->end};words(values,9);
  const unsigned counters[]={r->words,r->private_writes,r->parent_attempts,r->unlink_attempts,r->invalidations,r->frees};for(unsigned i=0;i<6;i++)put(counters[i],4);
  put(e->linked,1);put(e->retired,1);put(r->claimed,1);
  words(e->root_before,512);words(e->pdpt_before,512);
  delta(e->root_before,e->root_linked);delta(e->pdpt_before,e->pdpt_linked);
  delta(e->root_before,e->root_retired);delta(e->pdpt_before,e->pdpt_retired);
  static const uint64_t zero[512];delta(zero,e->pd);delta(zero,e->pt);
  const unsigned cc[]={c->status,c->pages,c->prepare_reads,c->prepare_pages,c->reads[0],c->reads[1],c->bytes[0],c->bytes[1],c->target_leaves};for(unsigned i=0;i<9;i++)put(cc[i],4);
  const uint64_t times[]={c->begin,c->prepare_end,c->end};words(times,3);
  bytes(c->data[0],c->bytes[0]);bytes(c->data[1],c->bytes[1]);gate(g);
  if(failed || total-begin>33000)return false;
  patch32(length_at,total-begin);
  put(v->slots,4);put(v->source_coalescings,4);put(v->slot_begin,8);put(v->slot_end,8);
  const unsigned costs[]={r->clock_samples,r->raw_loads,r->guard_checks,r->ram_checks,r->logical_reads,r->logical_stores,r->census_reads};for(unsigned i=0;i<7;i++)put(costs[i],4);
  const uint64_t costs_times[]={r->gate_end,r->census_begin,r->census_end};words(costs_times,3);
  put(pass?air61_read_gate_ram_steps1():air61_read_gate_ram_steps0(),8);
  #ifdef SRVROS_AIR61_REGISTER_SAMPLE
  put(v->attempts,4);put(v->reads,4);
  put(AIR61_SAMPLE_ADMISSIONS,4);
  for(unsigned i=0;i<AIR61_SAMPLE_ADMISSIONS;i++){
   const struct air61_sample_admission *a=&v->admission[i];
   if(!a->attempted || a->msr_reads!=a->cache.reads || a->msr_reads>49 || a->efer_reads!=1)return false;
   put(a->attempted,1);put(a->msr_reads,4);put(a->efer_reads,4);put(a->efer,8);cache(&a->cache);
  }
#endif
  for(unsigned i=0;i<12;i++){put(v->offsets[i],4);words(v->before[i],4);words(v->after[i],4);
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
   put(v->values[i],4);put(v->load_begin[i],8);put(v->load_end[i],8);
#endif
  }
 }
 return !failed && pack(out,cap,written);
}
#ifdef SRVROS_ECAM_GATE_HOST_TEST
void air61_read_report_test_reset(void){encoded=false;}
#endif
