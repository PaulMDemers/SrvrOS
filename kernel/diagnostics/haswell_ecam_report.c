/* Lossless bounded dual-file v5 receipt. No hardware operation during reporting. */
#include <srvros/ecam_boot.h>
#include <srvros/console.h>
#include <srvros/halt.h>
#include <srvros/timer.h>
#include <stddef.h>
static uint8_t chunk[512];static unsigned used,total,chunks;static bool failed,reported;
static char reported_session[28];
static const char *session_name(const char *p) {
 if(!p)return NULL;
 const char *s=p;for(;*p;p++)if(*p=='/')s=p+1;
 unsigned length=0;while(length<28 && s[length])length++;if(length!=27)return NULL;
 const char prefix[]="session-";for(unsigned i=0;i<8;i++)if(s[i]!=prefix[i])return NULL;
 for(unsigned i=8;i<27;i++)if(i==24 ? s[i]!='-' : !((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return NULL;
 return s[27]?NULL:s;
}
static void flush(void) {
 if(!used)return;
 static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 char out[685];unsigned at=0;
 for(unsigned i=0;i<used;i+=3){unsigned n=used-i;uint32_t v=(uint32_t)chunk[i]<<16;if(n>1)v|=(uint32_t)chunk[i+1]<<8;if(n>2)v|=chunk[i+2];out[at++]=alphabet[v>>18];out[at++]=alphabet[(v>>12)&63];out[at++]=n>1?alphabet[(v>>6)&63]:'=';out[at++]=n>2?alphabet[v&63]:'=';}
 out[at]=0;console_printf("hsw-ecam-data: v=5 chunk=%u data=%s\n",(uint64_t)chunks++,out);used=0;
}
static void put(uint64_t value,unsigned n) {
 if(n>33000-total){failed=true;return;}
 for(unsigned i=0;i<n;i++){chunk[used++]=(uint8_t)(value>>(8*i));total++;if(used==512)flush();}
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
bool haswell_ecam_boot_report(const char *directory) {
 const char *s=session_name(directory);if(reported || !s)return false;
 reported=true;for(unsigned i=0;i<28;i++)reported_session[i]=s[i];
 const struct ecam_boot_result *r=vmm_ecam_boot_result();
 const struct ecam_boot_gate_result *g=haswell_ecam_boot_gate_result();
 const struct ecam_boot_evidence *e=vmm_ecam_boot_evidence();
 bool ok=r->state==ECAM_ACTIVE && r->adoptions==1 && g->phases==2 && e->count_before<=128 && e->count_after==128;
 for(unsigned i=0;i<512;i++)if(e->root_before[i]!=e->root_after[i] || e->pdpt_after[i]!=(i==12?r->frame|3:e->pdpt_before[i]))ok=false;
 for(unsigned p=0;p<2;p++)for(unsigned q=0;q<2;q++)if(!g->firmware[p].sample[q].count || g->firmware[p].sample[q].count>128)ok=false;
 console_printf("hsw-ecam: v=5 state=%u complete=%u session=%s epoch=%u root=%x frame=%x\n",(uint64_t)r->state,(uint64_t)ok,s,r->begin,r->root,r->frame);
 used=total=chunks=0;failed=false;
 if(ok) {
  bytes((const uint8_t *)"E5ECAM01",8);put(r->state,4);
  const uint64_t v[]={r->root,r->pdpt,r->frame,r->hhdm,r->begin,r->end,r->adopted_us};words(v,7);
  const unsigned c[]={r->words,r->private_writes,r->parent_attempts,r->invalidations,r->metadata_attempts,r->frees,r->adoptions};for(unsigned i=0;i<7;i++)put(c[i],4);
  put(e->count_before,8);put(e->count_after,8);
  words(e->root_before,512);words(e->pdpt_before,512);delta(e->root_before,e->root_after);delta(e->pdpt_before,e->pdpt_after);words(e->pd,512);
  words(&e->metadata_before[0][0],384);words(&e->metadata_after[0][0],384);
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
  flush();
 }
 console_printf("hsw-ecam-end: v=5 bytes=%u chunks=%u report=%u ok=%u\n",(uint64_t)total,(uint64_t)chunks,timer_monotonic_us(),(uint64_t)(ok&&!failed));
 return ok&&!failed;
}
void haswell_ecam_boot_reference(const char *directory,bool saved) {
 const char *s=session_name(directory);if(!s || !reported)return;
 for(unsigned i=0;i<28;i++)if(s[i]!=reported_session[i])return;
 const struct ecam_boot_result *r=vmm_ecam_boot_result();
 console_printf("hsw-ecam-ref: v=5 session=%s epoch=%u root=%x frame=%x state=%u adopted=%u saved=%u\n",s,r->begin,r->root,r->frame,(uint64_t)r->state,(uint64_t)r->adoptions,(uint64_t)saved);
}
/* Only after terminal failure. The display clock is sampled once, separately
 * from the latched operation sample; it cannot restart or extend the lifetime. */
void haswell_ecam_boot_failure_report(void) {
 const struct ecam_boot_result *r=vmm_ecam_boot_result();
 const struct ecam_boot_diagnostic *d=vmm_ecam_boot_diagnostic();
 const struct ecam_boot_gate_diagnostic *g=haswell_ecam_boot_gate_diagnostic();
 const struct ecam_boot_gate_result *gate=haswell_ecam_boot_gate_result();
 uint64_t display=timer_monotonic_us();
 bool elapsed=d->reason && d->sample_clock_valid && d->sample_us>=r->begin && r->begin;
 console_printf("ecam-failure: v=1 stage=%s reason=%s\n",d->stage?d->stage:"unrecorded",d->reason?d->reason:"unrecorded");
 console_printf("ecam-failure-time: begin=%u prepared=%u handoff=%u adopt_entry_last=%u\n",r->begin,d->prepared_us,d->handoff_us,d->adopt_entry_last_us);
 console_printf("ecam-failure-time: last_ok=%u sample=%u rejected_clock=%u elapsed_valid=%u elapsed=%u display=%u\n",r->end,d->sample_us,(uint64_t)d->rejected_clock,(uint64_t)elapsed,elapsed?d->sample_us-r->begin:0,display);
 console_printf("ecam-failure-detail: flags=%u table=%x index=%u component=%u\n",(uint64_t)d->details,d->table,(uint64_t)d->index,(uint64_t)d->component);
 console_printf("ecam-failure-detail: expected=%x observed=%x words_at_failure=%u\n",d->expected,d->observed,(uint64_t)d->words);
 console_printf("ecam-gate-failure: stage=%s flags=%u index=%u component=%u\n",g->stage?g->stage:"none",(uint64_t)g->details,(uint64_t)g->index,(uint64_t)g->component);
 console_printf("ecam-gate-failure: expected=%x observed=%x\n",g->expected,g->observed);
 console_printf("ecam-failure-progress: invalidations=%u metadata=%u firmware_bytes=%u phases=%u\n",(uint64_t)r->invalidations,(uint64_t)r->metadata_attempts,(uint64_t)gate->firmware_bytes,(uint64_t)gate->phases);
 console_write("Detail flags: 1=expected/observed pair, 2=table/index, 4=observed only. Clock sample is latched; display time is later.\n");
}
_Noreturn void haswell_ecam_boot_fail_closed(const char *reason) {
 console_set_framebuffer_muted(false);console_clear();
 const struct ecam_boot_result *r=vmm_ecam_boot_result();
 console_printf("ECAM BOOT HALTED: %s\nstate=%u root=%x frame=%x words=%u parent_attempts=%u\n",reason,(uint64_t)r->state,r->root,r->frame,(uint64_t)r->words,(uint64_t)r->parent_attempts);
 haswell_ecam_boot_failure_report();
 console_write("No PCI/USB continuation or table reuse. Photograph this screen, then restart into Ubuntu.\n");console_sync();
 for(;;)halt_forever();
}
