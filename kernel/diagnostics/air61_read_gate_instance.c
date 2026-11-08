/* Independent fresh gate for one lifetime. Never reset in native builds. */
#include <srvros/air61_read_window.h>
#include <srvros/ecam_boot.h>
#define J0(a,b) a##b
#define J(a,b) J0(a,b)
#define FN(a) J(a,AIR61_READ_GATE_INSTANCE)
#define SRVROS_AIR61_WINDOW_GATE
#define haswell_ecam_boot_gate_result FN(air61_read_gate_result)
#define haswell_ecam_boot_gate_diagnostic FN(air61_read_gate_diagnostic)
#define vmm_air61_window_copy_ram(pa,out,len) vmm_air61_read_window_copy_ram(AIR61_READ_GATE_INSTANCE,pa,out,len)
#include "haswell_ecam_boot.c"
#undef vmm_air61_window_copy_ram
#undef haswell_ecam_boot_gate_result
#undef haswell_ecam_boot_gate_diagnostic
AIR61_SAMPLE_CODE const struct ecam_boot_gate_result *FN(air61_read_gate_result)(void){return &eg;}
AIR61_SAMPLE_CODE bool FN(air61_read_gate_check)(unsigned phase){
#ifdef SRVROS_ECAM_GATE_HOST_TEST
 extern void air61_read_test_phase(unsigned,unsigned);air61_read_test_phase(AIR61_READ_GATE_INSTANCE,phase);
#endif
 if(!phase){if(eg_started)return false;eg_started=true;eg_root=vmm_kernel_address_space();}
 return eg_check(NULL,phase);
}
AIR61_SAMPLE_CODE uint64_t FN(air61_read_gate_ram_steps)(void){return eg_ram_steps;}
AIR61_SAMPLE_CODE bool FN(air61_read_gate_ram)(uint64_t pa){return eg_ram_page(NULL,pa);}
static AIR61_SAMPLE_CODE bool touch_private(void *base,unsigned length){
 volatile uint8_t *p=base;
 for(unsigned at=0;at<length;at+=4096){if(!vmm_air61_read_window_prime_room())return false;p[at]=p[at];
#if defined(SRVROS_ECAM_GATE_HOST_TEST) && defined(SRVROS_AIR61_REGISTER_SAMPLE)
 extern void air61_sample_test_gate_touch(unsigned,const void *);air61_sample_test_gate_touch(AIR61_READ_GATE_INSTANCE,(const void *)(p+at));
#endif
 }
 if(!vmm_air61_read_window_prime_room())return false;
 p[length-1]=p[length-1];
#if defined(SRVROS_ECAM_GATE_HOST_TEST) && defined(SRVROS_AIR61_REGISTER_SAMPLE)
 extern void air61_sample_test_gate_touch(unsigned,const void *);air61_sample_test_gate_touch(AIR61_READ_GATE_INSTANCE,(const void *)(p+length-1));
#endif
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
 return vmm_air61_read_window_prime_room();
#else
 return true;
#endif
}
AIR61_SAMPLE_CODE bool FN(air61_read_gate_touch)(void){
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
 bool ready=AIR61_READ_GATE_INSTANCE==0?(eg_started && !eg_broken && eg.phases==1):!eg_started;
#else
 bool ready=!eg_started;
#endif
 return ready && touch_private(&eg,sizeof(eg)) && touch_private(&eg_diagnostic,sizeof(eg_diagnostic)) &&
  touch_private(eg_ram,sizeof(eg_ram)) && touch_private(&eg_count,sizeof(eg_count)) && touch_private(&eg_phase,sizeof(eg_phase)) &&
  touch_private(eg_ram_index,sizeof(eg_ram_index)) && touch_private(&eg_ram_index_count,sizeof(eg_ram_index_count)) &&
  touch_private(&eg_ram_index_ready,sizeof(eg_ram_index_ready)) && touch_private(&eg_ram_steps,sizeof(eg_ram_steps)) &&
  touch_private(&eg_started,sizeof(eg_started)) && touch_private(&eg_broken,sizeof(eg_broken)) && touch_private(&eg_root,sizeof(eg_root)) &&
  touch_private(&eg_last_policy,sizeof(eg_last_policy)) && touch_private(eg_policy_ready,sizeof(eg_policy_ready));
}
#ifdef SRVROS_ECAM_GATE_HOST_TEST
#include <string.h>
AIR61_SAMPLE_CODE void FN(air61_read_gate_reset)(void){
 memset(&eg,0,sizeof(eg));memset(&eg_diagnostic,0,sizeof(eg_diagnostic));
 memset(eg_policy_ready,0,sizeof(eg_policy_ready));eg_count=eg_phase=0;
 eg_started=eg_broken=false;eg_root=0;eg_ram_index_ready=false;eg_ram_index_count=0;eg_ram_steps=0;
}

#endif

#ifdef SRVROS_AIR61_REGISTER_SAMPLE
/* Fixed whitelist and original per-snapshot49 read cap. These counters/output
 * live in the already-primed current pass, not mutable gate-global storage. */
static AIR61_SAMPLE_CODE bool sample_msr(void *ctx,uint32_t n,uint64_t *v){
 struct air61_sample_admission *s=ctx;
 bool efer=n==0xc0000080u;
 if(!s || !s->attempted || (!efer && n!=0xfe && n!=0x277 && n!=0x2ff && !(n>=0x200 && n<=0x21f) && n!=0x250 && n!=0x258 && n!=0x259 && !(n>=0x268 && n<=0x26f)))return false;
 if(efer){if(s->efer_reads>=1)return false;s->efer_reads++;}
 else {if(s->msr_reads>=49)return false;s->msr_reads++;}
#ifdef SRVROS_ECAM_GATE_HOST_TEST
 return eg_test_msr(n,v);
#else
 uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));*v=a|((uint64_t)d<<32);return true;
#endif
}
AIR61_SAMPLE_CODE bool FN(air61_sample_cache)(struct air61_sample_admission *s){
 if(!s || s->attempted || !eg_started || eg_broken || eg.phases!=2 || !vmm_diagnostic_sole_kernel_root(eg_root))return false;
 s->attempted=true;
 const struct x86_cache_io io={.ctx=s,.cpu=eg_cpu,.msr=sample_msr,.now=eg_now};
 if(!x86_cache_snapshot(&s->cache,&io) || !sample_msr(s,0xc0000080,&s->efer) || s->msr_reads!=s->cache.reads || s->efer_reads!=1 || s->efer!=eg.efer[1] || !x86_cache_equal(&s->cache,&eg.cache[1]) || !vmm_diagnostic_sole_kernel_root(eg_root))return false;
 return true;
}
#endif
