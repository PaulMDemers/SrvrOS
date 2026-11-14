/* Optional comparative benchmark of the actual VMM with the normal host stubs.
 * Timings are not test assertions and cannot accept a native GPU lifetime. */
#define _POSIX_C_SOURCE 200809L
#ifdef NDEBUG
#error "VMM benchmark requires assertions enabled (-UNDEBUG)"
#endif
#include <time.h>
#define main previous_vmm_test_main
#include "test_vmm_user_range.c"
#undef main
static uint64_t *bench_tables[6];
static const uint64_t bench_va=0xffff800000000000ull,bench_pa=0x2000000;
static uint64_t nanos(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
static void layout(bool large){
 for(unsigned i=0;i<6;i++)memset(bench_tables[i],0,4096);
 kernel_pml4=bench_tables[0];kernel_cr3=test_cr3=(uint64_t)bench_tables[0];
 test_rflags=2;address_space_count=address_space_occupancy=0;
 bench_tables[0][256]=(uint64_t)bench_tables[1]|3;
 bench_tables[1][0]=(uint64_t)bench_tables[2]|3;
 for(unsigned n=0;n<3;n++){
  bench_tables[2][n]=large?(bench_pa+n*0x200000)|0x83:(uint64_t)bench_tables[3+n]|3;
  for(unsigned i=0;i<512;i++)bench_tables[3+n][i]=(bench_pa+(n*512+i)*4096)|3;
 }
}
int main(int argc,char **argv){
 assert(argc==3);const char *kind=argv[1];unsigned iterations=(unsigned)strtoul(argv[2],NULL,10);
 assert(iterations && iterations<=1000000);
 bool alias=!strcmp(kind,"alias-4k") || !strcmp(kind,"alias-large");
 bool large=!strcmp(kind,"translation-large") || !strcmp(kind,"alias-large");
 assert(alias || !strcmp(kind,"translation-4k") || !strcmp(kind,"translation-large"));
 for(unsigned i=0;i<6;i++){bench_tables[i]=aligned_alloc(4096,4096);assert(bench_tables[i]);}
 layout(large);unsigned before=table_reads;uint64_t checksum=0,begin=nanos();
 for(unsigned n=0;n<iterations;n++){
  struct vmm_range_observation d={0};
  if(alias){struct vmm_alias_observation a={0};assert(vmm_diagnostic_kernel_alias(kernel_cr3,bench_va,bench_pa,1356,52,&a,&d));assert(a.pages==1356 && a.leaves==(large?3u:1356u) && !a.pat_index && a.writable==1 && !a.user && !a.no_execute && a.leaf_sizes==(large?2u:1u));checksum+=a.leaves;}
  else assert(vmm_diagnostic_contiguous_kernel_range_observe(kernel_cr3,bench_va,bench_pa,1356,&d));
  assert(d.phase==4 && d.checked==1356 && !d.reason);checksum+=d.checked;
 }
 uint64_t elapsed=nanos()-begin;unsigned conversions=table_reads-before;
 assert(conversions==iterations*(large?8u:11u));assert(!switches && !retire_count && test_rflags==2 && test_cr3==kernel_cr3);
 printf("{\"kind\":\"%s\",\"iterations\":%u,\"ns\":%llu,\"conversions\":%u,\"checksum\":%llu}\n",kind,iterations,(unsigned long long)elapsed,conversions,(unsigned long long)checksum);
 for(unsigned i=0;i<6;i++)free(bench_tables[i]);
}
