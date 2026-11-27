/* Isolated native bridges. Existing gate predicates are instantiated afresh for
 * each pass; neither an old receipt nor the accepted no-read instance admits it. */
#include <srvros/air61_read_qualification.h>
#include <srvros/console.h>
#include <srvros/halt.h>
#include <srvros/timer.h>
static bool read_started;
static AIR61_SAMPLE_CODE bool equal_bytes(const void *a,const void *b,unsigned n){const uint8_t *x=a,*y=b;for(unsigned i=0;i<n;i++)if(x[i]!=y[i])return false;return true;}
AIR61_SAMPLE_CODE bool air61_read_gates_equal(void){
 const struct ecam_boot_gate_result *a=air61_read_gate_result0(),*b=air61_read_gate_result1();
 const struct hfw_sample *x=&a->firmware[0].sample[0],*y=&b->firmware[0].sample[0];
 if(!x->count || x->count>128 || !y->count || y->count>128)return false;
 if(!x86_cache_equal(&a->cache[0],&b->cache[0]) || a->efer[0]!=b->efer[0] || a->mcfg_pa[0]!=b->mcfg_pa[0] || x->count!=y->count || x->table_pa!=y->table_pa)return false;
 if(!equal_bytes(x->identity,y->identity,sizeof(x->identity)) || !equal_bytes(x->dmar,y->dmar,136) || !equal_bytes(a->mcfg[0],b->mcfg[0],60))return false;
 for(unsigned i=0;i<x->count;i++)if(x->entries[i].base!=y->entries[i].base || x->entries[i].length!=y->entries[i].length || x->entries[i].type!=y->entries[i].type)return false;
 const struct ecam_boot_policy *p=a->policy,*q=b->policy;
 if(p->extended_max!=q->extended_max || p->nx_edx!=q->nx_edx || p->apic!=q->apic || p->present_mask!=q->present_mask || p->launch_mask!=q->launch_mask || p->cr4!=q->cr4 || p->revision!=q->revision || p->flags!=q->flags || p->count!=q->count || p->bsp!=q->bsp || !equal_bytes(p->ids,q->ids,sizeof(p->ids)))return false;
 return true;
}
static AIR61_SAMPLE_CODE bool read_check(void *ctx,unsigned pass,unsigned phase){
 (void)ctx;if(pass>=2 || phase>2)return false;
 bool ok=pass?air61_read_gate_check1(phase):air61_read_gate_check0(phase);
 if(ok && !pass && !phase){
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
  ok=air61_read_gate_touch0();
#endif
  if(ok)ok=air61_read_gate_touch1();
 }
 return ok && (!pass || phase || air61_read_gates_equal());
}
static AIR61_SAMPLE_CODE bool read_ram(void *ctx,unsigned pass,uint64_t pa){(void)ctx;return pass<2 && (pass?air61_read_gate_ram1(pa):air61_read_gate_ram0(pa));}
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
static AIR61_SAMPLE_CODE bool read_sample_admit(void *ctx,unsigned pass,unsigned slot,struct air61_sample_admission *s){
 (void)ctx;return pass<2 && slot<AIR61_SAMPLE_ADMISSIONS && (pass?air61_sample_cache1(s):air61_sample_cache0(s));
}
#endif
static AIR61_SAMPLE_CODE uint64_t read_now(void *ctx){(void)ctx;return timer_monotonic_us();}
#ifdef SRVROS_ECAM_GATE_HOST_TEST
extern uint64_t eg_test_irq_save(void);
extern void eg_test_irq_restore(uint64_t);
#else
static AIR61_SAMPLE_CODE uint64_t eg_test_irq_save(void){uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;}
static AIR61_SAMPLE_CODE void eg_test_irq_restore(uint64_t f){if(f&512)__asm__ volatile("sti":::"memory");}
#endif

static void read_halt_timing(unsigned pass,const struct air61_read_pass_result *result){
 if(!result->timing.active)return;
 static const char *const names[AIR61_STEP_COUNT]={
  [AIR61_STEP_NONE]="none",[AIR61_STEP_GATE0]="gate0",[AIR61_STEP_OUTPUTS]="outputs",[AIR61_STEP_TEXT]="text",
  [AIR61_STEP_CENSUS]="census",[AIR61_STEP_BASELINE]="baseline",[AIR61_STEP_FRAMES]="frames",
  [AIR61_STEP_PRIVATE_VERIFY]="private-verify",[AIR61_STEP_GAP]="gap",[AIR61_STEP_GATE1]="gate1",
  [AIR61_STEP_FINAL_VERIFY]="final-verify",[AIR61_STEP_LINK]="link",[AIR61_STEP_SLOTS]="slots",
  [AIR61_STEP_FINAL_CACHE]="final-cache",[AIR61_STEP_RETIRE]="retire",[AIR61_STEP_CLEANUP]="cleanup",
 };
 const struct air61_read_timing *timing=&result->timing;
 const uint64_t *s=timing->start;
 console_printf("read-time: pass=%u failed=%s fault_at_us=%u rejected_us=%u last_step=%s clocks=%u\n",
  (uint64_t)pass,names[timing->failed],timing->fault_at,timing->rejected_clock,
  names[timing->active],(uint64_t)result->window.clock_samples);
 console_printf("read-start-us: gate0=%u outputs=%u text=%u census=%u baseline=%u frames=%u\n",
  s[AIR61_STEP_GATE0],s[AIR61_STEP_OUTPUTS],s[AIR61_STEP_TEXT],s[AIR61_STEP_CENSUS],s[AIR61_STEP_BASELINE],s[AIR61_STEP_FRAMES]);
 console_printf("read-start-us: private_verify=%u gap=%u gate1=%u final_verify=%u link=%u\n",
  s[AIR61_STEP_PRIVATE_VERIFY],s[AIR61_STEP_GAP],s[AIR61_STEP_GATE1],s[AIR61_STEP_FINAL_VERIFY],s[AIR61_STEP_LINK]);
 console_printf("read-start-us: slots=%u final_cache=%u retire=%u cleanup=%u\n",
  s[AIR61_STEP_SLOTS],s[AIR61_STEP_FINAL_CACHE],s[AIR61_STEP_RETIRE],s[AIR61_STEP_CLEANUP]);
 console_printf("read-census: end_us=%u status=%u pages=%u prepare=%u reads=%u,%u bytes=%u,%u\n",
  result->window.census_end,(uint64_t)result->census.status,(uint64_t)result->census.pages,
  (uint64_t)result->census.prepare_reads,(uint64_t)result->census.reads[0],(uint64_t)result->census.reads[1],
  (uint64_t)result->census.bytes[0],(uint64_t)result->census.bytes[1]);
}

