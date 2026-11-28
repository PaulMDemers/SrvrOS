#include <srvros/bcm4331_probe.h>
#include <srvros/console.h>
#include <srvros/display_profile.h>
#include <srvros/initramfs.h>
#include <srvros/pci.h>
#include <srvros/sha256.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>

enum {
    PCI_COMMAND = 0x04, PCI_MEMORY = 2, PCI_BUS_MASTER = 4,
    PCI_SUBSYSTEM = 0x2c, PCI_PM_CAP = 0x40, PCI_PMCSR = 0x44,
    PCI_BAR0_WINDOW = 0x80,
    PCI_EXP_CAP = 0xd0, PCI_EXP_STATUS = 0xda, PCI_EXP_PENDING = 0x20,
    PAGE_BYTES = 4096, BAR_BYTES = 4 * PAGE_BYTES, CHIPCOMMON_PAGE = 3 * PAGE_BYTES,
    CHIP_ID = 0x00, CHIP_CAP = 0x04, CHIP_CONTROL = 0x28, CHIP_STATUS = 0x2c,
    CHIP_EROM = 0xfc, CHIP_SROM_CONTROL = 0x190,
    CHIP_SROM = 0x800, CHIP_CAP_SROM = 1u << 30,
    CHIP_STATUS_SPROM = 1u << 1, CHIP_PA_PINS = (1u << 4) | (1u << 7) | (1u << 12),
    CORE_IO_CONTROL = 0x408, CORE_IO_STATUS = 0x500, CORE_RESET_CONTROL = 0x800, CORE_RESET_STATUS = 0x804,
    CORE_CLOCK = 1, CORE_FORCE_CLOCK = 2, D11_PHY_CLOCK = 4, CORE_RESET = 1,
    D11_MAC_CONTROL = 0x120, D11_PHY_VERSION = 0x3e0,
    D11_IRQ_REASON = 0x128, D11_IRQ_MASK = 0x12c, D11_MAC_SUSPENDED = 1,
    D11_SHM_CONTROL = 0x160, D11_SHM_DATA = 0x164, D11_SHM_DATA_ODD = 0x166,
    D11_CLOCK_CONTROL = 0x1e0, D11_FORCE_HT = 2, D11_PLL_REQUEST = 0x300,
    D11_HT_READY = 0x20000, D11_PLL_READY = 0x03000000,
    D11_MAC_ENABLED = 1, D11_PSM_RUN = 2, D11_PSM_JUMP_ZERO = 4,
    D11_SHM_ENABLED = 0x100, D11_IHR_ENABLED = 0x400, D11_INFRA = 0x20000,
    D11_GMODE = 0x80000000u,
    D11_SHM_FIRMWARE = 0, D11_SHM_SHARED = 1, D11_SHM_SCRATCH = 2, D11_SHM_AUTO_WRITE = 0x100,
    UCODE_HEADER_BYTES = 8, UCODE_BYTES = 43224,
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
            cores[1].address == 0x18001000 && cores[2].address == 0x18002000 &&
            cores[1].wrapper == 0x18101000;
        if ((cia & 0xf) != EROM_VALID || index == count) return false;
        uint32_t cib = words[index++];
        if ((cib & 0xf) != EROM_VALID) return false;

