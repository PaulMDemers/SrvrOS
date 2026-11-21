/* CPU0 diagnostic only. Never imported as a hardware ownership assurance. */
static uint64_t alias_inventory_cr4(void) {
#ifdef SRVROS_VMM_HOST_TEST
 return vmm_test_cr4();
#else
 uint64_t value;__asm__ volatile("mov %%cr4,%0" : "=r"(value) : : "memory");return value;
#endif
}
static uint64_t alias_inventory_efer(void) {
#ifdef SRVROS_VMM_HOST_TEST
 return vmm_test_efer();
#else
 uint32_t lo,hi;__asm__ volatile("rdmsr" : "=a"(lo),"=d"(hi) : "c"(0xc0000080u) : "memory");return ((uint64_t)hi<<32)|lo;
#endif
}
static void alias_inventory_cpuid(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
#ifdef SRVROS_VMM_HOST_TEST
 vmm_test_cpuid(leaf,a,b,c,d);
#else
 __asm__ volatile("cpuid" : "=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d) : "a"(leaf),"c"(0) : "memory");
#endif
}
static bool alias_inventory_fail(struct vmm_alias_detail *d,unsigned reason,unsigned level) {
 if(d){d->reason=reason;d->level=level;}return false;
}
static bool alias_inventory_ram(const struct vmm_alias_ram *ram,unsigned count,uint64_t pa,uint64_t bytes) {
 if(pa>UINT64_MAX-bytes)return false;
 for(unsigned i=0;i<count;i++)if(pa>=ram[i].base && pa+bytes<=ram[i].base+ram[i].length)return true;
 return false;
}
enum alias_target { ALIAS_PRIVATE, ALIAS_ABSENT, ALIAS_REGISTER, ALIAS_REGISTER_RO };
static bool alias_inventory_walk(uint64_t root,const struct vmm_alias_span *spans,unsigned count,unsigned bits,
 const struct vmm_alias_ram *ram,unsigned ram_count,struct vmm_alias_inventory *out,struct vmm_alias_detail *d,enum alias_target target,struct vmm_unmapped_detail *conflict) {
 bool absent=target==ALIAS_ABSENT;
 struct vmm_alias_inventory result={0};struct vmm_alias_span expected[VMM_ALIAS_SPANS];
 bool found=false;struct vmm_unmapped_detail first={0};
 if(d)*d=(struct vmm_alias_detail){0};
 if(!out || !spans || !count || count>VMM_ALIAS_SPANS || !ram || !ram_count || ram_count>VMM_ALIAS_RAM_LIMIT || bits<36 || bits>52)
  return alias_inventory_fail(d,VMM_ALIAS_INPUT,0);
 uint64_t limit=1ull<<bits,allowed=(limit-1)&PAGE_MASK,previous=0;
 for(unsigned i=0;i<ram_count;i++) {
  if(!ram[i].length || ram[i].base>=limit || ram[i].length>limit-ram[i].base || (i && ram[i].base<previous))
   return alias_inventory_fail(d,VMM_ALIAS_INPUT,0);
  previous=ram[i].base+ram[i].length;
 }
 for(unsigned i=0;i<count;i++) {
  expected[i]=spans[i];const struct vmm_alias_span *q=&expected[i];
  if(!q->pages || q->pages>4096 || (q->physical&4095) || q->physical>=limit || (uint64_t)q->pages*4096>limit-q->physical ||
     (!absent && ((q->virtual_base&4095) || q->virtual_base>UINT64_MAX-(uint64_t)(q->pages-1)*4096 || !is_canonical(q->virtual_base) ||
     !is_canonical(q->virtual_base+(uint64_t)(q->pages-1)*4096) ||
     (q->virtual_base>>47)!=((q->virtual_base+(uint64_t)(q->pages-1)*4096)>>47))))return alias_inventory_fail(d,VMM_ALIAS_INPUT,0);
  if(target==ALIAS_PRIVATE && !alias_inventory_ram(ram,ram_count,q->physical,(uint64_t)q->pages*4096))return alias_inventory_fail(d,VMM_ALIAS_RAM,0);
  for(unsigned j=0;j<i;j++) {
   const struct vmm_alias_span *p=&expected[j];
   if((q->physical<p->physical+(uint64_t)p->pages*4096 && p->physical<q->physical+(uint64_t)q->pages*4096) ||
      (!absent && q->virtual_base<=p->virtual_base+(uint64_t)(p->pages-1)*4096 && p->virtual_base<=q->virtual_base+(uint64_t)(q->pages-1)*4096))
    return alias_inventory_fail(d,VMM_ALIAS_INPUT,0);
  }
 }
 if(!alias_inventory_ram(ram,ram_count,root,4096))return alias_inventory_fail(d,VMM_ALIAS_RAM,1);
 for(unsigned i=0;i<count;i++)if(root>=expected[i].physical && root<expected[i].physical+(uint64_t)expected[i].pages*4096)
  return alias_inventory_fail(d,VMM_ALIAS_TABLE_BACKING,1);
 struct vmm_range_observation root_detail={0};
 if(!diagnostic_sole_root(root,&root_detail,1,0)){if(d)d->root=root_detail;return alias_inventory_fail(d,VMM_ALIAS_ROOT,1);}
 uint64_t mode=alias_inventory_cr4(),efer=alias_inventory_efer();
 uint32_t ca,cb,cc,cd;alias_inventory_cpuid(0x80000000u,&ca,&cb,&cc,&cd);
 if(ca<0x80000008u || !(efer&(1ull<<10)))return alias_inventory_fail(d,VMM_ALIAS_MODE,0);
 alias_inventory_cpuid(0x80000008u,&ca,&cb,&cc,&cd);
 if((ca&255)!=bits)return alias_inventory_fail(d,VMM_ALIAS_MODE,0);
 alias_inventory_cpuid(0x80000001u,&ca,&cb,&cc,&cd);
 bool nx=(cd&(1u<<20)) && (efer&(1ull<<11)),page1g=cd&(1u<<26);
 const uint64_t refused=(1ull<<12)|(1ull<<17)|(1ull<<22)|(1ull<<24);
 if(!(mode&(1ull<<5)) || (mode&refused))return alias_inventory_fail(d,VMM_ALIAS_MODE,0);
 struct alias_frame {const volatile uint64_t *table;uint64_t physical,prefix;unsigned index;} stack[4];
 stack[0]=(struct alias_frame){kernel_pml4,root,0,0};unsigned depth=0;result.tables=1;
 for(;;) {
  struct alias_frame *f=&stack[depth];unsigned level=depth+1;
  if(f->index==512){if(!depth)break;--depth;continue;}
  unsigned index=f->index++;uint64_t entry=f->table[index];++result.entries;
  if(!(entry&VMM_PAGE_PRESENT))continue;
  if((!nx && (entry&VMM_PAGE_NO_EXECUTE)) || (entry&~(allowed|4095ull|VMM_PAGE_NO_EXECUTE)))return alias_inventory_fail(d,VMM_ALIAS_RESERVED,level);
  uint64_t prefix=f->prefix|((uint64_t)index<<(39-9*depth));
  bool leaf=level==4 || (entry&PAGE_LARGE);
  if(level==1 && leaf)return alias_inventory_fail(d,VMM_ALIAS_RESERVED,level);
  if(!leaf) {
   uint64_t physical=entry&PAGE_MASK;
   if(!alias_inventory_ram(ram,ram_count,physical,4096))return alias_inventory_fail(d,VMM_ALIAS_RAM,level+1);
   for(unsigned i=0;i<count;i++)if(physical>=expected[i].physical && physical<expected[i].physical+(uint64_t)expected[i].pages*4096)
    return alias_inventory_fail(d,VMM_ALIAS_TABLE_BACKING,level+1);
   for(unsigned i=0;i<=depth;i++)if(stack[i].physical==physical)return alias_inventory_fail(d,VMM_ALIAS_CYCLE,level+1);
   if(result.tables==VMM_ALIAS_TABLE_LIMIT)return alias_inventory_fail(d,VMM_ALIAS_LIMIT,level+1);
   const uint64_t *table=entry_table(entry);
   if(!table || !is_canonical((uint64_t)table))return alias_inventory_fail(d,VMM_ALIAS_TABLE,level+1);
   ++result.tables;++depth;stack[depth]=(struct alias_frame){table,physical,prefix,0};continue;
  }
  if(level==2 && !page1g)return alias_inventory_fail(d,VMM_ALIAS_RESERVED,level);
  uint64_t size=level==2?0x40000000ull:level==3?0x200000ull:4096;
  if(level<4 && (entry&((size-1)&~0x1fffull)))return alias_inventory_fail(d,VMM_ALIAS_RESERVED,level);
  uint64_t physical=(entry&PAGE_MASK)&~(size-1);
  if(physical>limit-size)return alias_inventory_fail(d,VMM_ALIAS_RESERVED,level);
  uint64_t va=prefix;if(va&(1ull<<47))va|=0xffff000000000000ull;
  ++result.leaves;
  for(unsigned i=0;i<count;i++) {
   const struct vmm_alias_span *q=&expected[i];uint64_t lo=physical>q->physical?physical:q->physical;
   uint64_t end=physical+size,qend=q->physical+(uint64_t)q->pages*4096;if(end>qend)end=qend;
   if(lo>=end)continue;
   if(absent) {
    first=(struct vmm_unmapped_detail){.prefix=result,.virtual_base=va,.physical=physical,.bytes=size,.entry=entry,.span=i};
    first.walk.level=level;found=true;goto finish;
   }
   /* Each device register must have the requested write permission on a supervisor-only, NX
    * 4K leaf with PAT index 2. Check the leaf itself as well as effective
    * permissions so a later sibling map cannot expose it to user mode. */
   uint64_t reg_flags=VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE;
   if(target==ALIAS_REGISTER)reg_flags|=VMM_PAGE_WRITABLE;
   if((target==ALIAS_REGISTER || target==ALIAS_REGISTER_RO) && (level!=4 ||
      (entry & (VMM_PAGE_WRITABLE | VMM_PAGE_USER | VMM_PAGE_NO_EXECUTE | 0x98u))!=reg_flags))
    return alias_inventory_fail(d,VMM_ALIAS_FOREIGN,level);
   if(va+(lo-physical)!=q->virtual_base+(lo-q->physical))return alias_inventory_fail(d,VMM_ALIAS_FOREIGN,level);
   uint64_t pages=(end-lo)/4096;
   if(pages>q->pages-result.pages[i])return alias_inventory_fail(d,VMM_ALIAS_COVERAGE,level);
   result.pages[i]+=(uint32_t)pages;++result.matches[i];
  }
 }
finish:
 for(unsigned i=0;!absent && i<count;i++)if(result.pages[i]!=expected[i].pages || !result.matches[i])return alias_inventory_fail(d,VMM_ALIAS_COVERAGE,0);
 if(!diagnostic_sole_root(root,&root_detail,3,result.entries)){if(d)d->root=root_detail;return alias_inventory_fail(d,VMM_ALIAS_ROOT,3);}
 if(alias_inventory_cr4()!=mode || alias_inventory_efer()!=efer)return alias_inventory_fail(d,VMM_ALIAS_MODE,0);
 uint64_t final_flags;
#ifdef SRVROS_VMM_HOST_TEST
 final_flags=vmm_test_rflags();
#else
 __asm__ volatile("pushfq; popq %0" : "=r"(final_flags) : : "memory");
#endif
 /* No pointer conversion/callback follows these terminal context reads. */
 if((final_flags&512) || read_cr3()!=root)return alias_inventory_fail(d,VMM_ALIAS_ROOT,3);
 if(found) {
  if(conflict)*conflict=first;
  return alias_inventory_fail(d,VMM_ALIAS_PRESENT,first.walk.level);
 }
 *out=result;return true;
}

bool vmm_diagnostic_readonly_register_aliases(uint64_t root,const struct vmm_alias_span *spans,
 unsigned count,unsigned bits,const struct vmm_alias_ram *ram,unsigned ram_count) {
 struct vmm_alias_inventory inventory;
 return alias_inventory_walk(root,spans,count,bits,ram,ram_count,&inventory,NULL,ALIAS_REGISTER_RO,NULL);
}

bool vmm_diagnostic_unique_kernel_aliases(uint64_t root,const struct vmm_alias_span *spans,unsigned count,unsigned bits,
 const struct vmm_alias_ram *ram,unsigned ram_count,struct vmm_alias_inventory *out,struct vmm_alias_detail *d) {
 return alias_inventory_walk(root,spans,count,bits,ram,ram_count,out,d,ALIAS_PRIVATE,NULL);
}
bool vmm_diagnostic_kernel_physical_unmapped(uint64_t root,const struct vmm_physical_span *spans,unsigned count,unsigned bits,
 const struct vmm_alias_ram *ram,unsigned ram_count,struct vmm_alias_inventory *out,struct vmm_unmapped_detail *d) {
 struct vmm_alias_span query[VMM_ALIAS_SPANS]={0};struct vmm_alias_detail detail={0};
 if(d)*d=(struct vmm_unmapped_detail){0};
 if(!spans || !count || count>VMM_ALIAS_SPANS) {
  if(d)d->walk.reason=VMM_ALIAS_INPUT;
  return false;
 }
 for(unsigned i=0;i<count;i++)query[i]=(struct vmm_alias_span){.physical=spans[i].physical,.pages=spans[i].pages};
 bool ok=alias_inventory_walk(root,query,count,bits,ram,ram_count,out,&detail,ALIAS_ABSENT,d);
 if(d)d->walk=detail;
 return ok;
}
