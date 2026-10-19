#include <srvros/acpi.h>
#include <srvros/console.h>
#include <srvros/vmm.h>

#include "ioapic.h"
#include "irq.h"

#define IOAPIC_DEFAULT_PHYS 0xfec00000u
#define IOAPIC_VIRTUAL_BASE 0xffffc000fec00000ull
#define IOAPIC_REGSEL 0x00
#define IOAPIC_WINDOW 0x10
#define IOAPIC_ID 0x00
#define IOAPIC_VER 0x01
#define IOAPIC_REDTBL_BASE 0x10

#define IOAPIC_REDIR_MASKED (1u << 16)
#define IOAPIC_REDIR_TRIGGER_LEVEL (1u << 15)
#define IOAPIC_REDIR_ACTIVE_LOW (1u << 13)

static volatile uint32_t *ioapic;
static uint32_t gsi_base;
static uint32_t max_redirection_entry;

static uint64_t lock(void) {
#ifdef SRVROS_IOAPIC_HOST_TEST
    return 0;
#else
    uint64_t f;__asm__ volatile("pushfq;popq %0;cli":"=r"(f)::"memory");return f;
#endif
}
static void unlock(uint64_t f) {
#ifdef SRVROS_IOAPIC_HOST_TEST
    (void)f;
#else
    if(f&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
}
#ifdef SRVROS_IOAPIC_HOST_TEST
uint32_t ioapic_test_read(uint8_t reg);
void ioapic_test_write(uint8_t reg,uint32_t value);
#endif
static uint32_t read_reg(uint8_t reg) {
#ifdef SRVROS_IOAPIC_HOST_TEST
    return ioapic_test_read(reg);
#else
    ioapic[IOAPIC_REGSEL / sizeof(uint32_t)] = reg;
    return ioapic[IOAPIC_WINDOW / sizeof(uint32_t)];
#endif
}

static void write_reg(uint8_t reg, uint32_t value) {
#ifdef SRVROS_IOAPIC_HOST_TEST
    ioapic_test_write(reg,value);
#else
    ioapic[IOAPIC_REGSEL / sizeof(uint32_t)] = reg;
    ioapic[IOAPIC_WINDOW / sizeof(uint32_t)] = value;
#endif
}

static uint32_t redirection_flags(uint16_t acpi_flags) {
    uint32_t flags = 0;
    uint16_t polarity = acpi_flags & 0x3;
    uint16_t trigger = (acpi_flags >> 2) & 0x3;

    if (polarity == 3) {
        flags |= IOAPIC_REDIR_ACTIVE_LOW;
    }

    if (trigger == 3) {
        flags |= IOAPIC_REDIR_TRIGGER_LEVEL;
    }

    return flags;
}

static bool route_gsi(uint8_t irq, uint32_t gsi, uint8_t vector, uint16_t acpi_flags) {
    if (ioapic == 0) {
        return false;
    }

    if (gsi < gsi_base || gsi - gsi_base > max_redirection_entry) {
        console_printf("ioapic: cannot route irq=%u gsi=%u\n",
            (uint64_t)irq,
            (uint64_t)gsi);
        return false;
    }

    uint32_t index = gsi - gsi_base;
    uint32_t low = vector | redirection_flags(acpi_flags);
    uint32_t high = 0;

    uint64_t saved=lock();
    uint8_t reg=IOAPIC_REDTBL_BASE+index*2;
    write_reg(reg,read_reg(reg)|IOAPIC_REDIR_MASKED);
    write_reg(reg+1,high);
    write_reg(reg,low);
    unlock(saved);

    console_printf("ioapic: irq=%u gsi=%u vector=%u flags=%x\n",
        (uint64_t)irq,
        (uint64_t)gsi,
        (uint64_t)vector,
        (uint64_t)acpi_flags);
    return true;
}

bool ioapic_init(void) {
    const struct acpi_ioapic_info *info = acpi_ioapic();
    uint32_t physical = IOAPIC_DEFAULT_PHYS;

    if (info != 0 && info->present) {
        physical = info->address;
        gsi_base = info->gsi_base;
    } else {
        gsi_base = 0;
        console_write("ioapic: using default qemu address\n");
    }

    if (!vmm_map_page(IOAPIC_VIRTUAL_BASE,
            physical,
            VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) {
        console_write("ioapic: mapping failed\n");
        return false;
    }

    ioapic = (volatile uint32_t *)IOAPIC_VIRTUAL_BASE;
    max_redirection_entry = (read_reg(IOAPIC_VER) >> 16) & 0xff;
    if(max_redirection_entry>119) {
        console_write("ioapic: unsupported redirection register range\n");ioapic=0;return false;
    }

    console_printf("ioapic: id=%u max_redir=%u phys=%x gsi_base=%u\n",
        (uint64_t)((read_reg(IOAPIC_ID) >> 24) & 0xf),
        (uint64_t)max_redirection_entry,
        (uint64_t)physical,
        (uint64_t)gsi_base);

    for (uint32_t i = 0; i <= max_redirection_entry; i++) {
        write_reg(IOAPIC_REDTBL_BASE + i * 2, IOAPIC_REDIR_MASKED);
        write_reg(IOAPIC_REDTBL_BASE + i * 2 + 1, 0);
    }

    return true;
}

bool ioapic_route_isa_irq(uint8_t irq, uint8_t vector) {
    uint32_t gsi = irq;
    uint16_t acpi_flags = 0;
    const struct acpi_interrupt_source_override *override = acpi_irq_override(irq);

    if (override != 0) {
        gsi = override->gsi;
        acpi_flags = override->flags;
    }

    return route_gsi(irq, gsi, vector, acpi_flags);
}

bool ioapic_route_pci_irq(uint8_t irq, uint8_t vector) {
    if (irq >= 16) {
        return false;
    }

    return route_gsi(irq, irq, vector, 0x000f);
}

static bool gsi_register(uint32_t gsi,uint8_t *reg) {
    if(!ioapic || gsi<gsi_base || gsi-gsi_base>max_redirection_entry || gsi-gsi_base>119)return false;
    *reg=IOAPIC_REDTBL_BASE+2*(gsi-gsi_base);return true;
}
bool ioapic_read_gsi(uint32_t gsi,uint32_t *low,uint32_t *high) {
    uint8_t reg;if(!low || !high || !gsi_register(gsi,&reg))return false;
    uint64_t f=lock();*low=read_reg(reg);*high=read_reg(reg+1);unlock(f);return true;
}
bool ioapic_prepare_gsi(uint32_t gsi,uint8_t vector,bool active_low,bool level) {
    uint8_t reg;if(vector<IRQ_VECTOR_BASE || vector>=IRQ_VECTOR_BASE+IRQ_COUNT || !gsi_register(gsi,&reg))return false;
    uint64_t f=lock();uint32_t old=read_reg(reg);
    if(!(old&IOAPIC_REDIR_MASKED) || (old&((1u<<12)|(1u<<14)))) { unlock(f);return false; }
    uint32_t low=vector|IOAPIC_REDIR_MASKED|(active_low?IOAPIC_REDIR_ACTIVE_LOW:0)|(level?IOAPIC_REDIR_TRIGGER_LEVEL:0);
    write_reg(reg,old|IOAPIC_REDIR_MASKED);write_reg(reg+1,0);write_reg(reg,low);
    bool ok=(read_reg(reg)&(0xfff|IOAPIC_REDIR_MASKED|IOAPIC_REDIR_ACTIVE_LOW|IOAPIC_REDIR_TRIGGER_LEVEL))==low && read_reg(reg+1)==0;
    unlock(f);return ok;
}
bool ioapic_mask_gsi(uint32_t gsi,bool masked) {
    uint8_t reg;if(!gsi_register(gsi,&reg))return false;
    uint64_t f=lock();uint32_t low=read_reg(reg);
    if(!masked && ((low&0xff)<IRQ_VECTOR_BASE || (low&0xff)>=IRQ_VECTOR_BASE+IRQ_COUNT)) { unlock(f);return false; }
    write_reg(reg,masked?low|IOAPIC_REDIR_MASKED:low&~IOAPIC_REDIR_MASKED);
    bool ok=((read_reg(reg)&IOAPIC_REDIR_MASKED)!=0)==masked;
    if(!ok)write_reg(reg,read_reg(reg)|IOAPIC_REDIR_MASKED);
    unlock(f);return ok;
}
