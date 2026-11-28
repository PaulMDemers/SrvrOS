/* Only the Air7,2 full-GT diagnostic consumes this same-boot EFI record. */
#include <srvros/air_gpu_boot.h>
static struct air_gpu_boot air_boot;
static struct air_gpu_boot air_boot_candidate;
static bool air_boot_buffer;
static const char *air_import_status;
static unsigned air_oem_tables;
/* Limine base revision 6 supplies the system table as an HHDM pointer.
 * The pointers stored by firmware inside that table are physical. */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_efi_system_table_request air_efi_request = {
    .id=LIMINE_EFI_SYSTEM_TABLE_REQUEST_ID
};
struct air_efi_system {
    uint64_t signature;
    uint32_t revision,header_size;
    uint8_t unused[88];
    uint64_t count,entries;
};
struct air_efi_entry { uint64_t guid_lo,guid_hi,address; };
_Static_assert(__builtin_offsetof(struct air_efi_system,count)==104,"EFI x64 layout");
_Static_assert(sizeof(struct air_gpu_boot)<=4096,"EFI configuration record fits in its NVS page");
static void entry_requester_base64(const uint8_t *,unsigned,char *);

static bool air_span(uint64_t base,uint64_t bytes,uint64_t type)
{
    const struct limine_memmap_response *m=memmap_request.response;
    if (!bytes || base>UINT64_MAX-bytes || !m || !m->entries || m->entry_count>4096) return false;
    unsigned matches=0;
    for (uint64_t i=0;i<m->entry_count;i++) {
        const struct limine_memmap_entry *e=m->entries[i];
        if (!e || !e->length || e->base>UINT64_MAX-e->length) return false;
        if (base+bytes<=e->base || base>=e->base+e->length) continue;
        if (e->type!=type || e->base>base || e->base+e->length<base+bytes) return false;
        ++matches;
    }
    return matches==1;
}
static bool air_boot_range(uint64_t base,uint64_t bytes)
{
    return base>=0x100000 && !(base&4095) && bytes==AIR_BOOT_FRAMES*4096 &&
        base<=0x100000000ull-bytes && air_span(base,bytes,LIMINE_MEMMAP_ACPI_NVS);
}
static const void *air_read_span(uint64_t pa,uint64_t size,uint64_t type)
{
    if (!air_span(pa,size,type)) return NULL;
    for (uint64_t page=pa&~4095ull;page<=((pa+size-1)&~4095ull);page+=4096) {
        uint64_t actual;
        if (!vmm_virt_to_phys((uintptr_t)pmm_phys_to_virt(page),&actual) || actual!=page) return NULL;
        if (page==((pa+size-1)&~4095ull)) break;
    }
    return pmm_phys_to_virt(pa);
}
static bool air_config_record(uint64_t *address)
{
    const struct limine_efi_system_table_response *efi=air_efi_request.response;
    if (!efi) return true;
    uint64_t pa;
    if (!efi->address || !vmm_virt_to_phys((uintptr_t)efi->address,&pa) ||
        pmm_phys_to_virt(pa)!=efi->address || (pa&7)) return false;
    const struct air_efi_system *st=air_read_span(pa,sizeof(*st),LIMINE_MEMMAP_RESERVED_MAPPED);
    if (!st || st->signature!=0x5453595320494249ull || st->header_size<sizeof(*st) ||
        st->count>1024) return false;
    if (!st->count) return true;
    if (!st->entries || (st->entries&7)) return false;
    const struct air_efi_entry *entries=air_read_span(st->entries,st->count*sizeof(*entries),
                                                       LIMINE_MEMMAP_RESERVED_MAPPED);
    if (!entries) return false;
    for (uint64_t i=0;i<st->count;i++) {
        if (entries[i].guid_lo!=AIR_BOOT_GUID_LO || entries[i].guid_hi!=AIR_BOOT_GUID_HI) continue;
        if (*address || !entries[i].address || (entries[i].address&4095)) return false;
        *address=entries[i].address;
    }
    return true;
}
static bool air_boot_reserved(void)
{
    if (!air_boot_range(air_boot.ram,AIR_BOOT_FRAMES*4096)) return false;
    for (unsigned i=0;i<AIR_BOOT_FRAMES;i++) {
        uint64_t actual,pa=air_boot.ram+i*4096;
        if (!pmm_frame_has_tag(pa,PMM_FRAME_RESERVED) ||
            !vmm_virt_to_phys((uint64_t)pmm_phys_to_virt(pa),&actual) || actual!=pa) return false;
    }
    return true;
}
static bool air_boot_pristine(void)
{
    const uint64_t *ram=pmm_phys_to_virt(air_boot.ram);
    for (unsigned i=0;i<AIR_BOOT_FRAMES*512;i++) {
        uint64_t expected=i%512 ? 0 : AIR_BOOT_MARKER^air_boot.ram^(i/512);
        if (ram[i]!=expected) return false;
    }
    return true;
}
static bool air_valid_record(const struct air_gpu_boot *r)
{
    uint8_t sum=0;
    const size_t legacy=__builtin_offsetof(struct air_gpu_boot,display_branch);
    const size_t routes=__builtin_offsetof(struct air_gpu_boot,protocol_counts);
    if (!((r->oem_revision==3 && r->length==sizeof(*r)) ||
          (r->oem_revision==2 && r->length==routes) ||
          (r->oem_revision==1 && r->length==legacy))) return false;
    if (r->oem_revision==3) {
        for (unsigned role=0;role<AIR_BOOT_PROTOCOL_ROLES;role++) {
            if (r->protocol_counts[role]>AIR_BOOT_PROTOCOL_LIMIT ||
                (r->protocol_counts[role] && (!r->display_branch || r->state!=AIR_BOOT_CAPTURED)))
                return false;
        }
    }
    for (unsigned j=0;j<r->length;j++) sum+=((const uint8_t *)r)[j];
    return !sum && !__builtin_memcmp(r->signature,"OEMG",4) &&
        !__builtin_memcmp(r->oem,"SRVROS",6) && !__builtin_memcmp(r->table_id,"BDWBOOT1",8) &&
        r->step<=6 && (r->state!=AIR_BOOT_CAPTURED || r->step==6) &&
        r->revision==1 && r->state<=AIR_BOOT_CAPTURED &&
        r->first==AIR_BOOT_SLOT && r->protocols<=32 && r->firmware_files<=4 && r->firmware_files<=r->driver_opens;
}
static const char *air_transport;
static void air_boot_import(void)
{
    const struct air_gpu_boot *candidate=NULL;
    air_oem_tables=0;
    air_transport="none";
    air_import_status="absent";
    air_boot=(struct air_gpu_boot){0};
    air_boot_buffer=false;
    uint64_t config=0;
    if (!air_config_record(&config)) { air_import_status="configuration-directory"; return; }
    if (config) {
        air_transport="uefi";
        air_import_status="configuration-record";
        const struct air_gpu_boot *record=air_read_span(config,sizeof(*candidate),LIMINE_MEMMAP_ACPI_NVS);
        if (!record || !pmm_frame_has_tag(config,PMM_FRAME_RESERVED) || config<AIR_BOOT_FRAMES*4096) return;
        __builtin_memcpy(&air_boot_candidate,record,sizeof(air_boot_candidate));
        if (!air_valid_record(&air_boot_candidate) || air_boot_candidate.ram!=config-AIR_BOOT_FRAMES*4096) return;
        candidate=&air_boot_candidate;
    }
    for (uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if (!t || __builtin_memcmp(t->signature,"OEMG",4)) continue;
        ++air_oem_tables;
        if (t->length!=sizeof(air_boot) && t->length!=__builtin_offsetof(struct air_gpu_boot,display_branch) &&
            t->length!=__builtin_offsetof(struct air_gpu_boot,protocol_counts)) continue;
        const uint8_t *record=pmm_phys_to_virt(t->physical_address);
        uint32_t length;
        __builtin_memcpy(&length,record+__builtin_offsetof(struct air_gpu_boot,length),sizeof(length));
        if (length!=t->length) continue;
        if (__builtin_memcmp(record+__builtin_offsetof(struct air_gpu_boot,oem),"SRVROS",6) ||
            __builtin_memcmp(record+__builtin_offsetof(struct air_gpu_boot,table_id),"BDWBOOT1",8)) continue;
        if (candidate) { air_import_status="duplicate"; return; }
        air_transport="acpi";
        air_import_status="invalid";
        __builtin_memcpy(&air_boot_candidate,record,t->length);
        if (air_boot_candidate.length!=t->length || !air_valid_record(&air_boot_candidate)) return;
        candidate=&air_boot_candidate;
    }
    if (!candidate) return;
    __builtin_memcpy(&air_boot,candidate,candidate->length);
    air_import_status="reservation";
    if (!air_boot_reserved()) return;
    air_boot_buffer=air_boot_pristine();
    air_import_status=air_boot_buffer?"ready":"contents";
}
static bool air_branch_selected(void)
{
    uint32_t selected=(uint32_t)air_boot.display_branch;
    unsigned child=selected&255, gop=selected>>8;
    return air_boot_buffer && air_boot.oem_revision>=2 && air_boot.state==AIR_BOOT_CAPTURED &&
        child>=1 && child<=16 && gop>=1 && gop<=16;
}
static bool air_boot_display(uint64_t mmio,uint64_t aperture)
{
    if (!air_branch_selected() || air_boot.pci_id!=result.after.id ||
        air_boot.subsystem!=result.after.subsystem || air_boot.mmio!=mmio || air_boot.aperture!=aperture ||
        air_boot.framebuffer!=result.fb.physical || !result.fb.pitch || !result.fb.height ||
        result.fb.height>air_boot.framebuffer_bytes/result.fb.pitch ||
        !(air_boot.plane&(1u<<31)) || air_boot.surface>=4096ull*AIR_BOOT_SLOT ||
        air_boot.framebuffer_bytes>4096ull*AIR_BOOT_SLOT-air_boot.surface ||
        air_boot.framebuffer<aperture || air_boot.framebuffer-aperture!=air_boot.surface) return false;
    for (unsigned n=0;n<2;n++) {
        const uint32_t *plane=result.sample[n].pipe[0];
        if (air_boot.plane!=plane[2] || air_boot.surface!=plane[6] || (plane[4]&~8u)!=air_boot.surface)
            return false;
    }
    return true;
}
static bool air_boot_matches(uint64_t mmio,uint64_t aperture,const uint64_t *words)
{
    if (!air_boot_display(mmio,aperture)) return false;
    for (unsigned i=0;i<AIR_BOOT_PAGES;i++)
        if (!(words[i]&1) || air_boot.entries[i]!=words[i]) return false;
    return true;
}

