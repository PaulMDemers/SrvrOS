#define _POSIX_C_SOURCE 200809L
#include <srvros/intel_vtd_drain.h>
#include <srvros/x86_cache.h>
#include <srvros/vmm.h>
static void intel_display_test_vtd_prepare(void);
static void intel_display_test_vtd_boundary(void);
static bool intel_display_test_vtd_cache(struct x86_cache_snapshot *);
static bool intel_display_test_vtd_alias(uint64_t,uint64_t,uint64_t,struct vmm_alias_observation *);
static bool intel_display_test_vtd_read(uint32_t,uint32_t *);
static enum intel_gt_write intel_display_test_vtd_write(uint32_t,uint32_t);
static bool intel_display_test_vtd_map(void);
static bool intel_display_test_vtd_unmap(void);
static void intel_display_test_vtd_return(void);
static void intel_display_test_vtd_after(void);
#define INTEL_AUDIT_FIXTURE_MAIN audit_fixture_main
#include "test_intel_source_audit_adapter.c"
static unsigned vd_boundaries,vd_reads,vd_writes,vd_polls,vd_maps,vd_unmaps,vd_caches,vd_aliases;
static bool vd_broken;static unsigned vd_boundary_reads,vd_boundary_writes;
static void intel_display_test_vtd_prepare(void) {
 const char *path=getenv("SRVROS_VTD_DMAR");assert(path);
 FILE *f=fopen(path,"rb");assert(f && fread(extra_bytes,1,136,f)==136 && fgetc(f)==EOF);assert(!fclose(f));
 extra_table=true;extra_dmar=(struct acpi_table_info){.physical_address=(uint64_t)(uintptr_t)extra_bytes,.length=136};
 memcpy(extra_dmar.signature,"DMAR",4);
 static struct limine_memmap_entry lo,hi;static struct limine_memmap_entry *map[10];
 assert(memory.entry_count<8);unsigned n=(unsigned)memory.entry_count;for(unsigned i=0;i<n;i++)map[i]=memory.entries[i];
 lo=(struct limine_memmap_entry){0xfed1c000,0x4000,LIMINE_MEMMAP_RESERVED};hi=(struct limine_memmap_entry){0xffd70000,0x30000,LIMINE_MEMMAP_RESERVED};
 map[n++]=&lo;map[n++]=&hi;memory.entries=map;memory.entry_count=n;
 if(jf("vtd-table",1))extra_bytes[9]^=1;
 if(jf("vtd-resource",1))lo.base++;
}
static void intel_display_test_vtd_boundary(void) {
 ++vd_boundaries;
 bool fault=jf("vtd-parent",vd_boundaries) || jf("vtd-root",vd_boundaries) || jf("vtd-time",vd_boundaries) || jf("vtd-inherited-map",vd_boundaries) || jf("vtd-inherited-root",vd_boundaries) ||
  (vtd_native.maps && (jf("vtd-epoch",vd_boundaries) || jf("vtd-registration",vd_boundaries) || jf("vtd-source",vd_boundaries))) ||
  (vtd_native.mapped && jf("vtd-map-drift",vd_boundaries));
 if(fault && !vd_broken){vd_broken=true;vd_boundary_reads=vd_reads;vd_boundary_writes=vd_writes;}
 if(jf("vtd-epoch",vd_boundaries))vtd_native.source.binding.epoch++;
 if(jf("vtd-parent",vd_boundaries))vtd_native.parent=NULL;
 if(jf("vtd-registration",vd_boundaries))vtd_native.registered_child=NULL;
 if(jf("vtd-root",vd_boundaries))vtd_native.root+=4096;
 if(jf("vtd-source",vd_boundaries))vtd_native.source.consumer=NULL;
 if(jf("vtd-inherited-map",vd_boundaries))pa[INTEL_CAPTURE_PAGES+4]^=4096;
 if(jf("vtd-inherited-root",vd_boundaries))current+=4096;
 if(jf("vtd-time",vd_boundaries))ticks=source_engines_native.result.origin+100001;
 if(jf("vtd-map-drift",vd_boundaries) && vtd_native.mapped)pa[INTEL_CAPTURE_PAGES+16]^=4096;
}
static bool intel_display_test_vtd_map(void) {++vd_maps;return !jf("vtd-map",vd_maps);}
static bool intel_display_test_vtd_unmap(void) {++vd_unmaps;return !jf("vtd-unmap",vd_unmaps);}
static bool intel_display_test_vtd_cache(struct x86_cache_snapshot *out) {
 ++vd_caches;*out=source_audit_native.cache_context[1];
 if(jf("vtd-cache-drift",vd_caches))out->pat^=1;
 return !jf("vtd-cache",vd_caches);
}
static bool intel_display_test_vtd_alias(uint64_t root,uint64_t va,uint64_t pa,struct vmm_alias_observation *out) {
 assert(root==0x1000 && va==VTD_NATIVE_VA && pa==INTEL_VTD_DRAIN_UNIT && wake_irq);
 ++vd_aliases;*out=(struct vmm_alias_observation){.pages=1,.leaves=1,.pat_index=2,.writable=1,.no_execute=1,.leaf_sizes=1};
 if(jf("vtd-alias-readonly",vd_aliases))out->writable=0;
 if(jf("vtd-alias-user",vd_aliases))out->user=1;
 if(jf("vtd-alias-pat",vd_aliases))out->pat_index=0;
 if(jf("vtd-alias-nx",vd_aliases))out->no_execute=0;
 if(jf("vtd-alias-large",vd_aliases))out->leaf_sizes=2;
 return !jf("vtd-alias",vd_aliases);
}
static bool intel_display_test_vtd_read(uint32_t off,uint32_t *out) {
 assert(wake_irq && requester_command==3 && source_engines_native.result.state==ISE_HELD && vtd_native.mapped);
 ++vd_reads;if(jf("vtd-read",vd_reads))return false;
 if(jf("vtd-float",vd_reads)){*out=UINT32_MAX;return true;}
 switch(off) {
 case 0:*out=0x10;break;case 8:*out=0x40660462;break;case 12:*out=0x01c0000c;break;
 case 16:*out=0x1ff0505e;break;case 20:*out=0x7e;break;
 case 28:case 32:case 36:case 40:case 52:case 0x508:*out=0;break;
 case 44:*out=0x08000000;break;
 case 0x50c:
  if(!vd_writes)*out=0x02000000;
  else {*out=(!strcmp(engine_mode,"vtd-poll") && vd_polls++<2)?0x92030000:0x12030000;}
  break;
 default:assert(false);return false;
 }
 if(jf("vtd-value",vd_reads))*out^=1;
 return true;
}
static enum intel_gt_write intel_display_test_vtd_write(uint32_t off,uint32_t value) {
 assert(wake_irq && requester_command==3 && off==0x50c && value==0x90030000 && !vd_writes);
 ++vd_writes;if(jf("vtd-write-none",1))return GT_WRITE_NONE;
 if(jf("vtd-write-unknown",1))return GT_WRITE_UNCERTAIN;
 return GT_WRITE_DONE;
}
static void intel_display_test_vtd_after(void) {
 if(jf("vtd-late-time",1))ticks=source_engines_native.result.origin+100001;
}
static void intel_display_test_vtd_return(void) {
 static unsigned calls;if(++calls!=1)return;
 struct intel_vtd_drain *r=&vtd_native.core;
 for(unsigned i=1;i<=40;i++)if(jf("vtd-final",i))switch(i) {
 case 1:r->complete=false;break;case 2:r->retained=true;break;case 3:r->reason=1;break;
 case 4:r->p.writes=0;break;case 5:r->p.reads--;break;case 6:r->p.guards--;break;case 7:r->p.polls=0;break;
 case 8:r->p.observed[0]=0;break;case 9:r->p.observed[255]=1;break;case 10:r->p.last++;break;
 case 11:r->p.context[1][2]^=1;break;case 12:vtd_native.source.consumer=NULL;break;
 case 13:vtd_native.source.object=NULL;break;case 14:r->binding.epoch++;break;case 15:r->binding.hash^=1;break;
 case 16:vtd_native.source.binding.sequence++;break;case 17:vtd_native.source.binding.device++;break;
 case 18:r->io.now=NULL;break;case 19:r->original_io.read=NULL;break;case 20:vtd_native.maps=0;break;
 case 21:vtd_native.retired=0;break;case 22:vtd_native.mapped=true;break;case 23:vtd_native.registered_child=&vtd_native.source;break;
 case 24:vtd_native.complete=false;break;case 25:vtd_native.end=source_engines_native.result.child_end+1;break;
 case 26:vtd_native.root+=4096;break;case 27:vtd_native.alias.no_execute=0;break;case 28:vtd_native.effective=1;break;
 case 29:vtd_native.cache_checks--;break;case 30:r->binding.outer_begin++;break;case 31:vtd_native.source.binding.root+=4096;break;
 case 32:vtd_native.source.binding.owner=NULL;break;case 33:r->binding.unit+=4096;break;case 34:vtd_native.resource=0;break;
 case 35:vtd_native.alias.pat_index=0;break;case 36:vtd_native.begin=r->p.begin+1;break;case 37:r->clock_at=1;break;
 case 38:r->p.initial=0;break;case 39:r->object=NULL;break;case 40:r->p.uncertain=1;break;
 }
}
static int vtd_case(int argc,char **argv) {
 int code=audit_case_main(argc,argv);
 if(vd_broken)assert(vd_reads==vd_boundary_reads && vd_writes==vd_boundary_writes);
 if(vtd_native.complete && source_requester_native.result.complete)assert(vtd_returned() && vd_maps==1 && vd_unmaps==1 && vd_writes==1 && !pa[INTEL_CAPTURE_PAGES+16]);
 printf("vtd-native-fixture: boundaries=%u reads=%u writes=%u maps=%u unmaps=%u caches=%u aliases=%u complete=%u retained=%u\n",
  vd_boundaries,vd_reads,vd_writes,vd_maps,vd_unmaps,vd_caches,vd_aliases,vtd_returned(),vtd_native.retained);
 return code;
}
int main(int argc,char **argv) {
#ifndef _WIN32
 if(argc==2 && !strcmp(argv[1],"--fixture-server"))return fixture_server(vtd_case);
#endif
 return vtd_case(argc,argv);
}
