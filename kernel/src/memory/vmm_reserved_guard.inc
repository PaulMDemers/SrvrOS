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
static bool reserved_register_observe(uint64_t root,uint64_t reg,uint64_t alias) {
 struct vmm_alias_span query={alias,reg,1};
 struct vmm_alias_inventory inventory;
 struct vmm_alias_observation a;
 return alias_inventory_walk(root,&query,1,reserved_guard.bits,reserved_guard.ram,reserved_guard.ram_count,
                              &inventory,NULL,ALIAS_REGISTER,NULL) &&
         vmm_diagnostic_kernel_alias(root,alias,reg,1,reserved_guard.bits,&a,NULL) &&
         a.writable && !a.user && a.no_execute && a.leaf_sizes==1 && a.pat_index==2;
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
  struct vmm_physical_span pages[]={{reserved_guard.io_page,1},{reserved_guard.register_backing,1}};
  if(reserved_guard.register_backing &&
     !pmm_frame_has_tag(reserved_guard.register_backing,PMM_FRAME_RESERVED))
   return reserved_guard_fail(d,VMM_ALIAS_PMM_RESERVED);
  struct vmm_alias_inventory io_walk;
  if(!vmm_diagnostic_kernel_physical_unmapped(reserved_guard.root,pages,reserved_guard.register_backing?2:1,reserved_guard.bits,
      reserved_guard.ram,reserved_guard.ram_count,&io_walk,d))return false;
 }
 for(unsigned first=0;first<reserved_guard.port_count;first+=VMM_ALIAS_SPANS) {
  struct vmm_physical_span ports[VMM_ALIAS_SPANS];
  unsigned n=reserved_guard.port_count-first;
  if(n>VMM_ALIAS_SPANS)n=VMM_ALIAS_SPANS;
  for(unsigned i=0;i<n;i++)ports[i]=(struct vmm_physical_span){reserved_guard.ports[first+i],1};
  struct vmm_alias_inventory port_walk;
  if(!vmm_diagnostic_kernel_physical_unmapped(reserved_guard.root,ports,n,reserved_guard.bits,
      reserved_guard.ram,reserved_guard.ram_count,&port_walk,d))return false;
 }
 if(reserved_guard.register_alias && !reserved_register_observe(reserved_guard.root,
      reserved_guard.register_page,reserved_guard.register_alias))return reserved_guard_fail(d,VMM_ALIAS_FOREIGN);
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
 const struct vmm_alias_ram *ram,unsigned ram_count,uint64_t io_page,uint64_t backing,uint64_t reg,
 const uint64_t *ports,unsigned port_count,
 struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 if(d)*d=(struct vmm_unmapped_detail){0};
 if(!g || g->object || g->epoch || !out || !spans || !count || count>VMM_ALIAS_SPANS ||
    !ram || !ram_count || ram_count>VMM_ALIAS_RAM_LIMIT || bits<36 || bits>52)
  return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 if(reserved_guard.owner || __atomic_load_n(&reserved_mutations,__ATOMIC_SEQ_CST))return reserved_guard_fail(d,VMM_ALIAS_BUSY);
 if(reserved_guard_flags()&512)return reserved_guard_fail(d,VMM_ALIAS_ROOT);
 uint64_t limit=1ull<<bits;
 if((reg&4095) || (backing&4095) || reg>=limit || backing>=limit ||
    (!!reg != !!backing) || (reg && (!io_page || reg==io_page || backing==io_page || reg==backing)))
  return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 if((io_page&4095) || io_page>=limit)return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 if(port_count>VMM_GUARD_IO_LIMIT || (port_count && !ports))return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 for(unsigned i=0;i<port_count;i++) {
  uint64_t page=ports[i];
  if(!page || (page&4095) || page>=limit || page==io_page || page==reg || page==backing)
   return reserved_guard_fail(d,VMM_ALIAS_INPUT);
  for(unsigned j=0;j<i;j++)if(page==ports[j])return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 }
 for(unsigned i=0;i<count;i++) {
  if(!spans[i].pages || spans[i].pages>4096 || (spans[i].physical&4095) ||
     spans[i].physical>=limit || (uint64_t)spans[i].pages*4096>limit-spans[i].physical)
   return reserved_guard_fail(d,VMM_ALIAS_INPUT);
  for(unsigned j=0;j<port_count;j++)
   if(ports[j]>=spans[i].physical && ports[j]-spans[i].physical<(uint64_t)spans[i].pages*4096)
    return reserved_guard_fail(d,VMM_ALIAS_INPUT);
  if(io_page && io_page>=spans[i].physical && io_page-spans[i].physical<(uint64_t)spans[i].pages*4096)
   return reserved_guard_fail(d,VMM_ALIAS_INPUT);
  if(reg && ((reg>=spans[i].physical && reg-spans[i].physical<(uint64_t)spans[i].pages*4096) ||
             (backing>=spans[i].physical && backing-spans[i].physical<(uint64_t)spans[i].pages*4096)))
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
 reserved_guard.port_count=port_count;
 for(unsigned i=0;i<port_count;i++)reserved_guard.ports[i]=ports[i];
 reserved_guard.register_page=reg;reserved_guard.register_backing=backing;reserved_guard.register_alias=0;
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
 return reserved_guard_begin(g,root,spans,count,bits,ram,ram_count,0,0,0,NULL,0,out,d);
}
bool vmm_reserved_guard_begin_io_page(struct vmm_reserved_guard *g,uint64_t root,
 const struct vmm_physical_span *spans,unsigned count,unsigned bits,
 const struct vmm_alias_ram *ram,unsigned ram_count,uint64_t io_page,
 struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 if(!io_page)return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 return reserved_guard_begin(g,root,spans,count,bits,ram,ram_count,io_page,0,0,NULL,0,out,d);
}
bool vmm_reserved_guard_begin_register_window(struct vmm_reserved_guard *g,uint64_t root,
 const struct vmm_physical_span *spans,unsigned count,unsigned bits,const struct vmm_alias_ram *ram,unsigned ram_count,
 uint64_t aperture,uint64_t backing,uint64_t reg,struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 if(!reg || !backing)return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 return reserved_guard_begin(g,root,spans,count,bits,ram,ram_count,aperture,backing,reg,NULL,0,out,d);
}
bool vmm_reserved_guard_begin_submission_window(struct vmm_reserved_guard *g,uint64_t root,
 const struct vmm_physical_span *spans,unsigned count,unsigned bits,const struct vmm_alias_ram *ram,unsigned ram_count,
 uint64_t aperture,uint64_t backing,uint64_t reg,const uint64_t *ports,unsigned port_count,
 struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 if(!reg || !backing || !port_count)return reserved_guard_fail(d,VMM_ALIAS_INPUT);
 return reserved_guard_begin(g,root,spans,count,bits,ram,ram_count,aperture,backing,reg,ports,port_count,out,d);
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
/* Every aperture alias must be excluded by the original live borrow. */
bool vmm_reserved_guard_io_range_pinned(const struct vmm_reserved_guard *g,uint64_t root,
                                        uint64_t first,unsigned pages,const void *child) {
 if(!pages || pages>1+VMM_GUARD_IO_LIMIT || first>UINT64_MAX-4096ull*(pages-1) ||
    (!first || (first&4095)) || !vmm_reserved_guard_io_page_pinned(g,root,reserved_guard.io_page,child))return false;
 for(unsigned i=0;i<pages;i++){
  if(first+4096ull*i==reserved_guard.io_page)continue;
  unsigned j=0;
  while(j<reserved_guard.port_count && reserved_guard.ports[j]!=first+4096ull*i)++j;
  if(j==reserved_guard.port_count)return false;
 }
 return true;
}
bool vmm_reserved_guard_unpin_io_page(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,const void *child) {
 if(!vmm_reserved_guard_io_page_pinned(g,root,page,child))return false;
 if(reserved_guard.register_alias) {
  reserved_guard.state=VMM_GUARD_CHECKING;
  if(!reserved_register_observe(root,reserved_guard.register_page,reserved_guard.register_alias) ||
     !reserved_guard_identity(g) || reserved_guard.state!=VMM_GUARD_CHECKING ||
     alias_inventory_cr4()!=reserved_guard.cr4 || alias_inventory_efer()!=reserved_guard.efer ||
     (reserved_guard_flags()&512) || read_cr3()!=root) {
   reserved_guard.state=VMM_GUARD_FAULT;return false;
  }
  reserved_guard.state=VMM_GUARD_ACTIVE;
  reserved_guard.register_alias=0;
 }
 reserved_guard.io_borrower=NULL;
 return true;
}
bool vmm_reserved_guard_pin_register(const struct vmm_reserved_guard *g,uint64_t root,uint64_t aperture,
 uint64_t reg,uint64_t alias,const void *child) {
 if(!child || !reg || !alias || reserved_guard.register_page!=reg || reserved_guard.io_borrower ||
    __atomic_load_n(&reserved_mutations,__ATOMIC_SEQ_CST) ||
    !vmm_reserved_guard_io_page_held(g,root,aperture))return false;
 /* Exclude recursive mutation or close during the full reverse walk. */
 reserved_guard.state=VMM_GUARD_CHECKING;
 bool ok=reserved_register_observe(root,reg,alias);
 if(!reserved_guard_identity(g) || reserved_guard.state!=VMM_GUARD_CHECKING ||
    alias_inventory_cr4()!=reserved_guard.cr4 || alias_inventory_efer()!=reserved_guard.efer ||
    (reserved_guard_flags()&512) || read_cr3()!=root) {
  reserved_guard.state=VMM_GUARD_FAULT;return false;
 }
 reserved_guard.state=VMM_GUARD_ACTIVE;
 if(!ok)return false;
 reserved_guard.register_alias=alias;
 reserved_guard.io_borrower=child;
 return true;
}
bool vmm_reserved_guard_register_pinned(const struct vmm_reserved_guard *g,uint64_t root,uint64_t aperture,
 uint64_t reg,uint64_t alias,const void *child) {
 return alias && alias==reserved_guard.register_alias && reg==reserved_guard.register_page &&
        vmm_reserved_guard_io_page_pinned(g,root,aperture,child);
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
