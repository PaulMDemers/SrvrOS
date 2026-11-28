#include <srvros/bcm4331_probe.h>
#include <srvros/console.h>
#include <srvros/display_profile.h>
#include <srvros/pci.h>
#include <srvros/vmm.h>

enum {
    PCI_COMMAND = 0x04, PCI_MEMORY = 2, PCI_BUS_MASTER = 4,
    PCI_SUBSYSTEM = 0x2c, PCI_PM_CAP = 0x40, PCI_PMCSR = 0x44,
    PCI_BAR0_WINDOW = 0x80,
    PAGE_BYTES = 4096, BAR_BYTES = 4 * PAGE_BYTES, CHIPCOMMON_PAGE = 3 * PAGE_BYTES,
    CHIP_ID = 0x00, CHIP_CAP = 0x04, CHIP_EROM = 0xfc, CHIP_SROM_CONTROL = 0x190,
    EROM_VALID = 1, EROM_ADDR_TAG = 0x06, EROM_END = 0x0f,
    EROM_MASTER_PORT = 3, EROM_ADDRESS = 5, EROM_ADDR_SIZE = 0x30,
    EROM_SIZE_DESCRIPTOR = 0x30, EROM_ADDR_64 = 8,
    EROM_ADDR_TYPE = 0xc0, EROM_ADDR_PORT = 0xf00, EROM_SLAVE_WRAP = 0x80, EROM_MASTER_WRAP = 0xc0,
};

static const uint64_t bcm4331_window = 0xffffc001f8000000ull;

struct bcma_core {
    uint32_t address;
    uint32_t wrapper;
};

/* Match only the three Linux-enumerated cores on this board. BCMA addresses
 * are bus addresses, used only as selector values and never as host pointers. */
static const struct {
    unsigned id, revision;
} captured_cores[] = { {0x800, 0x25}, {0x812, 0x1d}, {0x820, 0x13} };

static bool read_erom(const volatile uint32_t *words, unsigned count, struct bcma_core cores[3]) {
    unsigned index = 0, found = 0;
    while (index < count) {
        uint32_t cia = words[index++];
        if (cia == EROM_END) return found == 7 && cores[0].address == 0x18000000 &&
            cores[1].address == 0x18001000 && cores[2].address != cores[0].address &&
            cores[2].address != cores[1].address;
        if ((cia & 0xf) != EROM_VALID || index == count) return false;
        uint32_t cib = words[index++];
        if ((cib & 0xf) != EROM_VALID) return false;

        unsigned id = (cia >> 8) & 0xfff, revision = cib >> 24;
        struct bcma_core core = {0};
        uint32_t slave_wrapper = 0;
        while (index < count) {
            uint32_t word = words[index];
            if ((word & 0xf) == EROM_VALID || word == EROM_END) break;
            if (!(word & EROM_VALID) || word == UINT32_MAX) return false;
            index++;
            if ((word & 0xf) == EROM_MASTER_PORT) continue;
            if ((word & (EROM_ADDR_TAG | EROM_VALID)) != EROM_ADDRESS) return false;
            /* This 32-bit backplane may still encode extended size records.
             * Consume their operand words before inspecting another tag. */
            bool four_kib = !(word & EROM_ADDR_SIZE);
            if (word & EROM_ADDR_64) {
                if (index == count || words[index++] != 0) return false;
            }
            if ((word & EROM_ADDR_SIZE) == EROM_SIZE_DESCRIPTOR) {
                if (index == count) return false;
                uint32_t size = words[index++];
                four_kib = (size & 0xfffff000) == PAGE_BYTES;
                if ((size & EROM_ADDR_64) && (index == count || words[index++] != 0)) return false;
            }
            uint32_t address = word & 0xfffff000;
            unsigned type = word & EROM_ADDR_TYPE;
            if (type == 0 && !core.address && !(word & EROM_ADDR_PORT) && four_kib) core.address = address;
            if (type == EROM_MASTER_WRAP && !core.wrapper && four_kib) core.wrapper = address;
            if (type == EROM_SLAVE_WRAP && !slave_wrapper && four_kib) slave_wrapper = address;
        }
        if (!core.wrapper) core.wrapper = slave_wrapper;
        for (unsigned i = 0; i < sizeof captured_cores / sizeof captured_cores[0]; i++) {
            if (id != captured_cores[i].id) continue;
            if ((found & (1u << i)) || revision != captured_cores[i].revision || (cia >> 20) != 0x4bf ||
                (cia & 0xf0) || (core.address & 0xfff00000) != 0x18000000 ||
                (core.wrapper & 0xfff00000) != 0x18100000) return false;
            cores[i] = core;
            found |= 1u << i;
        }
    }
    return false;
}

static const struct pci_device *bcm4331_controller(void) {
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *p = pci_device_at(i);
        if (p->bus == 3 && !p->device && !p->function && p->vendor_id == 0x14e4 &&
            p->device_id == 0x4331 && p->class_code == 2 && p->subclass == 0x80 && p->revision == 2 &&
            p->header_type == 0 && pci_read_config32(p, PCI_SUBSYSTEM) == 0x433114e4) return p;
    }
    return 0;
}