        unsigned id = (cia >> 8) & 0xfff, revision = cib >> 24;
        struct bcma_core core = {0};
        uint32_t slave_wrapper = 0;
        bool primary_seen = false;
        while (index < count) {
            uint32_t word = words[index];
            if ((word & 0xf) == EROM_VALID || word == EROM_END) break;
            if (!(word & EROM_VALID) || word == UINT32_MAX) return false;
            index++;
            if ((word & 0xf) == EROM_MASTER_PORT) continue;
            if ((word & (EROM_ADDR_TAG | EROM_VALID)) != EROM_ADDRESS) return false;
            /* The PCIe core also advertises wider bridge apertures. Consume
             * all operands, but admit only 32-bit 4 KiB register windows. */
            bool four_kib = !(word & EROM_ADDR_SIZE);
            uint32_t upper_address = 0;
            if (word & EROM_ADDR_64) {
                if (index == count) return false;
                upper_address = words[index++];
            }
            if ((word & EROM_ADDR_SIZE) == EROM_SIZE_DESCRIPTOR) {
                if (index == count) return false;
                uint32_t size = words[index++];
                four_kib = (size & 0xfffff000) == PAGE_BYTES;
                if (size & EROM_ADDR_64) {
                    if (index == count) return false;
                    if (words[index++] != 0) four_kib = false;
                }
            }
            uint32_t address = word & 0xfffff000;
            unsigned type = word & EROM_ADDR_TYPE;
            bool usable = !upper_address && four_kib;
            if (type == 0 && !primary_seen && !(word & EROM_ADDR_PORT)) {
                primary_seen = true;
                if (usable) core.address = address;
            }
            if (type == EROM_MASTER_WRAP && !core.wrapper && usable) core.wrapper = address;
            if (type == EROM_SLAVE_WRAP && !slave_wrapper && usable) slave_wrapper = address;
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

static bool pci_transactions_drained(const struct pci_device *p) {
    for (unsigned attempt = 0; attempt < 1000; attempt++) {
        uint16_t status = pci_read_config16(p, PCI_EXP_STATUS);
        if (status == UINT16_MAX) return false;
        if (!(status & PCI_EXP_PENDING)) return true;
        if (!timer_delay_us(10)) return false;
    }
    return false;
}

static uint8_t srom_crc_byte(uint8_t crc, uint8_t byte) {
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ ((crc & 1) ? 0xab : 0);
    return crc;
}

static bool inspect_srom(const volatile uint16_t *rom) {
    static const unsigned lengths[] = {220, 230, 234};
    for (unsigned i = 0; i < sizeof lengths / sizeof lengths[0]; i++) {
        unsigned count = lengths[i];
        uint8_t crc = 0xff;
        for (unsigned word = 0; word + 1 < count; word++) {
            uint16_t value = rom[word];
            crc = srom_crc_byte(srom_crc_byte(crc, (uint8_t)value), (uint8_t)(value >> 8));
        }
        uint16_t footer = rom[count - 1];
        uint8_t revision = (uint8_t)footer;
        crc = srom_crc_byte(crc, revision) ^ 0xff;
        if (crc == footer >> 8 && revision >= 8 && revision <= 11) {
            console_printf("bcm4331: sprom words=%u revision=%u crc=valid\n", (uint64_t)count, (uint64_t)revision);
            return true;
        }
    }
    return false;
}

static bool inspect_board_rom(volatile uint32_t *cc, bool *restored) {
    uint32_t original = cc[CHIP_CONTROL / 4];
    if (original == UINT32_MAX) return false;
    /* BCM4331 multiplexes these external PA lines with the SPROM pins.
     * Touch just that mux and restore the complete original register. */
    uint32_t sprom_pins = original & ~CHIP_PA_PINS;
    if (sprom_pins != original) cc[CHIP_CONTROL / 4] = sprom_pins;
    bool pins_selected = cc[CHIP_CONTROL / 4] == sprom_pins;
    bool verified = pins_selected && inspect_srom((const volatile uint16_t *)((uintptr_t)cc + CHIP_SROM));
    if (sprom_pins != original) cc[CHIP_CONTROL / 4] = original;
    *restored = cc[CHIP_CONTROL / 4] == original;
    console_printf("bcm4331: sprom pins_selected=%u pins_restored=%u crc_verified=%u\n",
        (uint64_t)pins_selected, (uint64_t)*restored, (uint64_t)verified);
    return verified;
}

static bool inspect_d11(const struct pci_device *p, const struct bcma_core *core, const volatile uint32_t *words) {
    pci_write_config32(p, PCI_BAR0_WINDOW, core->wrapper);
    if (pci_read_config32(p, PCI_BAR0_WINDOW) != core->wrapper) return false;
    uint32_t ioctl = words[CORE_IO_CONTROL / 4], iost = words[CORE_IO_STATUS / 4];
    uint32_t reset = words[CORE_RESET_CONTROL / 4], status = words[CORE_RESET_STATUS / 4];
    console_printf("bcm4331: d11_wrapper ioctl=%x iost=%x reset=%x reset_status=%x\n",
        (uint64_t)ioctl, (uint64_t)iost, (uint64_t)reset, (uint64_t)status);
    if (status || (reset & CORE_RESET) || (ioctl & (CORE_CLOCK | CORE_FORCE_CLOCK | D11_PHY_CLOCK)) !=
        (CORE_CLOCK | D11_PHY_CLOCK)) return false;
    pci_write_config32(p, PCI_BAR0_WINDOW, core->address);
    if (pci_read_config32(p, PCI_BAR0_WINDOW) != core->address) return false;
    uint32_t mac_control = words[D11_MAC_CONTROL / 4];
    uint16_t phy = ((const volatile uint16_t *)words)[D11_PHY_VERSION / 2];
    console_printf("bcm4331: phy=%x analog=%u type=%u revision=%u mac_control=%x\n",
        (uint64_t)phy, (uint64_t)(phy >> 12), (uint64_t)((phy >> 8) & 0xf), (uint64_t)(phy & 0xff),
        (uint64_t)mac_control);
    return mac_control != UINT32_MAX && phy == 0x9701;
}

static const uint8_t *pinned_asset(const char *path, unsigned size, const char *expected) {
    const struct initramfs_file *file = initramfs_find(path);
    if (!file || file->size != size) return 0;
    uint8_t digest[32];
    static const char digits[] = "0123456789abcdef";
    sha256_digest(file->data, size, digest);
    for (unsigned i = 0; i < sizeof digest; i++)
        if (digits[digest[i] >> 4] != expected[i * 2] || digits[digest[i] & 15] != expected[i * 2 + 1]) return 0;
    return file->data;
}

static bool wait_d11_register(const volatile uint32_t *regs, unsigned offset, uint32_t mask, uint32_t expected) {
    for (unsigned attempt = 0; attempt < 10000; attempt++) {
        uint32_t value = regs[offset / 4];
        if (value == UINT32_MAX) return false;
        if ((value & mask) == expected) return true;
        if (!timer_delay_us(10)) return false;
    }
    return false;
}

/* Clear the device's retained DMA and PSM state before uploading. Reuse the
 * captured warm 20 MHz PHY/clock settings and never reenable host DMA. */
static bool reset_d11(const struct pci_device *p, const struct bcma_core *core, volatile uint32_t *regs, bool *safe) {
    pci_write_config32(p, PCI_BAR0_WINDOW, core->wrapper);
    if (pci_read_config32(p, PCI_BAR0_WINDOW) != core->wrapper) return false;
    uint32_t ioctl = regs[CORE_IO_CONTROL / 4];
    if (ioctl != 0x2055 || !wait_d11_register(regs, CORE_RESET_STATUS, UINT32_MAX, 0)) return false;

    regs[CORE_RESET_CONTROL / 4] = CORE_RESET;
    bool asserted = wait_d11_register(regs, CORE_RESET_CONTROL, CORE_RESET, CORE_RESET);
    bool released = false, clock_ready = false;
    if (asserted) {
        regs[CORE_IO_CONTROL / 4] = ioctl | CORE_FORCE_CLOCK;
        if (regs[CORE_IO_CONTROL / 4] == (ioctl | CORE_FORCE_CLOCK)) {
            regs[CORE_RESET_CONTROL / 4] = 0;
            released = wait_d11_register(regs, CORE_RESET_CONTROL, CORE_RESET, 0) &&
                wait_d11_register(regs, CORE_RESET_STATUS, UINT32_MAX, 0);
        }
        if (released) {
            regs[CORE_IO_CONTROL / 4] = ioctl;
            if (regs[CORE_IO_CONTROL / 4] == ioctl) {
                pci_write_config32(p, PCI_BAR0_WINDOW, core->address);
                if (pci_read_config32(p, PCI_BAR0_WINDOW) == core->address) {
                    uint32_t clock = regs[D11_CLOCK_CONTROL / 4];
                    if (clock != UINT32_MAX) {
                        regs[D11_CLOCK_CONTROL / 4] = clock | D11_FORCE_HT | D11_PLL_REQUEST;
                        clock_ready = wait_d11_register(regs, D11_CLOCK_CONTROL,
                            D11_HT_READY | D11_PLL_READY, D11_HT_READY | D11_PLL_READY) &&
                            ((const volatile uint16_t *)regs)[D11_PHY_VERSION / 2] == 0x9701;
                    }
                }
            }
        }
    }
    if (!clock_ready) {
        pci_write_config32(p, PCI_BAR0_WINDOW, core->wrapper);
        *safe = false;
        if (pci_read_config32(p, PCI_BAR0_WINDOW) == core->wrapper) {
            regs[CORE_RESET_CONTROL / 4] = CORE_RESET;
            *safe = wait_d11_register(regs, CORE_RESET_CONTROL, CORE_RESET, CORE_RESET);
        }
    }
    console_printf("bcm4331: d11_reset asserted=%u released=%u clocks_verified=%u safe=%u\n",
        (uint64_t)asserted, (uint64_t)released, (uint64_t)clock_ready, (uint64_t)*safe);
    return clock_ready;
}

#include "bcm4331_radio.inc"

/* Run the D11 PSM bootstrap. Keep the MAC disabled and mask device IRQs
 * while loading firmware. An explicit radio trial may enable receive. */
static bool firmware_trial(volatile uint32_t *regs, const uint8_t *ucode, const struct radio_assets *radio,
    bool *beacon, bool *retired) {
    const uint32_t quiet = D11_GMODE | D11_SHM_ENABLED | D11_IHR_ENABLED | D11_INFRA;
    regs[D11_IRQ_MASK / 4] = 0;
    regs[D11_MAC_CONTROL / 4] = quiet | D11_PSM_JUMP_ZERO;
    bool admitted = regs[D11_MAC_CONTROL / 4] == (quiet | D11_PSM_JUMP_ZERO) && regs[D11_IRQ_MASK / 4] == 0;
    unsigned uploaded = 0;
    uint32_t reason = 0;
    uint16_t revision = 0, patch = 0;
    if (admitted) {
        volatile uint16_t *halves = (volatile uint16_t *)regs;
        for (unsigned i = 0; i < 64; i++) {
            regs[D11_SHM_CONTROL / 4] = (D11_SHM_SCRATCH << 16) | i;
            halves[D11_SHM_DATA / 2] = 0;
        }
        for (unsigned offset = 0; offset < PAGE_BYTES; offset += 2) {
            regs[D11_SHM_CONTROL / 4] = (D11_SHM_SHARED << 16) | (offset / 4);
            halves[((offset & 2) ? D11_SHM_DATA_ODD : D11_SHM_DATA) / 2] = 0;
        }
        regs[D11_SHM_CONTROL / 4] = (D11_SHM_FIRMWARE | D11_SHM_AUTO_WRITE) << 16;
        for (unsigned i = 0; i < UCODE_BYTES - UCODE_HEADER_BYTES; i += 4) {
            regs[D11_SHM_DATA / 4] = (uint32_t)ucode[i] << 24 | (uint32_t)ucode[i + 1] << 16 |
                (uint32_t)ucode[i + 2] << 8 | ucode[i + 3];
            if (!timer_delay_us(10)) break;
            uploaded++;
        }
        if (uploaded == (UCODE_BYTES - UCODE_HEADER_BYTES) / 4) {
            regs[D11_IRQ_REASON / 4] = UINT32_MAX;
            reason = regs[D11_IRQ_REASON / 4];
            if (!reason) {
                regs[D11_MAC_CONTROL / 4] = quiet | D11_PSM_RUN;
                for (unsigned attempt = 0; attempt < 200; attempt++) {
                    reason = regs[D11_IRQ_REASON / 4];
                    if (reason == D11_MAC_SUSPENDED || reason == UINT32_MAX || !timer_delay_us(5000)) break;
                }
                if (reason == D11_MAC_SUSPENDED) {
                    regs[D11_SHM_CONTROL / 4] = D11_SHM_SHARED << 16;
                    revision = halves[D11_SHM_DATA / 2];
                    patch = halves[D11_SHM_DATA_ODD / 2];
                }
            }
        }
    }
    bool radio_retired = true;
    if (radio && revision == 784 && patch == 2) *beacon = passive_radio_trial(regs, radio, &radio_retired);
    regs[D11_MAC_CONTROL / 4] = quiet;
    uint32_t control = regs[D11_MAC_CONTROL / 4];
    *retired = radio_retired && control != UINT32_MAX && !(control & (D11_MAC_ENABLED | D11_PSM_RUN)) && !regs[D11_IRQ_MASK / 4];
    console_printf("bcm4331: firmware words=%u irq=%x revision=%u patch=%u psm_retired=%u\n",
        (uint64_t)uploaded, (uint64_t)reason, (uint64_t)revision, (uint64_t)patch, (uint64_t)*retired);
    return *retired && revision == 784 && patch == 2;
}

enum bcm4331_probe_result bcm4331_probe(enum bcm4331_probe_mode mode) {
    if (!display_profile_is_pro81()) {
        console_write("bcm4331: refused platform identity\n");
        return BCM4331_REFUSED;
    }
    bool start_firmware = mode != BCM4331_TOPOLOGY;
    const uint8_t *ucode = start_firmware ? pinned_asset("/firmware/bcm4331-ucode29.fw", UCODE_BYTES,
        "df16911e9ec2ae2173bcdea06e49977b544ba666222c5e5fab5be0cca47bce03") : 0;
    if (start_firmware && !ucode) {
        console_write("bcm4331: refused missing or mismatched firmware image\n");
        return BCM4331_REFUSED;
    }
    struct radio_assets assets = {0};
    if (mode == BCM4331_CHANNEL10) {
        assets.initial = pinned_asset("/firmware/bcm4331-ht-init.fw", 2830,
            "1ee846e61c27505bc55d68bd1ff8155affc2771eb8244d935d329ed8c08ad764");
        assets.band = pinned_asset("/firmware/bcm4331-ht-band.fw", 178,
            "c45a240b9bb97df9f99550abe4a2904908931c5266cf7874869e8cdf6db8a7c5");
        assets.profile = pinned_asset("/firmware/bcm4331-channel10.bin", RECIPE_HEADER + RECIPE_OP_BYTES * RECIPE_OPS,
            "4ced8b4f9f4c39bf0c89cbf72f8dcee0b227720ab62019f7489b499e7ac89e98");
        if (!assets.initial || !assets.band || !assets.profile) {
            console_write("bcm4331: refused missing or mismatched channel 10 assets\n");
            return BCM4331_REFUSED;
        }
    }
    const struct pci_device *p = bcm4331_controller();
    uint64_t root = vmm_kernel_address_space(), occupied;
    const uint64_t fixed = bcm4331_window + CHIPCOMMON_PAGE;
    if (!p || !timer_reference_ready() || !vmm_diagnostic_sole_kernel_root(root) ||
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
        pci_read_config8(p, PCI_PM_CAP) != 1 || pci_read_config8(p, PCI_EXP_CAP) != 0x10 ||
        pm == UINT16_MAX || (pm & 3) ||
        command == UINT16_MAX || window == UINT32_MAX ||
        ((command & PCI_BUS_MASTER) && !(command & PCI_MEMORY))) {
        console_write("bcm4331: refused BAR layout or PCI power state\n");
        return BCM4331_REFUSED;
    }

    /* A warm Ubuntu reboot can leave BME set. Keep the function fenced
     * through the return so no previous DMA queue can resume. */
    uint16_t fenced = command & ~PCI_BUS_MASTER;
    if (fenced != command) pci_write_config16(p, PCI_COMMAND, fenced);
    bool drained = pci_read_config16(p, PCI_COMMAND) == fenced && pci_transactions_drained(p);
    console_printf("bcm4331: inherited_bus_master=%u transactions_drained=%u\n",
        (uint64_t)(fenced != command), (uint64_t)drained);
    if (!drained) return BCM4331_UNSAFE;

    const uint64_t flags = VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE;
    if (!vmm_map_page_in_space(root, fixed, bar + CHIPCOMMON_PAGE, flags | VMM_PAGE_WRITABLE)) return BCM4331_INCOMPLETE;
    uint16_t enabled = fenced | PCI_MEMORY;
    if (enabled != fenced) pci_write_config16(p, PCI_COMMAND, enabled);
    bool mapped = false, switched = false, matched = false, pins_restored = true, srom_ready = false, phy_ready = false;
    bool firmware_attempted = false, firmware_ready = false, psm_retired = true;
    bool beacon = false;
    if (pci_read_config16(p, PCI_COMMAND) == enabled) {
        volatile uint32_t *cc = (volatile uint32_t *)fixed;
        uint32_t id = cc[CHIP_ID / 4], cap = cc[CHIP_CAP / 4], erom = cc[CHIP_EROM / 4];
        uint32_t srom = cc[CHIP_SROM_CONTROL / 4];
        console_printf("bcm4331: chip=%x capabilities=%x erom=%x srom_control=%x\n",
            (uint64_t)id, (uint64_t)cap, (uint64_t)erom, (uint64_t)srom);
        if (id == 0x13924331 && erom == 0x18107000) {
            mapped = vmm_map_page_in_space(root, bcm4331_window, bar, flags | (start_firmware ? VMM_PAGE_WRITABLE : 0));
            if (mapped) {
                pci_write_config32(p, PCI_BAR0_WINDOW, erom);
                switched = true;
                struct bcma_core cores[3] = {0};
                bool selected = pci_read_config32(p, PCI_BAR0_WINDOW) == erom;
                const volatile uint32_t *words = (const volatile uint32_t *)bcm4331_window;
                matched = selected && read_erom(words, PAGE_BYTES / 4, cores);
                if (matched) {
                    for (unsigned i = 0; i < sizeof captured_cores / sizeof captured_cores[0]; i++)
                        console_printf("bcm4331: core=%x revision=%x address=%x wrapper=%x\n",
                            (uint64_t)captured_cores[i].id, (uint64_t)captured_cores[i].revision,
                            (uint64_t)cores[i].address, (uint64_t)cores[i].wrapper);
                    phy_ready = inspect_d11(p, &cores[1], words);
                    if ((cap & CHIP_CAP_SROM) && (cc[CHIP_STATUS / 4] & CHIP_STATUS_SPROM) &&
                        (srom & 1) && !(srom & (1u << 31)))
                        srom_ready = inspect_board_rom(cc, &pins_restored);
                    if (start_firmware && phy_ready && srom_ready && pins_restored) {
                        volatile uint32_t *regs = (volatile uint32_t *)bcm4331_window;
                        firmware_attempted = reset_d11(p, &cores[1], regs, &psm_retired);
                        if (firmware_attempted) firmware_ready = firmware_trial(regs, ucode + UCODE_HEADER_BYTES,
                            mode == BCM4331_CHANNEL10 ? &assets : 0, &beacon, &psm_retired);
                    }
                } else if (selected) {
                    console_write("bcm4331: enumeration ROM did not match the captured topology\n");
                    for (unsigned i = 0; i < 48; i++)
                        console_printf("bcm4331: erom[%u]=%x\n", (uint64_t)i, (uint64_t)words[i]);
                }
            }
        }
    }

    if (switched) pci_write_config32(p, PCI_BAR0_WINDOW, window);
    if (enabled != fenced) pci_write_config16(p, PCI_COMMAND, fenced);
    uint16_t final_command = pci_read_config16(p, PCI_COMMAND);
    bool bus_master_off = final_command != UINT16_MAX && !(final_command & PCI_BUS_MASTER);
    bool restored = pci_read_config32(p, PCI_BAR0_WINDOW) == window && final_command == fenced;
    uint64_t physical;
    bool unmapped = vmm_unmap_page_in_space(root, fixed, &physical) && physical == bar + CHIPCOMMON_PAGE;
    if (mapped) unmapped = (vmm_unmap_page_in_space(root, bcm4331_window, &physical) && physical == bar) && unmapped;
    console_printf("bcm4331: mapping=%s pci_windows_restored=%u bus_master_off=%u pins_restored=%u dma_pages=0 mac_writes=%u\n",
        unmapped ? "released" : "unverified", (uint64_t)restored, (uint64_t)bus_master_off, (uint64_t)pins_restored,
        (uint64_t)firmware_attempted);
    if (!restored || !unmapped || !pins_restored || !psm_retired) return BCM4331_UNSAFE;
    if (matched) console_printf("bcm4331: board_data=%u phy_identified=%u\n", (uint64_t)srom_ready, (uint64_t)phy_ready);
    if (mode == BCM4331_CHANNEL10) return beacon ? BCM4331_BEACON_OK : BCM4331_INCOMPLETE;
    if (start_firmware) return firmware_ready ? BCM4331_FIRMWARE_OK : BCM4331_INCOMPLETE;
    return matched ? BCM4331_TOPOLOGY_OK : BCM4331_INCOMPLETE;
}
