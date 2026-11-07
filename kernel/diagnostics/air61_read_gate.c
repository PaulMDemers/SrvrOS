/* Dormant native bridge. Existing gate predicates are instantiated afresh for
 * each pass; neither an old receipt nor the accepted no-read instance admits it. */
#include <srvros/air61_read_qualification.h>
#include <srvros/console.h>
#include <srvros/halt.h>
#include <srvros/timer.h>
static bool read_started;
static bool equal_bytes(const void *a,const void *b,unsigned n){const uint8_t *x=a,*y=b;for(unsigned i=0;i<n;i++)if(x[i]!=y[i])return false;return true;}
bool air61_read_gates_equal(void){
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
static bool read_check(void *ctx,unsigned pass,unsigned phase){
 (void)ctx;if(pass>=2 || phase>2)return false;
 bool ok=pass?air61_read_gate_check1(phase):air61_read_gate_check0(phase);
 return ok && (!pass || phase || air61_read_gates_equal());
}
static bool read_ram(void *ctx,unsigned pass,uint64_t pa){(void)ctx;return pass<2 && (pass?air61_read_gate_ram1(pa):air61_read_gate_ram0(pa));}
static uint64_t read_now(void *ctx){(void)ctx;return timer_monotonic_us();}
#ifdef SRVROS_ECAM_GATE_HOST_TEST
extern uint64_t eg_test_irq_save(void);
extern void eg_test_irq_restore(uint64_t);
#else
static uint64_t eg_test_irq_save(void){uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;}
static void eg_test_irq_restore(uint64_t f){if(f&512)__asm__ volatile("sti":::"memory");}
#endif
bool haswell_air61_read_qualify(void){
 if(read_started)return false;
 read_started=true;
 const struct ecam_boot_result *boot=vmm_ecam_boot_result();
 if(boot->state!=ECAM_ACTIVE || boot->adoptions!=1)return false;
 uint64_t flags=eg_test_irq_save();
 const struct air61_read_window_env io={.check=read_check,.ram=read_ram,.now=read_now};
 bool ok=vmm_air61_read_window_qualify(&io,0x61697272656164ull,timer_monotonic_us());
 const struct air61_read_series_result *r=vmm_air61_read_window_result();
 if(r->state==AIR61_READ_POISONED){
  console_set_framebuffer_muted(false);console_clear();
  console_printf("READ QUALIFICATION HALTED: %s\nroot=%x started=%u retired=%u\n",r->fault?r->fault:"unknown",r->root,(uint64_t)r->started,(uint64_t)r->retired);
  for(unsigned i=0;i<2;i++){const struct air61_read_pass_result *p=vmm_air61_read_window_pass(i);const struct air61_window_result *w=&p->window;
   console_printf("read-pass: pass=%u state=%u begin=%u end=%u words=%u slots=%u frames=%x,%x parent=%u unlink=%u frees=%u\n",(uint64_t)i,(uint64_t)w->state,w->begin,w->end,(uint64_t)w->words,(uint64_t)p->slots,w->frames[0],w->frames[1],(uint64_t)w->parent_attempts,(uint64_t)w->unlink_attempts,(uint64_t)w->frees);
   const struct ecam_boot_gate_result *g=i?air61_read_gate_result1():air61_read_gate_result0();
   const struct ecam_boot_gate_diagnostic *d=i?air61_read_gate_diagnostic1():air61_read_gate_diagnostic0();
   console_printf("read-gate: pass=%u phases=%u firmware_bytes=%u stage=%s index=%u component=%u expected=%x observed=%x\n",(uint64_t)i,(uint64_t)g->phases,(uint64_t)g->firmware_bytes,d->stage?d->stage:"none",(uint64_t)d->index,(uint64_t)d->component,d->expected,d->observed);
  }
  console_write("Zero target loads. Ownership retained; no clients or USB reuse. Photograph then restart Ubuntu.\n");console_sync();
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
void air61_read_bridge_test_reset(void){read_started=false;}
#endif
