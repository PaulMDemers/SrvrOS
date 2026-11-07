/* Window-only retained raw proof. Reporting performs no hardware operations. */
#include <srvros/air61_window.h>
#include <srvros/ecam_boot.h>
#include <srvros/console.h>
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
 out[at]=0;console_printf("hsw-window-data: v=1 chunk=%u data=%s\n",(uint64_t)chunks++,out);used=0;
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
bool haswell_air61_window_report(const char *directory){
 const char *s=session_name(directory);if(reported || !s)return false;
 reported=true;for(unsigned i=0;i<28;i++)reported_session[i]=s[i];
 const struct air61_window_result *r=vmm_air61_window_result();
 const struct air61_window_evidence *e=vmm_air61_window_evidence();
 const struct hac_result *c=vmm_air61_window_census();
 const struct ecam_boot_gate_result *g=haswell_air61_window_gate_result();
 bool ok=r->state==AIR61_WINDOW_RETIRED && !r->claimed && !r->fault && e->linked && e->retired && g->phases==2 && c->status==1;
 console_printf("hsw-window: v=1 state=%u complete=%u session=%s epoch=%u root=%x\n",(uint64_t)r->state,(uint64_t)ok,s,r->token.epoch,r->token.root);
 used=total=chunks=0;failed=false;
 if(ok){
  bytes((const uint8_t *)"W1AIR610",8);put(r->state,4);
  const uint64_t values[]={r->token.owner,r->token.epoch,r->token.root,r->pdpt,r->frames[0],r->frames[1],r->hhdm,r->begin,r->end};words(values,9);
  const unsigned counters[]={r->words,r->private_writes,r->parent_attempts,r->unlink_attempts,r->invalidations,r->frees};for(unsigned i=0;i<6;i++)put(counters[i],4);
  put(e->linked,1);put(e->retired,1);put(r->claimed,1);
  words(e->root_before,512);words(e->pdpt_before,512);
  delta(e->root_before,e->root_linked);delta(e->pdpt_before,e->pdpt_linked);
  delta(e->root_before,e->root_retired);delta(e->pdpt_before,e->pdpt_retired);
  static const uint64_t zero[512];delta(zero,e->pd);delta(zero,e->pt);
  const unsigned cc[]={c->status,c->pages,c->prepare_reads,c->prepare_pages,c->reads[0],c->reads[1],c->bytes[0],c->bytes[1],c->target_leaves};for(unsigned i=0;i<9;i++)put(cc[i],4);
  const uint64_t times[]={c->begin,c->prepare_end,c->end};words(times,3);
  bytes(c->data[0],c->bytes[0]);bytes(c->data[1],c->bytes[1]);gate(g);flush();
 }
 console_printf("hsw-window-end: v=1 bytes=%u chunks=%u report=%u ok=%u target_reads=0 target_writes=0\n",(uint64_t)total,(uint64_t)chunks,timer_monotonic_us(),(uint64_t)(ok&&!failed));
 console_printf("hsw-window-progress: begin=%u end=%u census=%u pages=%u prepare=%u reads=%u,%u bytes=%u,%u gate_phases=%u firmware_bytes=%u parent=%u unlink=%u inv=%u frees=%u\n",r->begin,r->end,(uint64_t)c->status,(uint64_t)c->pages,(uint64_t)c->prepare_reads,(uint64_t)c->reads[0],(uint64_t)c->reads[1],(uint64_t)c->bytes[0],(uint64_t)c->bytes[1],(uint64_t)g->phases,(uint64_t)g->firmware_bytes,(uint64_t)r->parent_attempts,(uint64_t)r->unlink_attempts,(uint64_t)r->invalidations,(uint64_t)r->frees);
 console_printf("hsw-window-failure: reason=%s gate=%s words=%u\n",r->fault?r->fault:"none",haswell_air61_window_gate_diagnostic()->stage?haswell_air61_window_gate_diagnostic()->stage:"none",(uint64_t)r->words);
 return ok&&!failed;
}
void haswell_air61_window_reference(const char *directory,bool saved){
 const char *s=session_name(directory);if(!s || !reported)return;
 for(unsigned i=0;i<28;i++)if(s[i]!=reported_session[i])return;
 const struct air61_window_result *r=vmm_air61_window_result();
 console_printf("hsw-window-ref: v=1 session=%s epoch=%u root=%x state=%u saved=%u target_reads=0 target_writes=0\n",s,r->token.epoch,r->token.root,(uint64_t)r->state,(uint64_t)saved);
}
