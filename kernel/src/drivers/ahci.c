#include <srvros/ahci.h>
#include <srvros/block.h>
#include <srvros/console.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>

#include <stdbool.h>
#include <stdint.h>

#define PCI_COMMAND 0x04
#define PCI_COMMAND_MEMORY_SPACE 0x0002
#define PCI_COMMAND_BUS_MASTER 0x0004

#define AHCI_BAR 5
#define AHCI_MMIO_VIRTUAL 0xffffc001f2000000ull
#define AHCI_MMIO_PAGES 4
#define AHCI_MAX_PORTS 8
#define AHCI_SECTOR_SIZE 512

#define HBA_GHC_AHCI_ENABLE (1u << 31)

#define HBA_PORT_CMD_ST (1u << 0)
#define HBA_PORT_CMD_FRE (1u << 4)
#define HBA_PORT_CMD_FR (1u << 14)
#define HBA_PORT_CMD_CR (1u << 15)
#define HBA_PORT_IS_TFES (1u << 30)

#define SATA_SIG_ATA 0x00000101u
#define FIS_TYPE_REG_H2D 0x27
#define ATA_CMD_IDENTIFY 0xec
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35

struct hba_port {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t reserved0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t reserved1[11];
    uint32_t vendor[4];
} __attribute__((packed));

struct hba_memory {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t reserved[0xa0 - 0x2c];
    uint8_t vendor[0x100 - 0xa0];
    struct hba_port ports[32];
} __attribute__((packed));

struct hba_command_header {
    uint16_t flags;
    uint16_t prdt_length;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} __attribute__((packed));

struct hba_prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc_i;
} __attribute__((packed));

struct hba_command_table {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    struct hba_prdt_entry prdt[1];
} __attribute__((packed));

struct ahci_disk {
    bool used;
    struct hba_port *port;
    uint8_t port_index;
    char name[8];
    uint64_t sectors;
    uint64_t command_list_phys;
    uint64_t fis_phys;
    uint64_t command_table_phys;
    uint64_t io_buffer_phys;
    struct hba_command_header *command_list;
    struct hba_command_table *command_table;
    uint8_t *io_buffer;
};

static volatile struct hba_memory *hba;
static uint64_t hba_physical;
static struct ahci_disk disks[AHCI_MAX_PORTS];
static uint64_t disk_count;

