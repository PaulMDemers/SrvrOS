#define _POSIX_C_SOURCE 200809L
#include <srvros/x86_cache.h>
#include <srvros/vmm.h>
static bool intel_display_test_source_audit_alias(uint64_t,uint64_t,uint64_t,uint32_t,unsigned,struct vmm_alias_observation *);
static bool intel_display_test_source_audit_clock(uint64_t *);
static bool intel_display_test_source_audit_pci(unsigned,uint32_t *);
static bool intel_display_test_source_audit_plane(unsigned,unsigned,uint32_t *);
static bool intel_display_test_source_audit_cache(struct x86_cache_snapshot *);
static bool intel_display_test_source_audit_inventory(uint64_t,const struct vmm_alias_span *,unsigned,const struct vmm_alias_ram *,unsigned,struct vmm_alias_inventory *,struct vmm_alias_detail *);
static uint64_t intel_display_test_private_clean_now(void);
static bool intel_display_test_private_clean_line(uint64_t);
static bool intel_display_test_private_clean_fence(void);
static void intel_display_test_private_clean_cpu(uint32_t *,uint32_t *,uint32_t *);
static bool intel_display_test_private_clean_cache(struct x86_cache_snapshot *);
static bool intel_display_test_private_clean_alias(uint64_t,uint64_t,uint64_t,uint32_t,unsigned,struct vmm_alias_observation *);
static bool intel_display_test_private_clean_inventory(uint64_t,const struct vmm_alias_span *,unsigned,const struct vmm_alias_ram *,unsigned,struct vmm_alias_inventory *,struct vmm_alias_detail *);
static bool intel_display_test_private_clean_page(uint64_t,uint64_t,uint64_t,unsigned,struct vmm_alias_observation *);
#define INTEL_ENGINES_FIXTURE_MAIN engines_adapter_main
#include "test_intel_source_engines_adapter.c"
static unsigned clean_clocks,clean_lines,clean_fences,clean_cpus,clean_contexts,clean_pages;
static uint64_t intel_display_test_private_clean_now(void) {
 ++clean_clocks;
 if(jf("clean-zero",clean_clocks))return 0;
 if(jf("clean-backward",clean_clocks))return 1;
 if(jf("clean-time",clean_clocks))ticks+=100001;
 if(jf("clean-hold",clean_clocks))buffers_native.hold[0].lifetime=0;
 if(jf("clean-result",clean_clocks))source_clean.span[0].pages--;
 return ++ticks;
}
static bool intel_display_test_private_clean_line(uint64_t va) {
 assert(wake_irq && source_clean_hold(&source_clean));
 if(!clean_lines){struct scanout_memory_cpu borrow;assert(!scanout_memory_cpu_begin(&buffers_native.memory[0],&buffers_native.token[0],&borrow));assert(!scanout_memory_hold_end(&buffers_native.hold[0]));}
 unsigned b=clean_lines/(1356*64),n=clean_lines%(1356*64);assert(b<2 && va==buffers_native.memory[b].physical+n*64ull);
 assert(clean_pages==clean_lines/64+1);
 ++clean_lines;return !jf("clean-line",clean_lines);
}
static bool intel_display_test_private_clean_fence(void) {
 assert(wake_irq);++clean_fences;
 if(jf("clean-fence",clean_fences) || (clean_fences>=2 && jf("clean-unknown",1)))return false;
 if(!source_clean.reason)assert(clean_lines==((clean_fences==1)?0:(clean_fences<4)?1356*64:2712*64));
 return true;
}
static void intel_display_test_private_clean_cpu(uint32_t *a,uint32_t *b,uint32_t *d) {
 ++clean_cpus;*a=0x306d4;*b=8u<<8;*d=0x91000;
 if(jf("clean-feature",clean_cpus))*d=0;
 if(jf("clean-cpu",clean_cpus))*a^=1;
 if(jf("clean-line-size",clean_cpus))*b=16u<<8;
}
static bool intel_display_test_private_clean_cache(struct x86_cache_snapshot *out) {
 ++clean_contexts;
 *out=(struct x86_cache_snapshot){.cpu={{0x756e6547,0x49656e69,0x6c65746e},0x306d4,0x91000,39,0x80050033},
 .pat=0x0007040600070406ull,.cap=0x402,.def_type=0x806,.base={0xb0000001,0},.mask={0x7ff0000800ull,0},
 .status=CACHE_OBSERVED,.reads=10,.count=2,.begin_us=ticks,.end_us=ticks+1};ticks++;
 if(jf("clean-cache",clean_contexts))return false;
 if(jf("clean-cache-drift",clean_contexts))out->pat^=1;
 if(jf("clean-cache-hole",clean_contexts)){out->base[1]=buffers_native.memory[0].physical+4096;out->mask[1]=(((1ull<<39)-1)&~4095ull)|0x800;}
 return true;
}
static bool intel_display_test_private_clean_alias(uint64_t root,uint64_t va,uint64_t pa,uint32_t pages,unsigned bits,struct vmm_alias_observation *out) {
 assert(root==0x1000 && va==pa && pages==1356 && bits==39 && source_clean_hold(&source_clean));
 *out=(struct vmm_alias_observation){.pages=pages,.leaves=4,.writable=1,.leaf_sizes=2};
 if(jf("clean-alias",1))out->user=1;
 if(jf("clean-alias-readonly",1))out->writable=0;
 if(jf("clean-alias-pat",1))out->pat_index=3;
 return true;
}
static bool intel_display_test_private_clean_inventory(uint64_t root,const struct vmm_alias_span *s,unsigned bits,const struct vmm_alias_ram *ram,unsigned nr,struct vmm_alias_inventory *out,struct vmm_alias_detail *d) {
 (void)d;assert(root==0x1000 && bits==39 && nr && ram && source_clean_hold(&source_clean));
 for(unsigned i=0;i<2;i++)assert(s[i].physical==buffers_native.memory[i].physical && s[i].pages==1356);
 *out=(struct vmm_alias_inventory){.tables=9,.entries=4608,.leaves=3072,.matches={4,4},.pages={1356,1356}};return !jf("clean-inventory",1);
}
static bool intel_display_test_private_clean_page(uint64_t root,uint64_t va,uint64_t pa,unsigned bits,struct vmm_alias_observation *a) {
 assert(wake_irq && root==source_clean_root && va==pa && bits==39 && source_clean_hold(&source_clean));
 ++clean_pages;assert(clean_lines==(clean_pages-1)*64);
 *a=(struct vmm_alias_observation){.pages=1,.leaves=1,.writable=1,.leaf_sizes=1};
 if(jf("clean-page",clean_pages))return false;
 if(jf("clean-page-readonly",clean_pages))a->writable=0;
 if(jf("clean-page-pat",clean_pages))a->pat_index=3;
 return true;
}
static unsigned audit_clocks,audit_pci,audit_planes,audit_caches,audit_aliases;
static bool intel_display_test_source_audit_clock(uint64_t *at) {
 ++audit_clocks;struct intel_source_audit_native *a=&source_audit_native;
 if(jf("audit-zero",audit_clocks)){*at=0;return true;}
 if(jf("audit-backward",audit_clocks)){*at=1;return true;}
 if(jf("audit-time",audit_clocks))ticks=a->origin+INTEL_DIAGNOSTIC_PREPARE_US+1;
 if(jf("audit-parent-time",audit_clocks))ticks=a->parent_begin+300001;
 if(jf("audit-source",audit_clocks))a->parent=0;
 if(jf("audit-provider",audit_clocks))a->provider=0;
 if(jf("audit-epoch",audit_clocks))a->epoch++;
 if(jf("audit-kind",audit_clocks))a->kind=3-a->kind;
 if(jf("audit-mode",audit_clocks))a->mode^=3;
 if(jf("audit-origin",audit_clocks))a->origin++;
 if(jf("audit-parent-origin",audit_clocks))a->parent_begin++;
 if(jf("audit-count",audit_clocks))a->pci_reads++;
 if(jf("audit-range-count",audit_clocks))a->range_checks++;
 if(jf("audit-alias-count",audit_clocks))a->alias_checks++;
 if(jf("audit-samples",audit_clocks))a->samples++;
 if(jf("audit-state",audit_clocks))a->state=2;
 if(jf("audit-registration",audit_clocks))a->registered=false;
 if(jf("audit-root",audit_clocks))pa[INTEL_CAPTURE_PAGES+14]^=4096;
 if(jf("audit-parent-registration",audit_clocks))source_requester_native.result.registered_child=0;
 if(jf("audit-io",audit_clocks))source_engines_native.result.io.ctx=0;
 return false;
}
static bool intel_display_test_source_audit_pci(unsigned i,uint32_t *out) {
 assert(source_audit_live() && wake_irq && requester_command==3 && engines_actual==5 && i<9);
 for(unsigned e=0;e<5;e++)assert(engines_control[e]==3);
 ++audit_pci;if(jf("audit-pci-fail",audit_pci))return false;
 *out=pci_read_config32(&devices[0],(uint8_t)intel_pci_requester_offsets[i]);
 if(jf("audit-pci-float",audit_pci))*out=UINT32_MAX;
 if(jf("audit-pci-drift",audit_pci))*out^=1;
 return true;
}
static bool intel_display_test_source_audit_plane(unsigned p,unsigned i,uint32_t *out) {
 assert(source_audit_live() && requester_command==3 && p<3 && i<9 && engines_actual==5);
 ++audit_planes;if(jf("audit-plane-fail",audit_planes))return false;
 *out=nonzero_native.r.readers.planes[p][i];
 if(jf("audit-plane-float",audit_planes))*out=UINT32_MAX;
 if(jf("audit-plane-drift",audit_planes))*out^=1;
 return true;
}
static bool intel_display_test_source_audit_cache(struct x86_cache_snapshot *out) {
 assert(source_audit_live() && requester_command==3 && engines_actual==5);
 ++audit_caches;uint64_t begin=ticks;ticks+=2;
 *out=(struct x86_cache_snapshot){.cpu={{0x756e6547,0x49656e69,0x6c65746e},0x306d4,0x91000,39,0x80050033},
 .pat=0x0007040600070406ull,.cap=0x402,.def_type=0x806,.base={0xb0000001,0},.mask={0x7ff0000800ull,0},
 .status=CACHE_OBSERVED,.reads=10,.count=2,.fixed_count=0,.begin_us=begin,.end_us=ticks};
 if(jf("audit-cache-fail",audit_caches))return false;
 if(jf("audit-cache-time",audit_caches))out->end_us=source_audit_native.origin+INTEL_DIAGNOSTIC_PREPARE_US+1;
 if(jf("audit-cache-before",audit_caches))out->begin_us=1;
 if(jf("audit-cache-zero",audit_caches))out->reads=0;
 if(jf("audit-cache-hole",audit_caches)) {
  out->base[1]=buffers_native.binding.physical[0]+4096;
  out->mask[1]=(((1ull<<39)-1)&~4095ull)|0x800;
 }
 if(jf("audit-cache-pat",audit_caches))out->pat=(out->pat&~255ull)|7;
 return true;
}
static bool intel_display_test_source_audit_alias(uint64_t root,uint64_t va,uint64_t physical,uint32_t pages,unsigned bits,struct vmm_alias_observation *out) {
 assert(source_audit_live() && wake_irq && requester_command==3 && engines_actual==5);
 assert(root==0x1000 && va==physical && pages==1356 && bits==39);
 ++audit_aliases;
 if(jf("audit-alias-fail",audit_aliases))return false;
 *out=(struct vmm_alias_observation){.pages=pages,.leaves=pages,.writable=1,.no_execute=1,.leaf_sizes=1};
 if(jf("audit-alias-user",audit_aliases))out->user=1;
 if(jf("audit-alias-readonly",audit_aliases))out->writable=0;
 if(jf("audit-alias-pages",audit_aliases))out->pages--;
 if(jf("audit-alias-empty",audit_aliases))out->leaves=0;
 if(jf("audit-alias-overflow",audit_aliases))out->leaves=pages+1;
 if(jf("audit-alias-size",audit_aliases))out->leaf_sizes=8;
 if(jf("audit-alias-nx",audit_aliases))out->no_execute=2;
 if(jf("audit-alias-cache",audit_aliases))out->pat_index=3;
 if(jf("audit-alias-index",audit_aliases))out->pat_index=8;
 if(jf("audit-alias-time",audit_aliases))ticks=source_audit_native.origin+INTEL_DIAGNOSTIC_PREPARE_US+1;
 return true;
}
static unsigned audit_inventories;
static bool intel_display_test_source_audit_inventory(uint64_t root,const struct vmm_alias_span *s,unsigned bits,const struct vmm_alias_ram *ram,unsigned nr,struct vmm_alias_inventory *out,struct vmm_alias_detail *d) {
 assert(source_audit_live() && wake_irq && requester_command==3 && engines_actual==5 && root==0x1000 && bits==39 && ram && nr);
 for(unsigned b=0;b<2;b++)assert(s[b].pages==1356 && s[b].physical==buffers_native.binding.physical[b] && s[b].virtual_base==s[b].physical);
 ++audit_inventories;
 if(jf("audit-inventory-fail",audit_inventories)){d->reason=VMM_ALIAS_FOREIGN;return false;}
 *out=(struct vmm_alias_inventory){.tables=9,.entries=4608,.leaves=3072,.matches={1356,1356},.pages={1356,1356}};
 if(jf("audit-inventory-tables",audit_inventories))out->tables=257;
 if(jf("audit-inventory-entries",audit_inventories))out->entries--;
 if(jf("audit-inventory-leaves",audit_inventories))out->leaves=4609;
 if(jf("audit-inventory-matches",audit_inventories))out->matches[1]--;
 if(jf("audit-inventory-pages",audit_inventories))out->pages[1]--;
 if(jf("audit-inventory-time",audit_inventories))ticks=source_audit_native.origin+INTEL_DIAGNOSTIC_PREPARE_US+1;
 return true;
}
static int audit_case_main(int argc,char **argv) {
 if(argc==2 && !strcmp(argv[1],"--inventory-ram")) {
  reset();assert(source_audit_ram_prepare() && source_audit_ram.count==1 && !source_audit_native.inventory_reason);
  struct limine_memmap_entry regions[3]={{.base=0x300000,.length=4096,.type=LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE},{.base=0x100000,.length=4096,.type=LIMINE_MEMMAP_USABLE},{.base=0x200000,.length=4096,.type=LIMINE_MEMMAP_FRAMEBUFFER}};
  struct limine_memmap_entry *ptrs[3]={&regions[0],&regions[1],&regions[2]};struct limine_memmap_entry saved[3];memcpy(saved,regions,sizeof(saved));
  struct limine_memmap_response m={.entries=ptrs,.entry_count=3};memmap_request.response=&m;
  assert(source_audit_ram_prepare() && source_audit_ram.count==2 && source_audit_ram.ranges[0].base==0x100000 && source_audit_ram.ranges[1].base==0x300000 && !memcmp(saved,regions,sizeof(saved)));
  regions[0].base=0x100000;assert(!source_audit_ram_prepare() && source_audit_native.inventory_reason==VMM_ALIAS_RAM);
  regions[0].base=UINT64_MAX-1;assert(!source_audit_ram_prepare());regions[0].base=0x300000;regions[0].length=0;assert(!source_audit_ram_prepare());
  m.entry_count=0;assert(!source_audit_ram_prepare());m.entry_count=4097;assert(!source_audit_ram_prepare());memmap_request.response=0;assert(!source_audit_ram_prepare());
  static struct limine_memmap_entry many[257];static struct limine_memmap_entry *mp[257];
  for(unsigned i=0;i<257;i++){many[i]=(struct limine_memmap_entry){.base=0x100000+4096ull*i,.length=4096,.type=LIMINE_MEMMAP_USABLE};mp[i]=&many[i];}
  m=(struct limine_memmap_response){.entry_count=257,.entries=mp};memmap_request.response=&m;assert(!source_audit_ram_prepare());
  m.entry_count=256;assert(source_audit_ram_prepare() && source_audit_ram.count==256 && !source_audit_native.inventory_reason);
  puts("inventory-ram: PASS captured sorted RAM only, malformed/overlap/capacity refusal, no writes");return 0;
 }
 if(argc==2 && (!strcmp(argv[1],"--range-endpoint") || !strcmp(argv[1],"--admission-witness")))return original_display_fixture_main(argc,argv);
 int code=engines_adapter_main(argc,argv);
 struct intel_source_audit_native *a=&source_audit_native;
 printf("source-audit-fixture: state=%u complete=%u registered=%u clocks=%u pci=%u planes=%u caches=%u actual=%u command=%u\n",a->state,a->complete,a->registered,audit_clocks,audit_pci,audit_planes,audit_caches,engines_actual,requester_command);
 if(a->complete && source_requester_native.result.complete){assert(range_calls && admission_capture.queries==range_calls && !admission_capture.code);assert(admission_cost_capture.count && !admission_cost_capture.flags);assert(audit_inventories==2 && a->inventory_count==2 && !a->inventory_reason);}
 if(a->complete && source_requester_native.result.complete)assert(a->samples==2 && a->pci_reads==18 && a->plane_reads==54 && a->cache_checks==2 && a->range_checks==4 && !a->registered);
 return code;
}
#include "fixture_server.h"

#ifndef INTEL_AUDIT_FIXTURE_MAIN
#define INTEL_AUDIT_FIXTURE_MAIN main
#endif
int INTEL_AUDIT_FIXTURE_MAIN(int argc,char **argv) {
#ifndef _WIN32
 if(argc==2 && !strcmp(argv[1],"--fixture-server"))return fixture_server(audit_case_main);
#endif
 return audit_case_main(argc,argv);
}