static void air_boot_inventory_report(void)
{
    if (air_boot.oem_revision!=3 || !air_branch_selected()) return;
    console_printf("intel-efi-inventory: v=1 branch=%x gop=%u child=%u pci_driver=%u eg2_driver=%u\n",
        air_boot.display_branch,(uint64_t)air_boot.protocol_counts[AIR_BOOT_GOP],
        (uint64_t)air_boot.protocol_counts[AIR_BOOT_GOP_CHILD],
        (uint64_t)air_boot.protocol_counts[AIR_BOOT_PCI_DRIVER],
        (uint64_t)air_boot.protocol_counts[AIR_BOOT_EG2_DRIVER]);
    for (unsigned role=0;role<AIR_BOOT_PROTOCOL_ROLES;role++) {
        unsigned n=air_boot.protocol_counts[role];
        if (!n) continue;
        char encoded[4*((AIR_BOOT_PROTOCOL_LIMIT*16+2)/3)+1];
        entry_requester_base64((const uint8_t *)&air_boot.handle_protocol_ids[role],n*16,encoded);
        console_printf("intel-efi-handle-guids: v=1 role=%u data=%s\n",(uint64_t)role,encoded);
    }
}

static void air_boot_report(void)
{
    console_printf("intel-efi-handoff: v=1 table=%u state=%u step=%u backing=%u drivers=%u children=%u protocols=%u files=%u ram=%x grants=0\n",
        (uint64_t)(air_boot.revision==1),(uint64_t)air_boot.state,(uint64_t)air_boot.step,(uint64_t)air_boot_buffer,
        (uint64_t)air_boot.driver_opens,(uint64_t)air_boot.child_opens,(uint64_t)air_boot.protocols,
        (uint64_t)air_boot.firmware_files,air_boot.ram);
    for (unsigned kind=0;kind<2;kind++) {
        unsigned count=kind?air_boot.firmware_files:air_boot.protocols;
        for (unsigned i=0;i<count;i++) {
            const uint8_t *id=kind?air_boot.driver_file_ids[i]:air_boot.protocol_ids[i];
            uint64_t lo,hi;
            __builtin_memcpy(&lo,id,8);
            __builtin_memcpy(&hi,id+8,8);
            console_printf("intel-efi-guid: v=1 kind=%u index=%u low=%x high=%x\n",(uint64_t)kind,(uint64_t)i,lo,hi);
        }
    }
    if (air_boot.state!=AIR_BOOT_CAPTURED) return;
    console_printf("intel-efi-display: v=1 id=%x subsystem=%x mmio=%x aperture=%x framebuffer=%x bytes=%u plane=%x surface=%x\n",
        (uint64_t)air_boot.pci_id,(uint64_t)air_boot.subsystem,air_boot.mmio,air_boot.aperture,air_boot.framebuffer,
        air_boot.framebuffer_bytes,(uint64_t)air_boot.plane,(uint64_t)air_boot.surface);
    for (unsigned i=0;i<AIR_BOOT_PAGES;i++)
        console_printf("intel-efi-entry: v=1 slot=%u pte=%x\n",(uint64_t)AIR_BOOT_SLOT+i,air_boot.entries[i]);
}