static void zero_memory(void *address, uint64_t size) {
    uint8_t *bytes = address;
    for (uint64_t i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint64_t read_identify_lba_count(const uint8_t *identify) {
    uint64_t sectors =
        (uint64_t)read_u16(identify + 200) |
        ((uint64_t)read_u16(identify + 202) << 16) |
        ((uint64_t)read_u16(identify + 204) << 32) |
        ((uint64_t)read_u16(identify + 206) << 48);
    if (sectors != 0) {
        return sectors;
    }

    return (uint64_t)read_u16(identify + 120) |
        ((uint64_t)read_u16(identify + 122) << 16);
}

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(uint64_t flags) {
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
}

static void stop_port(struct hba_port *port) {
    port->cmd &= ~HBA_PORT_CMD_ST;
    for (uint64_t limit = 1000000; (port->cmd & HBA_PORT_CMD_CR) != 0 && limit > 0; limit--) {
        __asm__ volatile ("pause");
    }
    port->cmd &= ~HBA_PORT_CMD_FRE;
    for (uint64_t limit = 1000000; (port->cmd & HBA_PORT_CMD_FR) != 0 && limit > 0; limit--) {
        __asm__ volatile ("pause");
    }
}

static void start_port(struct hba_port *port) {
    port->cmd |= HBA_PORT_CMD_FRE;
    port->cmd |= HBA_PORT_CMD_ST;
}

static bool port_present(struct hba_port *port) {
    uint32_t det = port->ssts & 0xf;
    uint32_t ipm = (port->ssts >> 8) & 0xf;
    return det == 3 && ipm == 1 && port->sig == SATA_SIG_ATA;
}

static bool allocate_port_memory(struct ahci_disk *disk) {
    disk->command_list_phys = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    disk->fis_phys = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    disk->command_table_phys = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    disk->io_buffer_phys = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    if (disk->command_list_phys == 0 ||
        disk->fis_phys == 0 ||
        disk->command_table_phys == 0 ||
        disk->io_buffer_phys == 0) {
        return false;
    }

    disk->command_list = pmm_phys_to_virt(disk->command_list_phys);
    disk->command_table = pmm_phys_to_virt(disk->command_table_phys);
    disk->io_buffer = pmm_phys_to_virt(disk->io_buffer_phys);
    zero_memory(disk->command_list, 4096);
    zero_memory((void *)pmm_phys_to_virt(disk->fis_phys), 4096);
    zero_memory(disk->command_table, 4096);
    zero_memory(disk->io_buffer, 4096);
    return true;
}

static bool ensure_mmio_mapping(void) {
    if (hba_physical == 0) {
        return false;
    }

    for (uint64_t i = 0; i < AHCI_MMIO_PAGES; i++) {
        uint64_t virtual_address = AHCI_MMIO_VIRTUAL + i * 4096;
        uint64_t translated = 0;
        if (vmm_virt_to_phys(virtual_address, &translated)) {
            continue;
        }

        if (!vmm_map_page(virtual_address,
                hba_physical + i * 4096,
                VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE)) {
            console_write("ahci: mmio remap failed\n");
            return false;
        }
    }

    return true;
}

static bool issue_command(struct ahci_disk *disk, uint8_t command, uint64_t lba, uint16_t sectors, bool write) {
    uint64_t flags = irq_save();
    uint64_t previous_address_space = vmm_current_address_space();
    uint64_t kernel_address_space = vmm_kernel_address_space();
    bool switched_address_space = previous_address_space != kernel_address_space;
    bool success = false;

    if (switched_address_space) {
        vmm_switch_address_space(kernel_address_space);
    }
    if (!ensure_mmio_mapping()) {
        goto out;
    }

    struct hba_port *port = disk->port;
    struct hba_command_header *header = &disk->command_list[0];
    struct hba_command_table *table = disk->command_table;
    uint32_t byte_count = sectors == 0 ? AHCI_SECTOR_SIZE : (uint32_t)sectors * AHCI_SECTOR_SIZE;

    zero_memory(header, sizeof(*header));
    zero_memory(table, sizeof(*table));

    header->flags = 5 | (write ? (1u << 6) : 0);
    header->prdt_length = 1;
    header->ctba = (uint32_t)(disk->command_table_phys & 0xffffffffu);
    header->ctbau = (uint32_t)(disk->command_table_phys >> 32);

    table->prdt[0].dba = (uint32_t)(disk->io_buffer_phys & 0xffffffffu);
    table->prdt[0].dbau = (uint32_t)(disk->io_buffer_phys >> 32);
    table->prdt[0].dbc_i = (byte_count - 1) | (1u << 31);

    uint8_t *fis = table->cfis;
    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = 1u << 7;
    fis[2] = command;
    fis[4] = (uint8_t)lba;
    fis[5] = (uint8_t)(lba >> 8);
    fis[6] = (uint8_t)(lba >> 16);
    fis[7] = 1u << 6;
    fis[8] = (uint8_t)(lba >> 24);
    fis[9] = (uint8_t)(lba >> 32);
    fis[10] = (uint8_t)(lba >> 40);
    fis[12] = (uint8_t)sectors;
    fis[13] = (uint8_t)(sectors >> 8);

    port->is = 0xffffffffu;
    uint64_t tfd_limit = 1000000;
    while ((port->tfd & 0x88) != 0 && tfd_limit-- > 0) {
        __asm__ volatile ("pause");
    }
    if ((port->tfd & 0x88) != 0) {
        goto out;
    }

    port->ci = 1;
    while ((port->ci & 1) != 0) {
        if ((port->is & HBA_PORT_IS_TFES) != 0) {
            goto out;
        }
        __asm__ volatile ("pause");
    }

    success = (port->is & HBA_PORT_IS_TFES) == 0;

out:
    if (switched_address_space) {
        vmm_switch_address_space(previous_address_space);
    }
    irq_restore(flags);
    return success;
}

static bool ahci_read(struct block_device *device, uint64_t offset, void *buffer, uint64_t length) {
    if (device == 0 || buffer == 0) {
        return false;
    }

    struct ahci_disk *disk = device->private_data;
    if (disk == 0 || (offset + length) > disk->sectors * AHCI_SECTOR_SIZE) {
        return false;
    }

    uint8_t *out = buffer;
    uint64_t copied = 0;
    while (copied < length) {
        uint64_t absolute = offset + copied;
        uint64_t lba = absolute / AHCI_SECTOR_SIZE;
        uint64_t sector_offset = absolute % AHCI_SECTOR_SIZE;
        if (!issue_command(disk, ATA_CMD_READ_DMA_EXT, lba, 1, false)) {
            return false;
        }

        uint64_t chunk = AHCI_SECTOR_SIZE - sector_offset;
        if (chunk > length - copied) {
            chunk = length - copied;
        }
        for (uint64_t i = 0; i < chunk; i++) {
            out[copied + i] = disk->io_buffer[sector_offset + i];
        }
        copied += chunk;
    }

    return true;
}

static bool ahci_write(struct block_device *device, uint64_t offset, const void *buffer, uint64_t length) {
    if (device == 0 || buffer == 0) {
        return false;
    }

    struct ahci_disk *disk = device->private_data;
    if (disk == 0 || (offset + length) > disk->sectors * AHCI_SECTOR_SIZE) {
        return false;
    }

    const uint8_t *in = buffer;
    uint64_t copied = 0;
    while (copied < length) {
        uint64_t absolute = offset + copied;
        uint64_t lba = absolute / AHCI_SECTOR_SIZE;
        uint64_t sector_offset = absolute % AHCI_SECTOR_SIZE;
        uint64_t chunk = AHCI_SECTOR_SIZE - sector_offset;
        if (chunk > length - copied) {
            chunk = length - copied;
        }

        if (chunk != AHCI_SECTOR_SIZE) {
            if (!issue_command(disk, ATA_CMD_READ_DMA_EXT, lba, 1, false)) {
                return false;
            }
        }

        for (uint64_t i = 0; i < chunk; i++) {
            disk->io_buffer[sector_offset + i] = in[copied + i];
        }

        if (!issue_command(disk, ATA_CMD_WRITE_DMA_EXT, lba, 1, true)) {
            return false;
        }
        copied += chunk;
    }

    return true;
}

static void make_disk_name(char *name, uint64_t index) {
    name[0] = 'a';
    name[1] = 'h';
    name[2] = 'c';
    name[3] = 'i';
    name[4] = (char)('0' + (index % 10));
    name[5] = '\0';
}

static bool init_port(uint8_t port_index, struct hba_port *port) {
    if (disk_count >= AHCI_MAX_PORTS || !port_present(port)) {
        return false;
    }

    struct ahci_disk *disk = &disks[disk_count];
    disk->port = port;
    disk->port_index = port_index;
    make_disk_name(disk->name, disk_count);

    stop_port(port);
    if (!allocate_port_memory(disk)) {
        return false;
    }

    port->clb = (uint32_t)(disk->command_list_phys & 0xffffffffu);
    port->clbu = (uint32_t)(disk->command_list_phys >> 32);
    port->fb = (uint32_t)(disk->fis_phys & 0xffffffffu);
    port->fbu = (uint32_t)(disk->fis_phys >> 32);
    port->serr = 0xffffffffu;
    port->is = 0xffffffffu;
    port->ie = 0;
    start_port(port);

    if (!issue_command(disk, ATA_CMD_IDENTIFY, 0, 0, false)) {
        console_printf("ahci: port=%u identify failed\n", (uint64_t)port_index);
        return false;
    }

    disk->sectors = read_identify_lba_count(disk->io_buffer);
    if (disk->sectors == 0) {
        console_printf("ahci: port=%u no capacity\n", (uint64_t)port_index);
        return false;
    }

    if (block_register(disk->name, AHCI_SECTOR_SIZE, disk->sectors, ahci_read, ahci_write, disk) == 0) {
        return false;
    }

    disk->used = true;
    disk_count++;
    console_printf("ahci: registered %s port=%u sectors=%u\n",
        disk->name,
        (uint64_t)port_index,
        disk->sectors);
    return true;
}

bool ahci_init(void) {
    const struct pci_device *device = pci_find_by_class(0x01, 0x06);
    if (device == 0 || device->prog_if != 0x01) {
        console_write("ahci: no controller\n");
        return false;
    }

    uint16_t command = pci_read_config16(device, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
    pci_write_config16(device, PCI_COMMAND, command);

    uint32_t abar = pci_read_config32(device, 0x10 + AHCI_BAR * 4) & 0xfffffff0u;
    if (abar == 0) {
        console_write("ahci: missing abar\n");
        return false;
    }

    hba_physical = (uint64_t)abar;
    for (uint64_t i = 0; i < AHCI_MMIO_PAGES; i++) {
        if (!vmm_map_page(AHCI_MMIO_VIRTUAL + i * 4096,
                hba_physical + i * 4096,
                VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE)) {
            console_write("ahci: mmio map failed\n");
            hba_physical = 0;
            return false;
        }
    }

    hba = (volatile struct hba_memory *)AHCI_MMIO_VIRTUAL;
    hba->ghc |= HBA_GHC_AHCI_ENABLE;
    uint32_t ports = hba->pi;
    console_printf("ahci: abar=%x pi=%x cap=%x\n",
        (uint64_t)abar,
        (uint64_t)ports,
        (uint64_t)hba->cap);

    for (uint8_t port = 0; port < 32 && disk_count < AHCI_MAX_PORTS; port++) {
        if ((ports & (1u << port)) != 0) {
            (void)init_port(port, (struct hba_port *)&hba->ports[port]);
        }
    }

    if (disk_count == 0) {
        console_write("ahci: no sata disks\n");
        return false;
    }
    return true;
}
