#include <srvros/haswell_alias_context.h>
static struct hac_result hac_result;
static unsigned hac_step;
static uint64_t hac_root_pa,hac_irq_before,hac_irq_after,hac_efer[2];
static uint32_t hac_max[2],hac_nx[2];
#if defined(SRVROS_HCC_HOST_TEST) && !defined(SRVROS_HAC_HOST_TEST)
static bool hac_run(void){return false;}
#else
#ifdef SRVROS_HAC_HOST_TEST
extern uint64_t hac_test_save(void);
extern void hac_test_restore(uint64_t);
extern bool hac_test_state(uint32_t *,uint32_t *,uint64_t *);
#define hac_save hac_test_save
#define hac_restore hac_test_restore
#define hac_state hac_test_state
#else
static uint64_t hac_save(void){uint64_t v;__asm__ volatile("pushfq; popq %0; cli":"=r"(v)::"memory");return v;}
static void hac_restore(uint64_t v){if(v&(1ull<<9))__asm__ volatile("sti":::"memory");}
static bool hac_state(uint32_t *max,uint32_t *nx,uint64_t *efer) {
 uint32_t b,c,d;hcc_cpuid(0x80000000,max,&b,&c,&d);if(*max<0x80000001)return false;
 hcc_cpuid(0x80000001,&b,&c,&d,nx);if(!(*nx&(1u<<20)))return false;
 uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000080u));*efer=lo|((uint64_t)hi<<32);
 return !(*efer&~0xd01ull) && (*efer&0xd00)==0xd00;
}
#endif
static bool hac_read(void *ctx,uint64_t pa,unsigned index,uint64_t *out) {
 (void)ctx;
 if(index>=512 || !hcc_ram_page(pa) || !vmm_diagnostic_sole_kernel_root(hac_root_pa))return false;
#ifdef SRVROS_HAC_HOST_TEST
 return hcc_test_entry(pa,index,out);
#else
 uint64_t address=pa+index*8,translated;
 volatile uint64_t *p=pmm_phys_to_virt(address);
 if(!p || !vmm_virt_to_phys_in_space(hac_root_pa,(uint64_t)(uintptr_t)p,&translated) || translated!=address)return false;
 *out=*p;return true;
#endif
}
static bool hac_run(void) {
 bool ok=false;hac_result.status=3;hac_step=1;
 console_write("hsw-alias-progress: two bounded RAM-only single-managed-root passes; NX/EFER read-only; no target access\n");
 hac_root_pa=hcc_root();hac_irq_before=hac_save();
 if(!vmm_diagnostic_sole_kernel_root(hac_root_pa))goto done;
 hac_step=2;if(!hac_state(&hac_max[0],&hac_nx[0],&hac_efer[0]))goto done;
 hac_step=3;
 const struct hac_io io={.read=hac_read,.now=now};
 if(!hac_capture(&hac_result,&io,hac_root_pa))goto done;
 for(unsigned i=0;i<hac_result.pages;i++)if(hac_result.table[i].level==1 && !(hac_nx[0]&(1u<<26)))
  for(unsigned j=0;j<512;j++)if((hac_result.table[i].word[j]&129)==129){hac_result.status=4;goto done;}
 hac_step=4;if(!vmm_diagnostic_sole_kernel_root(hac_root_pa)){hac_result.status=5;goto done;}
 hac_step=5;if(!hac_state(&hac_max[1],&hac_nx[1],&hac_efer[1]) || hac_max[0]!=hac_max[1] || hac_nx[0]!=hac_nx[1] || hac_efer[0]!=hac_efer[1]){hac_result.status=5;goto done;}
 ok=true;
 done: hac_restore(hac_irq_before);
#ifdef SRVROS_HAC_HOST_TEST
 hac_irq_after=hac_test_save();hac_test_restore(hac_irq_after);
#else
 __asm__ volatile("pushfq; popq %0":"=r"(hac_irq_after));
#endif
 if((hac_irq_before^hac_irq_after)&(1ull<<9)){hac_result.status=5;ok=false;}
 if(ok)hac_step=6;
 return ok;
}
#endif
static void hac_report(bool complete) {
 unsigned status=complete?1:((hac_result.status && hac_result.status!=1)?hac_result.status:3);
 console_printf("hsw-alias: v=1 status=%u step=%u pages=%u reads=%u,%u bytes=%u,%u target_leaves=%u root=%x nx=%x,%x max=%x,%x efer=%x,%x irq=%x,%x begin=%u end=%u table_cap=64 entry_cap=32768 data_cap=4096 limit_us=100000 scope=sole-managed-root writes=0 maps=0 ownership=0\n",
 (uint64_t)status,(uint64_t)hac_step,(uint64_t)hac_result.pages,(uint64_t)hac_result.reads[0],(uint64_t)hac_result.reads[1],(uint64_t)hac_result.bytes[0],(uint64_t)hac_result.bytes[1],(uint64_t)hac_result.target_leaves,hac_root_pa,(uint64_t)hac_nx[0],(uint64_t)hac_nx[1],(uint64_t)hac_max[0],(uint64_t)hac_max[1],hac_efer[0],hac_efer[1],hac_irq_before,hac_irq_after,hac_result.begin,hac_result.end);
 if(!complete)return;
 static char data[700];
 for(unsigned pass=0;pass<2;pass++)for(unsigned at=0,chunk=0;at<hac_result.bytes[pass];chunk++) {
  unsigned n=hac_result.bytes[pass]-at;if(n>512)n=512;
  b64(hac_result.data[pass]+at,n,data);console_printf("hsw-alias-data: v=1 phase=%u chunk=%u data=%s\n",(uint64_t)pass,(uint64_t)chunk,data);at+=n;
 }
}
