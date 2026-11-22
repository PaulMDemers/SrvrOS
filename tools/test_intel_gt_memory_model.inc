/* Independent PCI/translation/UC register model for the original new epoch. */
static uint64_t memory_pa[2],memory_iotlb=0x0200000000000000ull;
static unsigned memory_boundaries,memory_aliases,memory_resources,memory_maps;
static bool memory_parent(void) {
 return bootstrap_parent() && gpu_command==3 && hda_command==2 && wake_irq &&
 !display_access_allowed() && memory_identity();
}
static void intel_display_test_memory_boundary(void) {
 ++memory_boundaries;
 assert(bootstrap_parent() && gpu_command==3 && !display_access_allowed());
 if(bf("memory-zero",memory_boundaries))ticks=UINT64_MAX;
 if(bf("memory-back",memory_boundaries))ticks=gt_memory.d.last-2;
 if(bf("memory-over",memory_boundaries))ticks=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US+1;
 if(bf("memory-child",memory_boundaries))gt_bootstrap_native.memory_child=NULL;
 if(bf("memory-return",memory_boundaries))gt_bootstrap_native.memory_returns++;
 if(bf("memory-root",memory_boundaries))gt_memory.d.root^=4096;
 if(bf("memory-epoch",memory_boundaries))gt_memory.d.epoch++;
 if(bf("memory-data",memory_boundaries))gt_memory.d.reads++;
}
static bool intel_display_test_memory_translate(unsigned i,uint64_t *out) {
 assert(i<2);if(!memory_pa[i])return false;*out=memory_pa[i];return true;
}
static bool intel_display_test_memory_alias(unsigned i,struct vmm_alias_observation *out) {
 assert(i<2 && memory_parent() && memory_pa[i]==memory_physical(i));++memory_aliases;
 *out=(struct vmm_alias_observation){1,1,2,1,0,1,1};
 if(bf("memory-alias-user",memory_aliases))out->user=1;
 if(bf("memory-alias-epoch",memory_aliases))gt_memory.d.epoch++;
 if(bf("memory-alias-cache",memory_aliases))out->pat_index=1;
 return !bf("memory-alias",memory_aliases);
}
static bool intel_display_test_memory_resource(unsigned *out) {
 assert(memory_parent());++memory_resources;*out=2;
 if(bf("memory-resource-change",memory_resources))*out=3;
 return !bf("memory-resource",memory_resources);
}
static bool intel_display_test_memory_map(unsigned i,uint64_t physical) {
 assert(memory_parent() && i==memory_maps && i<2 && !memory_pa[i] && physical==memory_physical(i));
 if(bf("memory-map",i+1))return false;
 memory_pa[i]=physical;++memory_maps;return true;
}
static bool intel_display_test_memory_unmap(unsigned i,uint64_t *physical) {
 assert(memory_parent() && memory_records() && gt_memory.post.complete && i==1-memory_unmaps && memory_pa[i]);
 if(bf("memory-unmap",memory_unmaps+1))return false;
 *physical=memory_pa[i];memory_pa[i]=0;++memory_unmaps;return true;
}
static bool intel_display_test_memory_read(unsigned i,uint32_t off,uint32_t *out) {
 assert(memory_parent() && memory_pa[i]==memory_physical(i));++memory_reads;
 if(bf("memory-read",memory_reads))return false;
 uint64_t value=0;
 if(!i) {
  switch(off) {
  case 0:value=0x10;break;
  case 8:case 12:value=INTEL_VTD_DRAIN_CAP;break;
  case 16:case 20:value=INTEL_VTD_DRAIN_ECAP;break;
  case 28:case 32:case 36:case 52:break;
  case 40:case 44:value=0x0800000000000000ull;break;
  case 0x508:case 0x50c:value=memory_iotlb;break;
  default:assert(false);
  }
  *out=(uint32_t)(value>>((off&4)?32:0));
 } else {assert(off==8);*out=0;}
 if(bf("memory-float",memory_reads))*out=UINT32_MAX;
 return true;
}
static enum intel_gt_write intel_display_test_memory_write(unsigned i,uint32_t off,uint32_t value) {
 assert(memory_parent() && memory_pa[i]==memory_physical(i));
 assert((!i && off==0x50c && value==0x90030000 && !memory_writes) ||
 (i==1 && off==8 && value==1 && memory_writes==1));
 if(bf("memory-write-none",memory_writes+1))return GT_WRITE_NONE;
 if(bf("memory-write-unknown",memory_writes+1))return GT_WRITE_UNCERTAIN;
 if(!i)memory_iotlb=0x1203000000000000ull;
 ++memory_writes;return GT_WRITE_DONE;
}