enum bcm4331_probe_result bcm4331_probe(void) {
    if (!display_profile_is_pro81()) {
        console_write("bcm4331: refused platform identity\n");
        return BCM4331_REFUSED;
    }
    const struct pci_device *p = bcm4331_controller();
    uint64_t root = vmm_kernel_address_space(), occupied;
    const uint64_t fixed = bcm4331_window + CHIPCOMMON_PAGE;
    if (!p || !vmm_diagnostic_sole_kernel_root(root) ||
        vmm_virt_to_phys_in_space(root, bcm4331_window, &occupied) || vmm_virt_to_phys_in_space(root, fixed, &occupied)) {
        console_write("bcm4331: refused PCI identity or mapping context\n");
        return BCM4331_REFUSED;
    }

    uint16_t command = pci_read_config16(p, PCI_COMMAND), pm = pci_read_config16(p, PCI_PMCSR);
    uint32_t window = pci_read_config32(p, PCI_BAR0_WINDOW);
    uint64_t bar = p->bar[0] & ~15ull;
    console_printf("bcm4331: pci=03:00.0 device=14e4:4331 rev=02 bar0=%x command=%x pm=%x window=%x\n",
        bar, (uint64_t)command, (uint64_t)pm, (uint64_t)window);
    if (!bar || p->bar[1] || (p->bar[0] & (BAR_BYTES - 1)) != 4 ||
        pci_read_config8(p, PCI_PM_CAP) != 1 || pm == UINT16_MAX || (pm & 3) ||
        command == UINT16_MAX || window == UINT32_MAX ||
        ((command & PCI_BUS_MASTER) && !(command & PCI_MEMORY))) {
        console_write("bcm4331: refused BAR layout or PCI power state\n");
        return BCM4331_REFUSED;
    }

    const uint64_t flags = VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE;
    if (!vmm_map_page_in_space(root, fixed, bar + CHIPCOMMON_PAGE, flags)) return BCM4331_INCOMPLETE;
    uint16_t enabled = command | PCI_MEMORY;
    if (enabled != command) pci_write_config16(p, PCI_COMMAND, enabled);
    bool mapped = false, switched = false, matched = false;
    if (pci_read_config16(p, PCI_COMMAND) == enabled) {
        const volatile uint32_t *cc = (const volatile uint32_t *)fixed;
        uint32_t id = cc[CHIP_ID / 4], cap = cc[CHIP_CAP / 4], erom = cc[CHIP_EROM / 4];
        uint32_t srom = cc[CHIP_SROM_CONTROL / 4];
        console_printf("bcm4331: chip=%x capabilities=%x erom=%x srom_control=%x\n",
            (uint64_t)id, (uint64_t)cap, (uint64_t)erom, (uint64_t)srom);
        if (id == 0x13924331 && erom == 0x18107000 && !(command & PCI_BUS_MASTER)) {
            mapped = vmm_map_page_in_space(root, bcm4331_window, bar, flags);
            if (mapped) {
                pci_write_config32(p, PCI_BAR0_WINDOW, erom);
                switched = true;
                struct bcma_core cores[3] = {0};
                matched = pci_read_config32(p, PCI_BAR0_WINDOW) == erom &&
                    read_erom((const volatile uint32_t *)bcm4331_window, PAGE_BYTES / 4, cores);
                if (matched) {
                    for (unsigned i = 0; i < sizeof captured_cores / sizeof captured_cores[0]; i++)
                        console_printf("bcm4331: core=%x revision=%x address=%x wrapper=%x\n",
                            (uint64_t)captured_cores[i].id, (uint64_t)captured_cores[i].revision,
                            (uint64_t)cores[i].address, (uint64_t)cores[i].wrapper);
                } else console_write("bcm4331: enumeration ROM did not match the captured topology\n");
            }
        } else if (command & PCI_BUS_MASTER) {
            console_write("bcm4331: PCI bus mastering was set; fixed-window reads only\n");
        }
    }

    if (switched) pci_write_config32(p, PCI_BAR0_WINDOW, window);
    if (enabled != command) pci_write_config16(p, PCI_COMMAND, command);
    bool restored = pci_read_config32(p, PCI_BAR0_WINDOW) == window &&
        pci_read_config16(p, PCI_COMMAND) == command;
    uint64_t physical;
    bool unmapped = vmm_unmap_page_in_space(root, fixed, &physical) && physical == bar + CHIPCOMMON_PAGE;
    if (mapped) unmapped = (vmm_unmap_page_in_space(root, bcm4331_window, &physical) && physical == bar) && unmapped;
    console_printf("bcm4331: mapping=%s pci_restored=%u bus_master_changes=0 dma_pages=0 radio_writes=0\n",
        unmapped ? "released" : "unverified", (uint64_t)restored);
    if (!restored || !unmapped) return BCM4331_UNSAFE;
    return matched ? BCM4331_TOPOLOGY_OK : BCM4331_INCOMPLETE;
}
