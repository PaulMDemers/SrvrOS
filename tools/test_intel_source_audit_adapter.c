#include <srvros/x86_cache.h>
static bool intel_display_test_source_audit_clock(uint64_t *);
static bool intel_display_test_source_audit_pci(unsigned,uint32_t *);
static bool intel_display_test_source_audit_plane(unsigned,unsigned,uint32_t *);
static bool intel_display_test_source_audit_cache(struct x86_cache_snapshot *);
#define INTEL_ENGINES_FIXTURE_MAIN engines_adapter_main
#include "test_intel_source_engines_adapter.c"
static unsigned audit_clocks,audit_pci,audit_planes,audit_caches;
static bool intel_display_test_source_audit_clock(uint64_t *at) {
 ++audit_clocks;struct intel_source_audit_native *a=&source_audit_native;
 if(jf("audit-zero",audit_clocks)){*at=0;return true;}
 if(jf("audit-backward",audit_clocks)){*at=1;return true;}
 if(jf("audit-time",audit_clocks))ticks=a->origin+100001;
 if(jf("audit-parent-time",audit_clocks))ticks=a->parent_begin+300001;
 if(jf("audit-source",audit_clocks))a->parent=0;
 if(jf("audit-provider",audit_clocks))a->provider=0;
 if(jf("audit-epoch",audit_clocks))a->epoch++;
 if(jf("audit-kind",audit_clocks))a->kind=3-a->kind;
 if(jf("audit-mode",audit_clocks))a->mode^=3;
 if(jf("audit-origin",audit_clocks))a->origin++;
 if(jf("audit-parent-origin",audit_clocks))a->parent_begin++;
 if(jf("audit-count",audit_clocks))a->pci_reads++;
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
 *out=(struct x86_cache_snapshot){.cpu={{0x756e6547,0x49656e69,0x6c65746e},0x306d4,0x11000,39,0x80050033},
 .pat=0x0007040600070406ull,.cap=0x402,.def_type=0x806,.base={0xb0000001,0},.mask={0x7ff0000800ull,0},
 .status=CACHE_OBSERVED,.reads=10,.count=2,.fixed_count=0,.begin_us=begin,.end_us=ticks};
 if(jf("audit-cache-fail",audit_caches))return false;
 if(jf("audit-cache-time",audit_caches))out->end_us=source_audit_native.origin+100001;
 if(jf("audit-cache-before",audit_caches))out->begin_us=1;
 if(jf("audit-cache-zero",audit_caches))out->reads=0;
 return true;
}
int main(int argc,char **argv) {
 int code=engines_adapter_main(argc,argv);
 struct intel_source_audit_native *a=&source_audit_native;
 printf("source-audit-fixture: state=%u complete=%u registered=%u clocks=%u pci=%u planes=%u caches=%u actual=%u command=%u\n",a->state,a->complete,a->registered,audit_clocks,audit_pci,audit_planes,audit_caches,engines_actual,requester_command);
 if(a->complete && source_requester_native.result.complete)assert(a->samples==2 && a->pci_reads==18 && a->plane_reads==54 && a->cache_checks==2 && !a->registered);
 return code;
}
