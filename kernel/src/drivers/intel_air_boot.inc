/* Only the Air7,2 full-GT diagnostic consumes this same-boot EFI record. */
#include <srvros/air_gpu_boot.h>
static struct air_gpu_boot air_boot;
static bool air_boot_buffer;
static const char *air_import_status;
static unsigned air_oem_tables;
static bool air_boot_range(uint64_t base,uint64_t bytes)
{
    const struct limine_memmap_response *m=memmap_request.response;
    if (!m || !m->entries || m->entry_count>4096 || base<0x100000 || (base&4095) ||
        bytes!=AIR_BOOT_FRAMES*4096 || base>0x100000000ull-bytes) return false;
    unsigned matches=0;
    for (uint64_t i=0;i<m->entry_count;i++) {
        const struct limine_memmap_entry *e=m->entries[i];
        if (!e || !e->length || e->base>UINT64_MAX-e->length) return false;
        if (base+bytes<=e->base || base>=e->base+e->length) continue;
        if (e->type!=LIMINE_MEMMAP_ACPI_NVS || e->base>base || e->base+e->length<base+bytes) return false;
        ++matches;
    }
    return matches==1;
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
static void air_boot_import(void)
{
    struct air_gpu_boot candidate;
    bool found=false;
    air_oem_tables=0;
    air_import_status="absent";
    air_boot=(struct air_gpu_boot){0};
    air_boot_buffer=false;
    for (uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if (!t || __builtin_memcmp(t->signature,"OEMG",4)) continue;
        ++air_oem_tables;
        if (t->length!=sizeof(air_boot)) continue;
        struct air_gpu_boot record;
        __builtin_memcpy(&record,pmm_phys_to_virt(t->physical_address),sizeof(record));
        const struct air_gpu_boot *r=&record;
        if (__builtin_memcmp(r->oem,"SRVROS",6) || __builtin_memcmp(r->table_id,"BDWBOOT1",8)) continue;
        air_import_status=found?"duplicate":"invalid";
        uint8_t sum=0;
        for (unsigned j=0;j<sizeof(*r);j++) sum+=((const uint8_t *)r)[j];
        if (found || sum || r->length!=sizeof(*r) || __builtin_memcmp(r->signature,"OEMG",4) ||
            r->step>6 || (r->state==AIR_BOOT_CAPTURED && r->step!=6) || r->revision!=1 || r->oem_revision!=1 || r->state>AIR_BOOT_CAPTURED ||
            r->first!=AIR_BOOT_SLOT || r->protocols>32 || r->firmware_files>4 || r->firmware_files>r->driver_opens) return;
        candidate=record;
        found=true;
    }
    if (!found) return;
    air_boot=candidate;
    air_import_status="reservation";
    if (!air_boot_reserved()) return;
    air_boot_buffer=air_boot_pristine();
    air_import_status=air_boot_buffer?"ready":"contents";
}
static bool air_boot_display(uint64_t mmio,uint64_t aperture)
{
    if (!air_boot_buffer || air_boot.state!=AIR_BOOT_CAPTURED || air_boot.pci_id!=result.after.id ||
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
