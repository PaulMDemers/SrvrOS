#define _POSIX_C_SOURCE 200809L
#include <srvros/intel_ggtt_invalidate.h>
/* Actual combined port in the original v36 fixture ownership graph. */
#include <srvros/x86_cache.h>
#include <srvros/vmm.h>
static void intel_display_test_ggtt_boundary(void);
static bool intel_display_test_ggtt_cache(struct x86_cache_snapshot *);
static bool intel_display_test_ggtt_alias(uint64_t,uint64_t,uint64_t,struct vmm_alias_observation *);
static bool intel_display_test_ggtt_read(uint32_t,uint32_t *);
static enum intel_gt_write intel_display_test_ggtt_write(uint32_t,uint32_t);
static bool intel_display_test_ggtt_map(void);
static bool intel_display_test_ggtt_unmap(void);
static void intel_display_test_ggtt_return(void);
#define INTEL_VTD_FIXTURE_MAIN ggtt_parent_fixture_main
#include "test_intel_vtd_native.c"
#undef INTEL_VTD_FIXTURE_MAIN
static unsigned gg_boundaries,gg_reads,gg_writes,gg_maps,gg_unmaps,gg_caches,gg_aliases,gg_write_boundary,gg_read_boundary[2];
static bool gg_broken;static unsigned gg_boundary_reads,gg_boundary_writes,gg_boundary_maps,gg_boundary_unmaps;
static void intel_display_test_ggtt_boundary(void) {
 ++gg_boundaries;
 bool fault=jf("ggtt-time",gg_boundaries) || jf("ggtt-parent",gg_boundaries) || jf("ggtt-root",gg_boundaries) ||
 jf("ggtt-mmio",gg_boundaries) || jf("ggtt-hda-pci",gg_boundaries) || jf("ggtt-hda-alias",gg_boundaries) ||
 jf("ggtt-hda-borrow",gg_boundaries) || jf("ggtt-old-drain",gg_boundaries) ||
 (ggtt_native.maps && (jf("ggtt-epoch",gg_boundaries) || jf("ggtt-registration",gg_boundaries) || jf("ggtt-source",gg_boundaries))) ||
 (ggtt_native.mapped && jf("ggtt-map-drift",gg_boundaries));
 if(fault && !gg_broken) {
  gg_broken=true;gg_boundary_reads=gg_reads;gg_boundary_writes=gg_writes;gg_boundary_maps=gg_maps;gg_boundary_unmaps=gg_unmaps;
 }
 if(jf("ggtt-time",gg_boundaries))ticks=source_engines_native.result.origin+INTEL_DIAGNOSTIC_PREPARE_US+1;
 if(jf("ggtt-parent",gg_boundaries))ggtt_native.parent=NULL;
 if(jf("ggtt-root",gg_boundaries))current+=4096;
 if(jf("ggtt-mmio",gg_boundaries))ggtt_native.mmio+=65536;
 if(jf("ggtt-hda-pci",gg_boundaries))hda_command=6;
 if(jf("ggtt-hda-alias",gg_boundaries))pa[index_of(HDA_VA)]^=4096;
 if(jf("ggtt-hda-borrow",gg_boundaries))joint.scope.borrow.dependent_returns++;
 if(jf("ggtt-old-drain",gg_boundaries))vtd_native.core.p.writes=0;
 if(jf("ggtt-epoch",gg_boundaries) && ggtt_native.maps)ggtt_native.source.binding.epoch++;
 if(jf("ggtt-registration",gg_boundaries) && ggtt_native.maps)ggtt_native.registered_child=NULL;
 if(jf("ggtt-source",gg_boundaries) && ggtt_native.maps)ggtt_native.source.consumer=NULL;
 if(jf("ggtt-map-drift",gg_boundaries) && ggtt_native.mapped)pa[INTEL_CAPTURE_PAGES+17]^=4096;
}
static bool intel_display_test_ggtt_cache(struct x86_cache_snapshot *out) {
 ++gg_caches;*out=source_audit_native.cache_context[1];
 if(jf("ggtt-cache-drift",gg_caches))out->pat^=1;
 return !jf("ggtt-cache",gg_caches);
}
static bool intel_display_test_ggtt_alias(uint64_t root,uint64_t va,uint64_t physical,struct vmm_alias_observation *out) {
 assert(root==0x1000 && va==GGTT_NATIVE_VA && physical==0xc0101000 && wake_irq);
 ++gg_aliases;*out=(struct vmm_alias_observation){.pages=1,.leaves=1,.pat_index=2,.writable=1,.no_execute=1,.leaf_sizes=1};
 if(jf("ggtt-readonly",gg_aliases))out->writable=0;
 if(jf("ggtt-user",gg_aliases))out->user=1;
 if(jf("ggtt-pat",gg_aliases))out->pat_index=0;
 if(jf("ggtt-nx",gg_aliases))out->no_execute=0;
 if(jf("ggtt-large",gg_aliases))out->leaf_sizes=2;
 return !jf("ggtt-alias",gg_aliases);
}
static bool intel_display_test_ggtt_map(void){++gg_maps;return !jf("ggtt-map",gg_maps);}
static bool intel_display_test_ggtt_unmap(void){++gg_unmaps;return !jf("ggtt-unmap",gg_unmaps);}
static bool intel_display_test_ggtt_read(uint32_t off,uint32_t *out) {
 assert(wake_irq && requester_command==3 && source_engines_native.result.state==ISE_HELD && ggtt_native.mapped && off==0x101008);
 assert(gg_reads<2 && gg_writes==gg_reads);gg_read_boundary[gg_reads]=gg_boundaries;++gg_reads;
 if(jf("ggtt-read",gg_reads))return false;
 *out=jf("ggtt-value",gg_reads)?1:jf("ggtt-float",gg_reads)?UINT32_MAX:0;
 return true;
}
static enum intel_gt_write intel_display_test_ggtt_write(uint32_t off,uint32_t value) {
 assert(wake_irq && requester_command==3 && off==0x101008 && value==1 && !gg_writes && gg_reads==1);
 gg_write_boundary=gg_boundaries;++gg_writes;
 if(jf("ggtt-write-none",1))return GT_WRITE_NONE;
 if(jf("ggtt-write-unknown",1))return GT_WRITE_UNCERTAIN;
 return GT_WRITE_DONE;
}
static void intel_display_test_ggtt_return(void) {
 static unsigned calls;if(++calls!=1)return;
 struct intel_ggtt_invalidate *r=&ggtt_native.core;
 for(unsigned i=1;i<=35;i++)if(jf("ggtt-final",i))switch(i) {
 case 1:r->complete=false;break;case 2:r->retained=true;break;case 3:r->reason=1;break;
 case 4:r->p.writes=0;break;case 5:r->p.reads=1;break;case 6:r->p.guards--;break;
 case 7:r->p.posted=1;break;case 8:r->p.last++;break;case 9:r->binding.epoch++;break;
 case 10:ggtt_native.source.consumer=NULL;break;case 11:r->original_io.read=NULL;break;
 case 12:ggtt_native.maps=0;break;case 13:ggtt_native.retired=0;break;case 14:ggtt_native.mapped=true;break;
 case 15:ggtt_native.registered_child=&ggtt_native.source;break;case 16:ggtt_native.complete=false;break;
 case 17:ggtt_native.end=source_engines_native.result.child_end+1;break;case 18:ggtt_native.root+=4096;break;
 case 19:ggtt_native.alias.no_execute=0;break;case 20:ggtt_native.effective=1;break;case 21:ggtt_native.cache_checks--;break;
 case 22:r->binding.outer_begin++;break;case 23:ggtt_native.source.binding.root+=4096;break;
 case 24:ggtt_native.source.binding.owner=NULL;break;case 25:r->binding.mmio+=4096;break;
 case 26:ggtt_native.alias.pat_index=0;break;case 27:ggtt_native.begin=r->p.begin+1;break;
 case 28:r->clock_at=1;break;case 29:r->object=NULL;break;case 30:r->p.uncertain=1;break;
 case 31:ggtt_native.failed_clock=1;break;case 32:ggtt_native.source.binding.sequence++;break;
 case 33:r->io.now=NULL;break;case 34:ggtt_native.alias_checks--;break;case 35:r->p.initial=1;break;
 }
}
static int ggtt_case(int argc,char **argv) {
 int code=vtd_case(argc,argv);
 if(gg_broken)assert(gg_reads==gg_boundary_reads && gg_writes==gg_boundary_writes && gg_maps==gg_boundary_maps && gg_unmaps==gg_boundary_unmaps);
 if(source_requester_native.result.complete)assert(ggtt_returned() && gg_reads==2 && gg_writes==1 && gg_maps==1 && gg_unmaps==1 && !pa[INTEL_CAPTURE_PAGES+17]);
 printf("ggtt-native-fixture: boundaries=%u reads=%u writes=%u maps=%u unmaps=%u caches=%u aliases=%u complete=%u retained=%u write_boundary=%u read0_boundary=%u read1_boundary=%u\n",
 gg_boundaries,gg_reads,gg_writes,gg_maps,gg_unmaps,gg_caches,gg_aliases,ggtt_returned(),ggtt_native.retained,gg_write_boundary,gg_read_boundary[0],gg_read_boundary[1]);
 return code;
}
int main(int argc,char **argv) {
#ifndef _WIN32
 if(argc==2 && !strcmp(argv[1],"--fixture-server"))return fixture_server(ggtt_case);
#endif
 return ggtt_case(argc,argv);
}
