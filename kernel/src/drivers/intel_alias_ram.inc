#ifndef SRVROS_INTEL_ALIAS_RAM_INC
#define SRVROS_INTEL_ALIAS_RAM_INC
/* RAM in which the bounded CPU page-table inventory may follow a pointer. */
struct intel_alias_ram_catalog {
 struct vmm_alias_ram ranges[VMM_ALIAS_RAM_LIMIT];
 unsigned count;
 bool ready;
};
static bool intel_alias_ram_prepare(struct intel_alias_ram_catalog *catalog,
                                    const struct limine_memmap_response *m) {
 *catalog=(struct intel_alias_ram_catalog){0};
 if(!m || !m->entries || !m->entry_count || m->entry_count>4096)return false;
 for(uint64_t i=0;i<m->entry_count;i++) {
  const struct limine_memmap_entry *e=m->entries[i];
  if(!e || !e->length || e->base>UINT64_MAX-e->length)return false;
  if(e->type!=LIMINE_MEMMAP_USABLE && e->type!=LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE && e->type!=LIMINE_MEMMAP_EXECUTABLE_AND_MODULES)continue;
  if(catalog->count==VMM_ALIAS_RAM_LIMIT)return false;
  unsigned at=catalog->count++;
  while(at && catalog->ranges[at-1].base>e->base){catalog->ranges[at]=catalog->ranges[at-1];--at;}
  catalog->ranges[at]=(struct vmm_alias_ram){e->base,e->length};
 }
 for(unsigned i=1;i<catalog->count;i++)if(catalog->ranges[i].base<catalog->ranges[i-1].base+catalog->ranges[i-1].length)return false;
 catalog->ready=catalog->count!=0;
 return catalog->ready;
}
#endif
