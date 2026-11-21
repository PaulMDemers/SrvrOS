/* Actual CPU-side guard only. This file never grants a hardware rights mask. */
static uint64_t reserved_epoch;
static bool reserved_guard_fail(struct vmm_unmapped_detail *d,unsigned reason) {
 if(d){*d=(struct vmm_unmapped_detail){0};d->walk.reason=reason;}return false;
}
static uint64_t reserved_guard_flags(void) {
#ifdef SRVROS_VMM_HOST_TEST
 return vmm_test_rflags();
#else
 uint64_t flags;__asm__ volatile("pushfq; popq %0" : "=r"(flags) : : "memory");return flags;
#endif
}
static bool reserved_guard_identity(const struct vmm_reserved_guard *g) {
 return g && reserved_guard.owner==g && g->object==g && g->epoch==reserved_guard.epoch;
}
static bool reserved_guard_observe(struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 struct vmm_alias_inventory result;
 if(reserved_guard_flags()&512)return reserved_guard_fail(d,VMM_ALIAS_ROOT);
 if(alias_inventory_cr4()!=reserved_guard.cr4 || alias_inventory_efer()!=reserved_guard.efer)
  return reserved_guard_fail(d,VMM_ALIAS_MODE);
 for(unsigned i=0;i<reserved_guard.count;i++)
  for(unsigned page=0;page<reserved_guard.spans[i].pages;page++)
   if(!pmm_frame_has_tag(reserved_guard.spans[i].physical+(uint64_t)page*4096,PMM_FRAME_RESERVED))
    return reserved_guard_fail(d,VMM_ALIAS_PMM_RESERVED);
 if(!vmm_diagnostic_kernel_physical_unmapped(reserved_guard.root,reserved_guard.spans,
    reserved_guard.count,reserved_guard.bits,reserved_guard.ram,reserved_guard.ram_count,&result,d))return false;
 if(reserved_guard.io_page) {
  struct vmm_physical_span page={reserved_guard.io_page,1};
  struct vmm_alias_inventory io_walk;
  if(!vmm_diagnostic_kernel_physical_unmapped(reserved_guard.root,&page,1,reserved_guard.bits,
      reserved_guard.ram,reserved_guard.ram_count,&io_walk,d))return false;
 }
 if(!reserved_guard_identity(reserved_guard.owner))return reserved_guard_fail(d,VMM_ALIAS_GUARD);
 if(alias_inventory_cr4()!=reserved_guard.cr4 || alias_inventory_efer()!=reserved_guard.efer)
  return reserved_guard_fail(d,VMM_ALIAS_MODE);
 /* Mode reads are callback boundaries in fixtures; the final flags/root check
  * must follow them, not precede them. No table pointer survives this call. */
 if((reserved_guard_flags()&512) || read_cr3()!=reserved_guard.root)
  return reserved_guard_fail(d,VMM_ALIAS_ROOT);
 *out=result;return true;
}
static bool reserved_guard_begin(struct vmm_reserved_guard *g,uint64_t root,
 const struct vmm_physical_span *spans,unsigned count,unsigned bits,
 const struct vmm_alias_ram *ram,unsigned ram_count,uint64_t io_page,
 struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 if(d)*d=(struct vmm_unmapped_detail){0};
 if(!g || g->object || g->epoch || !out || !spans || !count || count>VMM_ALIAS_SPANS ||
    !ram || !ram_count || ram_count>VMM_ALIAS_RAM_LIMIT || bits<36 || bits>52)
  return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 if(reserved_guard.owner || __atomic_load_n(&reserved_mutations,__ATOMIC_SEQ_CST))return reserved_guard_fail(d,VMM_ALIAS_BUSY);
 if(reserved_guard_flags()&512)return reserved_guard_fail(d,VMM_ALIAS_ROOT);
 uint64_t limit=1ull<<bits;
 if((io_page&4095) || io_page>=limit)return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 for(unsigned i=0;i<count;i++) {
  if(!spans[i].pages || spans[i].pages>4096 || (spans[i].physical&4095) ||
     spans[i].physical>=limit || (uint64_t)spans[i].pages*4096>limit-spans[i].physical)
   return reserved_guard_fail(d,VMM_ALIAS_INPUT);
  if(io_page && io_page>=spans[i].physical && io_page-spans[i].physical<(uint64_t)spans[i].pages*4096)
   return reserved_guard_fail(d,VMM_ALIAS_INPUT);
  for(unsigned j=0;j<i;j++)if(spans[i].physical<spans[j].physical+(uint64_t)spans[j].pages*4096 &&
      spans[j].physical<spans[i].physical+(uint64_t)spans[i].pages*4096)return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 }
 if(reserved_epoch==UINT64_MAX)return reserved_guard_fail(d,VMM_ALIAS_GUARD);
 /* Publish PREPARING before PMM, mode or table callbacks. No mutation, second
  * admission or recursive check may pass through a partially validated owner. */
 reserved_guard.owner=g;reserved_guard.state=VMM_GUARD_PREPARING;
 reserved_guard.epoch=++reserved_epoch;g->object=g;g->epoch=reserved_guard.epoch;
 reserved_guard.io_page=io_page;
 reserved_guard.root=root;reserved_guard.count=count;reserved_guard.bits=bits;reserved_guard.ram_count=ram_count;
 for(unsigned i=0;i<count;i++)reserved_guard.spans[i]=spans[i];
 for(unsigned i=0;i<ram_count;i++)reserved_guard.ram[i]=ram[i];
 reserved_guard.cr4=alias_inventory_cr4();reserved_guard.efer=alias_inventory_efer();
 if(!reserved_guard_observe(out,d)) {
  /* No rights or aliases were issued; a failed admission may drop this empty
   * CPU deny scope. The original handle remains consumed. */
  reserved_guard.owner=NULL;reserved_guard.state=VMM_GUARD_EMPTY;return false;
 }
 reserved_guard.state=VMM_GUARD_ACTIVE;return true;
}
bool vmm_reserved_guard_begin(struct vmm_reserved_guard *g,uint64_t root,
 const struct vmm_physical_span *spans,unsigned count,unsigned bits,
 const struct vmm_alias_ram *ram,unsigned ram_count,
 struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 return reserved_guard_begin(g,root,spans,count,bits,ram,ram_count,0,out,d);
}
bool vmm_reserved_guard_begin_io_page(struct vmm_reserved_guard *g,uint64_t root,
 const struct vmm_physical_span *spans,unsigned count,unsigned bits,
 const struct vmm_alias_ram *ram,unsigned ram_count,uint64_t io_page,
 struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 if(!io_page)return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 return reserved_guard_begin(g,root,spans,count,bits,ram,ram_count,io_page,out,d);
}
bool vmm_reserved_guard_io_page_held(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page) {
 return page && reserved_guard_identity(g) && reserved_guard.state==VMM_GUARD_ACTIVE &&
        reserved_guard.io_page==page && reserved_guard.root==root &&
        !(reserved_guard_flags()&512) && read_cr3()==root;
}
bool vmm_reserved_guard_pin_io_page(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,const void *child) {
 if(!child || reserved_guard.io_borrower || !vmm_reserved_guard_io_page_held(g,root,page))return false;
 reserved_guard.io_borrower=child;
 return true;
}
bool vmm_reserved_guard_io_page_pinned(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,const void *child) {
 return child && reserved_guard.io_borrower==child && vmm_reserved_guard_io_page_held(g,root,page);
}
bool vmm_reserved_guard_unpin_io_page(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,const void *child) {
 if(!vmm_reserved_guard_io_page_pinned(g,root,page,child))return false;
 reserved_guard.io_borrower=NULL;
 return true;
}
static bool reserved_guard_validate(struct vmm_reserved_guard *g,bool close,
 struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 if(d)*d=(struct vmm_unmapped_detail){0};
 if(!out || !reserved_guard_identity(g)) {
  if(g && reserved_guard.owner==g)reserved_guard.state=VMM_GUARD_FAULT;
  return reserved_guard_fail(d,VMM_ALIAS_GUARD);
 }
 if(reserved_guard.state!=VMM_GUARD_ACTIVE || (close && reserved_guard.io_borrower) || __atomic_load_n(&reserved_mutations,__ATOMIC_SEQ_CST))
  return reserved_guard_fail(d,VMM_ALIAS_BUSY);
 reserved_guard.state=VMM_GUARD_CHECKING;
 if(!reserved_guard_observe(out,d)) {reserved_guard.state=VMM_GUARD_FAULT;return false;}
 if(close){reserved_guard.owner=NULL;reserved_guard.state=VMM_GUARD_EMPTY;}
 else reserved_guard.state=VMM_GUARD_ACTIVE;
 return true;
}
bool vmm_reserved_guard_check(struct vmm_reserved_guard *g,struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 return reserved_guard_validate(g,false,out,d);
}
bool vmm_reserved_guard_close(struct vmm_reserved_guard *g,struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 return reserved_guard_validate(g,true,out,d);
}
