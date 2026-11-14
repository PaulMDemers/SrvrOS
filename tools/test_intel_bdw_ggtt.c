#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/intel_bdw_ggtt.h>

int main(void) {
 uint64_t original=0x123456788e201001ull;
 struct scanout_transfer_manifest manifest={
  .aperture_bytes=0x10000000,.fence_end=0x2200000,
  .first=8704,.pages=1,.obligations=255,.original=&original};
 struct intel_bdw_ggtt_page page;
 unsigned count=0;
 for(uint64_t physical=4096;physical<0x100000000ull;physical+=4096) {
  assert(intel_bdw_ggtt_page_plan(&manifest,physical,&page));
  assert(page.physical==physical && (page.replacement&~4095ull)==physical);
  assert((page.replacement&4095)==1 && page.original==original);
  assert(page.mmio_offset==0x811000 && page.gpu_address==0x2200000);
  ++count;
 }
 struct intel_bdw_ggtt_page saved=page;
 const uint64_t invalid[]={0,1,4095,4097,0xffffffff,0x100000000ull,UINT64_MAX};
 for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++) {
  assert(!intel_bdw_ggtt_page_plan(&manifest,invalid[i],&page));
  assert(!memcmp(&page,&saved,sizeof page));
 }
 assert(!intel_bdw_ggtt_page_plan(0,4096,&page));
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,0));
 manifest.original=0;
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 manifest.original=&original;
 ++manifest.fence_end;
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 --manifest.fence_end;
 assert(!memcmp(&page,&saved,sizeof page));
 manifest.first=8703;
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 manifest.first=(uint32_t)(manifest.aperture_bytes/4096-1);
 assert(intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 assert(page.gpu_address==manifest.aperture_bytes-4096);
 ++manifest.first;
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 manifest.aperture_bytes=0x100000000ull;manifest.first=(1u<<20)-1;
 assert(intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 assert(page.mmio_offset==0xfffff8 && page.gpu_address==0xfffff000);
 ++manifest.first;
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 manifest.aperture_bytes=0x100000001ull;
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 manifest.aperture_bytes=0;
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 manifest.aperture_bytes=0x10000000;
 manifest.fence_begin=manifest.fence_end;
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 manifest.fence_begin=0;
 manifest.first=8704;manifest.pages=2;
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 manifest.pages=1;original=UINT64_MAX;
 assert(!intel_bdw_ggtt_page_plan(&manifest,4096,&page));
 printf("bdw-ggtt: PASS %u low-address encodings, original high-word preservation, fence and aperture boundaries\n",count);
 return 0;
}