#ifdef SRVROS_AIR61_REGISTER_SAMPLE
AIR61_SAMPLE_CODE bool haswell_air61_register_sample(void){
#else
AIR61_SAMPLE_CODE bool haswell_air61_read_qualify(void){
#endif
 if(read_started)return false;
 read_started=true;
 const struct ecam_boot_result *boot=vmm_ecam_boot_result();
 if(boot->state!=ECAM_ACTIVE || boot->adoptions!=1)return false;
 uint64_t flags=eg_test_irq_save();
 const struct air61_read_window_env io={.check=read_check,.ram=read_ram,.now=read_now,
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
 .sample_admit=read_sample_admit,
#endif
 };
 #ifdef SRVROS_AIR61_REGISTER_SAMPLE
 bool ok=vmm_air61_register_sample(&io,0x61697273616d70ull,timer_monotonic_us());
#else
 bool ok=vmm_air61_read_window_qualify(&io,0x61697272656164ull,timer_monotonic_us());
#endif
 const struct air61_read_series_result *r=vmm_air61_read_window_result();
 if(r->state==AIR61_READ_POISONED){
  console_set_framebuffer_muted(false);console_clear();
  console_printf("READ QUALIFICATION HALTED: %s\nroot=%x started=%u retired=%u\n",r->fault?r->fault:"unknown",r->root,(uint64_t)r->started,(uint64_t)r->retired);
  for(unsigned i=0;i<2;i++){const struct air61_read_pass_result *p=vmm_air61_read_window_pass(i);const struct air61_window_result *w=&p->window;
   console_printf("read-pass: pass=%u state=%u begin=%u end=%u words=%u slots=%u frames=%x,%x parent=%u unlink=%u frees=%u\n",(uint64_t)i,(uint64_t)w->state,w->begin,w->end,(uint64_t)w->words,(uint64_t)p->slots,w->frames[0],w->frames[1],(uint64_t)w->parent_attempts,(uint64_t)w->unlink_attempts,(uint64_t)w->frees);
   const struct ecam_boot_gate_result *g=i?air61_read_gate_result1():air61_read_gate_result0();
   const struct ecam_boot_gate_diagnostic *d=i?air61_read_gate_diagnostic1():air61_read_gate_diagnostic0();
   console_printf("read-gate: pass=%u phases=%u firmware_bytes=%u stage=%s index=%u component=%u expected=%x observed=%x\n",(uint64_t)i,(uint64_t)g->phases,(uint64_t)g->firmware_bytes,d->stage?d->stage:"none",(uint64_t)d->index,(uint64_t)d->component,d->expected,d->observed);
   read_halt_timing(i,p);
  }
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
  for(unsigned p=0;p<2;p++){const struct air61_read_pass_result *v=vmm_air61_read_window_pass(p);
   console_printf("sample-count: pass=%u attempts=%u reads=%u\n",(uint64_t)p,(uint64_t)v->attempts,(uint64_t)v->reads);
   for(unsigned i=0;i<v->attempts && i<12;i++)console_printf("sample-raw: pass=%u slot=%u offset=%x value=%x begin=%u end=%u\n",(uint64_t)p,(uint64_t)i,(uint64_t)v->offsets[i],(uint64_t)v->values[i],v->load_begin[i],v->load_end[i]);
  }
  console_write("Read-only sample stopped. Ownership retained; no clients or USB reuse. Photograph then restart Ubuntu.\n");console_sync();
#else
  console_write("Zero target loads. Ownership retained; no clients or USB reuse. Photograph then restart Ubuntu.\n");console_sync();
#endif
#ifdef SRVROS_ECAM_GATE_HOST_TEST
  extern void window_test_halt(void);window_test_halt();
#else
  for(;;)halt_forever();
#endif
  return false;
 }
 eg_test_irq_restore(flags);return ok;
}
#ifdef SRVROS_ECAM_GATE_HOST_TEST
AIR61_SAMPLE_CODE void air61_read_bridge_test_reset(void){read_started=false;}
#endif
