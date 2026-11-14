/* Included only in the separate pinned reader image, never the old lease ELF. */
#include <srvros/intel_reader_audit.h>
#define READER_VA (BACKING_VA+0x50000)
struct pinned_readers {
 struct hda_owner_adapter *parent;
 struct intel_hda_owner_result *owner;
 struct intel_hda_owner_token *token;
 struct intel_hda_owner_borrow *borrow;
 struct intel_reader_result result;
 unsigned maps,retired,cache_checks;
 bool mapped,uncertain;
 uint64_t physical;
};
static bool pinned_reader_guard(void *ctx) {
 struct pinned_readers *a=ctx;
 return !a->uncertain && !trial_mapping_fault && intel_hda_owner_live(a->owner,a->token) &&
  a->borrow->object==a->borrow && a->borrow->owner==a->owner && a->borrow->epoch==a->token->epoch &&
  a->borrow->active && a->owner->pins==1 && a->parent->h.initial.command==2 && hda_basic(&a->parent->h);
}
static bool pinned_reader_config(void *ctx,unsigned off,uint32_t *out) {
 struct pinned_readers *a=ctx;
 if(!pinned_reader_guard(a) || !out || (off!=0x34 && off!=0x50 && off!=0x60 && off!=0x70 && off!=0x74 && off!=0x78))return false;
 *out=pci_read_config32(a->parent->h.device,(uint8_t)off);return true;
}
static bool pinned_reader_uc(struct pinned_readers *a,uint64_t pa,uint32_t off) {
 a->cache_checks++;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_pinned_reader_uc(pa,off);
#else
 return intel_cache_probe_reader_uc_page(pa,off);
#endif
}
static bool pinned_reader_plane(void *ctx,unsigned pipe,uint32_t out[INTEL_READER_WORDS]) {
 struct pinned_readers *a=ctx;uint64_t old=0;
 if(pipe>=3 || !pinned_reader_guard(a) || a->mapped)return false;
 uint32_t off=0x70000+pipe*0x1000;uint64_t pa=result.mmio_base+off;
 if(!backing_range_reserved(pa,4096) || !pinned_reader_uc(a,pa,off) || vmm_virt_to_phys_in_space(a->parent->h.gt.a.common.cr3,READER_VA,&old))return false;
 if(!vmm_map_page_in_space(a->parent->h.gt.a.common.cr3,READER_VA,pa,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE)) {
  if(vmm_virt_to_phys_in_space(a->parent->h.gt.a.common.cr3,READER_VA,&old)){a->uncertain=true;trial_mapping_fault=true;}
  return false;
 }
 a->mapped=true;a->physical=pa;a->maps++;
 bool good=pinned_reader_guard(a) && vmm_virt_to_phys_in_space(a->parent->h.gt.a.common.cr3,READER_VA,&old) && old==pa;
 for(unsigned i=0;good && i<INTEL_READER_WORDS;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  good=intel_display_test_pinned_reader_read(pipe,i,&out[i]);
#else
  out[i]=*(volatile const uint32_t *)(READER_VA+intel_reader_offsets[i]);
#endif
 }
 good=good && pinned_reader_guard(a) && pinned_reader_uc(a,pa,off);
 if(!vmm_virt_to_phys_in_space(a->parent->h.gt.a.common.cr3,READER_VA,&old) || old!=pa ||
    !vmm_unmap_page_in_space(a->parent->h.gt.a.common.cr3,READER_VA,&old) || old!=pa) {
  a->uncertain=true;trial_mapping_fault=true;return false;
 }
 a->mapped=false;a->retired++;return good;
}
static bool pinned_reader_run(struct pinned_readers *a,struct intel_hda_result *mid) {
 struct intel_reader_io io={a,pinned_reader_guard,pinned_reader_config,pinned_reader_plane,now};
 return intel_reader_pass(&a->result,io,0) && intel_hda_owner_check(a->owner,a->borrow,mid) &&
  intel_reader_pass(&a->result,io,1) && intel_reader_finish(&a->result,io);
}
static void pinned_reader_report(const struct pinned_readers *a) {
 const struct intel_reader_result *r=&a->result;
 for(unsigned n=0;n<r->configs;n++) {
  const uint32_t *c=r->sample[n].config;
  console_printf("intel-pinned-config: v=1 sample=%u c0=%u c1=%u c2=%u c3=%u c4=%u c5=%u\n",(uint64_t)n,(uint64_t)c[0],(uint64_t)c[1],(uint64_t)c[2],(uint64_t)c[3],(uint64_t)c[4],(uint64_t)c[5]);
  for(unsigned p=0;p<3 && n*3+p<r->planes;p++) {
   const uint32_t *w=r->sample[n].plane[p];
   console_printf("intel-pinned-plane: v=1 sample=%u pipe=%u w0=%u w1=%u w2=%u w3=%u w4=%u w5=%u w6=%u w7=%u w8=%u w9=%u w10=%u w11=%u w12=%u\n",(uint64_t)n,(uint64_t)p,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7],(uint64_t)w[8],(uint64_t)w[9],(uint64_t)w[10],(uint64_t)w[11],(uint64_t)w[12]);
  }
 }
 console_printf("intel-pinned-summary: v=1 samples=%u configs=%u planes=%u reads=%u reason=%u primary=%u cursor=%u sprite=%u mismatch=%u stable=%u nonposted=%u complete=%u begin_us=%u end_us=%u maps=%u retired=%u retained=%u cache_checks=%u display_writes=0 posted_drain=0 prefetch_retired=0 reader_retirement=0 managed_pages=0\n",(uint64_t)r->samples,(uint64_t)r->configs,(uint64_t)r->planes,(uint64_t)r->reads,(uint64_t)r->reason,(uint64_t)r->primary,(uint64_t)r->cursor,(uint64_t)r->sprite,(uint64_t)r->mismatch,(uint64_t)r->stable,(uint64_t)r->nonposted_clear,(uint64_t)r->complete,r->begin_us,r->end_us,(uint64_t)a->maps,(uint64_t)a->retired,(uint64_t)(a->mapped||a->uncertain),(uint64_t)a->cache_checks);
}
