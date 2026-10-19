#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../ports/srvros/uacpi/runtime/sci_test.c"
static struct pci_device devices[2];
static struct acpi_fadt table;
static struct acpi_table_info table_info;
uint64_t pci_device_count(void) { return 2; }
const struct pci_device *pci_device_at(uint64_t i) { return &devices[i]; }
uint64_t acpi_table_count(void) { return 1; }
const struct acpi_table_info *acpi_table_at(uint64_t i) { assert(!i); return &table_info; }
void *pmm_phys_to_virt(uint64_t address) { assert(address == 0x1234000); return &table; }
int main(void) {
    devices[0]=(struct pci_device){.vendor_id=0x8086,.device_id=0x29c0};
    devices[1]=(struct pci_device){.vendor_id=0x8086,.device_id=0x2918,.device=31};
    table_info=(struct acpi_table_info){.signature="FACP",.length=sizeof(table),.physical_address=0x1234000};
    memcpy(table.hdr.oemid,"BOCHS ",6);
    table.sci_int=9; table.pm1a_evt_blk=0x600; table.pm1a_cnt_blk=0x604;
    table.pm1_evt_len=4; table.pm1_cnt_len=2; table.smi_cmd=0xb2;
    table.acpi_enable=0xf1; table.acpi_disable=0xf0;
    table.gpe0_blk=0x620; table.gpe0_blk_len=16;
    assert(q35_fadt()==&table);
    devices[0].device_id=0x1604; assert(!q35_fadt()); devices[0].device_id=0x29c0;
    devices[1].device_id=0x9cc3; assert(!q35_fadt()); devices[1].device_id=0x2918;
    table.hdr.oemid[0]='A'; assert(!q35_fadt()); table.hdr.oemid[0]='B';
    table_info.length=115; assert(!q35_fadt()); table_info.length=sizeof(table);
#define REJECT(field, value) do { typeof(table.field) saved=table.field; table.field=(value); assert(!q35_fadt()); table.field=saved; } while(0)
    REJECT(sci_int, 10); REJECT(pm1a_evt_blk, 0x1800); REJECT(pm1a_cnt_blk, 0x1804);
    REJECT(pm1b_evt_blk, 0x700); REJECT(pm1b_cnt_blk, 0x704);
    REJECT(pm1_evt_len, 8); REJECT(pm1_cnt_len, 4); REJECT(smi_cmd, 0xb3);
    REJECT(acpi_enable, 0); REJECT(acpi_disable, 0);
    REJECT(gpe0_blk, 0x1820); REJECT(gpe0_blk_len, 32); REJECT(gpe1_blk, 0x720);
    assert(q35_fadt()==&table);
    puts("sci-gate: PASS Q35 identity/firmware/register contract; physical and mismatched profiles refused before I/O");
}
