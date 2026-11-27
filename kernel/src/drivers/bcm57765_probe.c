#include <srvros/bcm57765_probe.h>
#include <srvros/console.h>
#include <srvros/display_profile.h>
#include <srvros/pci.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>

enum {
    PCI_COMMAND = 0x04,
    PCI_MEMORY = 0x02,
    PCI_BUS_MASTER = 0x04,
    PCI_SUBSYSTEM = 0x2c,
    PCI_PM_CAP = 0x48,
    PCI_PM_CONTROL = 0x4c,
    PCI_ASIC_REV = 0xfc,
    PM_STATE = 0x0003,
    PM_EVENT_STATUS = 0x8000,
    MAC_MODE = 0x400,
    MAC_STATUS = 0x404,
    MAC_EVENT = 0x408,
    MAC_EVENT_MDIO = 0x00c00000,
    MDIO_COMMAND = 0x44c,
    MDIO_MODE = 0x454,
    MDIO_AUTO_POLL = 0x10,
    MDIO_BUSY = 0x20000000,
    MDIO_FAILED = 0x10000000,
    MDIO_READ = 0x08000000,
    MII_CONTROL = 0,
    MII_STATUS = 1,
    MII_ID_HIGH = 2,
    MII_ID_LOW = 3,
    MII_AUX_STATUS = 0x19,
    MII_LINK = 0x0004,
    MII_AUX_LINK = 0x0004,
};

static const uint64_t register_window = 0xffffc001f7000000ull;

static const struct pci_device *controller(void) {
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *p = pci_device_at(i);
        if (p->bus == 2 && !p->device && !p->function && p->vendor_id == 0x14e4 &&
            p->device_id == 0x16b4 && p->class_code == 2 && !p->subclass &&
            !p->prog_if && p->revision == 0x10 && !(p->header_type & 0x7f)) {
            uint32_t subsystem = pci_read_config32(p, PCI_SUBSYSTEM);
            uint32_t asic = pci_read_config32(p, PCI_ASIC_REV);
            uint16_t pm_cap = pci_read_config16(p, PCI_PM_CAP);
            console_printf("bcm57765: identity subsystem=%x asic_raw=%x pm_cap=%x\n",
                (uint64_t)subsystem, (uint64_t)asic, (uint64_t)pm_cap);
            if (subsystem == 0x16b414e4 && asic == 0x57785100 && (pm_cap & 0xff) == 1)
                return p;
        }
    }
    return 0;
}

static uint32_t mac_read(unsigned reg) {
    return *(volatile const uint32_t *)(register_window + reg);
}

static bool mdio_read(unsigned reg, uint16_t *data, bool *pending) {
    if (mac_read(MDIO_COMMAND) & MDIO_BUSY) return false;
    /* Clause 22 read on the captured PHY at address 1. No PHY register writes. */
    *(volatile uint32_t *)(register_window + MDIO_COMMAND) =
        MDIO_BUSY | MDIO_READ | (1u << 21) | (reg << 16);
    *pending = true;
    for (unsigned i = 0; i < 100; i++) {
        if (!timer_delay_us(10)) return false;
        uint32_t command = mac_read(MDIO_COMMAND);
        if (command & MDIO_BUSY) continue;
        *pending = false;
        if (command & MDIO_FAILED) return false;
        *data = (uint16_t)command;
        return true;
    }
    return false;
}

