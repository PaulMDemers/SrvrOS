/* Independent gate instance, same exact-unit predicates/pins as ECAM. */
#include <srvros/air61_window.h>
#include <srvros/console.h>
#include <srvros/halt.h>
#define SRVROS_AIR61_WINDOW_GATE
#define haswell_ecam_boot_gate_result haswell_air61_window_gate_result
#define haswell_ecam_boot_gate_diagnostic haswell_air61_window_gate_diagnostic
#include "haswell_ecam_boot.c"
#undef haswell_ecam_boot_gate_result
#undef haswell_ecam_boot_gate_diagnostic
const struct ecam_boot_gate_result *haswell_air61_window_gate_result(void){return &eg;}
#ifdef SRVROS_ECAM_GATE_HOST_TEST
extern uint64_t eg_test_irq_save(void);
extern void eg_test_irq_restore(uint64_t);
#else
static uint64_t eg_test_irq_save(void){uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;}
static void eg_test_irq_restore(uint64_t f){if(f&512)__asm__ volatile("sti":::"memory");}
#endif
bool haswell_air61_window_run(void){
 if(eg_started)return false;
 eg_started=true;
 const struct ecam_boot_result *boot=vmm_ecam_boot_result();
 if(boot->state!=ECAM_ACTIVE || boot->adoptions!=1)return false;
 uint64_t flags=eg_test_irq_save();eg_root=vmm_kernel_address_space();
 const struct air61_window_env io={.check=eg_check,.ram=eg_ram_page,.now=eg_now};
 struct air61_window_token token={0};
 bool ok=vmm_air61_window_prepare(&io,0x616972363177696eull,timer_monotonic_us(),&token);
 if(ok)ok=vmm_air61_window_retire(&token);
 const struct air61_window_result *r=vmm_air61_window_result();
 if(r->state==AIR61_WINDOW_POISONED){
  console_set_framebuffer_muted(false);console_clear();
  console_printf("WINDOW BOOT HALTED: %s\nstate=%u root=%x frames=%x,%x words=%u\n",r->fault?r->fault:"unknown",(uint64_t)r->state,r->token.root,r->frames[0],r->frames[1],(uint64_t)r->words);
  const struct hac_result *c=vmm_air61_window_census();
  console_printf("window-failure: begin=%u last=%u census_status=%u reads=%u,%u prepare=%u\n",r->begin,r->end,(uint64_t)c->status,(uint64_t)c->reads[0],(uint64_t)c->reads[1],(uint64_t)c->prepare_reads);
  console_printf("window-gate: stage=%s firmware_bytes=%u phases=%u\n",eg_diagnostic.stage?eg_diagnostic.stage:"none",(uint64_t)eg.firmware_bytes,(uint64_t)eg.phases);
  console_printf("window-retirement: parent_attempts=%u unlink_attempts=%u invalidations=%u frees=%u\n",(uint64_t)r->parent_attempts,(uint64_t)r->unlink_attempts,(uint64_t)r->invalidations,(uint64_t)r->frees);
  console_printf("window-cost: clocks=%u loads=%u guards=%u ram=%u reads=%u stores=%u census_reads=%u gate_end=%u census_begin=%u census_end=%u\n",(uint64_t)r->clock_samples,(uint64_t)r->raw_loads,(uint64_t)r->guard_checks,(uint64_t)r->ram_checks,(uint64_t)r->logical_reads,(uint64_t)r->logical_stores,(uint64_t)r->census_reads,r->gate_end,r->census_begin,r->census_end);
  console_printf("window-gate-detail: index=%u component=%u expected=%x observed=%x\n",(uint64_t)eg_diagnostic.index,(uint64_t)eg_diagnostic.component,eg_diagnostic.expected,eg_diagnostic.observed);
  console_write("No target loads. Claim/backing retained; no user init or USB reuse. Photograph then restart Ubuntu.\n");console_sync();
#ifdef SRVROS_ECAM_GATE_HOST_TEST
  extern void window_test_halt(void);window_test_halt();
#else
  for(;;)halt_forever();
#endif
  return false;
 }
 eg_test_irq_restore(flags);return ok;
}