static enum bcm57765_probe_result read_link(bool *pending) {
    uint32_t mode = mac_read(MAC_MODE), status = mac_read(MAC_STATUS);
    uint32_t event = mac_read(MAC_EVENT), mdio_mode = mac_read(MDIO_MODE);
    console_printf("bcm57765: mac_mode=%x mac_status=%x mac_event=%x mdio_mode=%x\n",
        (uint64_t)mode, (uint64_t)status, (uint64_t)event, (uint64_t)mdio_mode);
    if (mode == UINT32_MAX || status == UINT32_MAX || (event & MAC_EVENT_MDIO) ||
        (mdio_mode & MDIO_AUTO_POLL)) {
        console_write("bcm57765: manual MDIO unavailable\n");
        return BCM57765_INCOMPLETE;
    }
    uint16_t id_high, id_low, control, first, current, aux;
    if (!mdio_read(MII_ID_HIGH, &id_high, pending) || !mdio_read(MII_ID_LOW, &id_low, pending)) {
        console_write("bcm57765: PHY ID read failed\n");
        return BCM57765_INCOMPLETE;
    }
    console_printf("bcm57765: phy_addr=1 phy_id=%x:%x\n", (uint64_t)id_high, (uint64_t)id_low);
    if (id_high != 0x0362 || id_low != 0x5e44) return BCM57765_INCOMPLETE;
    if (!mdio_read(MII_CONTROL, &control, pending) || control == UINT16_MAX) return BCM57765_INCOMPLETE;
    for (unsigned sample = 0; sample < 21; sample++) {
        /* BMSR latches link loss. Use the second read for current carrier. */
        if (!mdio_read(MII_STATUS, &first, pending) || !mdio_read(MII_STATUS, &current, pending) ||
            !mdio_read(MII_AUX_STATUS, &aux, pending) || current == UINT16_MAX || aux == UINT16_MAX)
            return BCM57765_INCOMPLETE;
        bool carrier = (current & MII_LINK) && (aux & MII_AUX_LINK);
        unsigned code = carrier ? (aux >> 8) & 7 : 0;
        static const unsigned speed[] = { 0, 10, 10, 100, 0, 100, 1000, 1000 };
        if (speed[code] || sample == 20) {
            const char *duplex = !speed[code] ? "unknown" :
                (code == 2 || code == 5 || code == 7) ? "full" : "half";
            console_printf("bcm57765: bmcr=%x bmsr_first=%x bmsr=%x aux=%x carrier=%u speed_mbps=%u duplex=%s sample=%u\n",
                (uint64_t)control, (uint64_t)first, (uint64_t)current, (uint64_t)aux,
                (uint64_t)carrier, (uint64_t)speed[code], duplex, (uint64_t)sample);
            if (speed[code]) return BCM57765_LINK_UP;
            return ((current & MII_LINK) || (aux & MII_AUX_LINK)) ? BCM57765_INCOMPLETE : BCM57765_LINK_DOWN;
        }
        if (!timer_delay_us(50000)) return BCM57765_INCOMPLETE;
    }
    return BCM57765_INCOMPLETE;
}

enum bcm57765_probe_result bcm57765_probe(void) {
    if (!display_profile_is_pro81()) {
        console_write("bcm57765: refused platform identity\n");
        return BCM57765_REFUSED;
    }
    const struct pci_device *p = controller();
    uint64_t root = vmm_kernel_address_space(), occupied;
    if (!p || !timer_reference_ready() || !vmm_diagnostic_sole_kernel_root(root) ||
        vmm_virt_to_phys_in_space(root, register_window, &occupied)) {
        console_write("bcm57765: refused PCI identity, timer, or mapping context\n");
        return BCM57765_REFUSED;
    }

    uint64_t bar = ((uint64_t)p->bar[1] << 32) | p->bar[0];
    uint64_t base = bar & ~15ull;
    uint64_t bar2 = ((uint64_t)p->bar[3] << 32) | p->bar[2];
    uint16_t command = pci_read_config16(p, PCI_COMMAND);
    uint16_t pm = pci_read_config16(p, PCI_PM_CONTROL);
    console_printf("bcm57765: pci=02:00.0 device=14e4:16b4 bar0=%x bar2=%x command=%x pm=%x\n",
        bar, bar2, (uint64_t)command, (uint64_t)pm);
    if ((bar & 0xffff) != 0x000c || !base || base >= 0x100000000ull ||
        bar2 != bar + 0x10000 || command == UINT16_MAX || pm == UINT16_MAX ||
        ((command & PCI_BUS_MASTER) && ((pm & PM_STATE) || !(command & PCI_MEMORY)))) {
        console_write("bcm57765: refused BAR layout or active bus master transition\n");
        return BCM57765_REFUSED;
    }
    if (!vmm_map_page_in_space(root, register_window, base,
            VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE))
        return BCM57765_REFUSED;

    enum bcm57765_probe_result result = BCM57765_INCOMPLETE;
    const bool needs_memory = !(command & PCI_MEMORY);
    const bool needs_power = (pm & PM_STATE) != 0;
    bool pending = false;
    if (!needs_power || pci_power_d0(p)) {
        if (needs_memory) pci_write_config16(p, PCI_COMMAND, command | PCI_MEMORY);
        if (pci_read_config16(p, PCI_COMMAND) == (command | PCI_MEMORY))
            result = read_link(&pending);
    }
    bool mdio_idle = !pending || !(mac_read(MDIO_COMMAND) & MDIO_BUSY);
    if (needs_memory) pci_write_config16(p, PCI_COMMAND, command);
    if (needs_power) {
        pci_write_config16(p, PCI_PM_CONTROL, pm & ~PM_EVENT_STATUS);
        timer_delay_us(10000);
    }
    bool restored = pci_read_config16(p, PCI_COMMAND) == command &&
        ((pci_read_config16(p, PCI_PM_CONTROL) ^ pm) & ~PM_EVENT_STATUS) == 0;
    uint64_t removed;
    bool unmapped = vmm_unmap_page_in_space(root, register_window, &removed) && removed == base;
    console_printf("bcm57765: mapping=%s pci_restored=%u mdio_idle=%u bus_master_changes=0 packet_dma=0\n",
        unmapped ? "released" : "unverified", (uint64_t)restored, (uint64_t)mdio_idle);
    return unmapped && restored && mdio_idle ? result : BCM57765_UNSAFE;
}
