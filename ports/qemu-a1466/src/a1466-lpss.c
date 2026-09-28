/*
 * Apple MacBook Air A1466 Broadwell LPSS input rehearsal devices
 *
 * This is a deliberately small functional model for operating-system driver
 * development. It models the PCI identities, PXA2xx-compatible SPI register
 * window, and the Broadwell/LPT LPSS private register window used by SrvrOS.
 */

#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pci_ids.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "ui/input.h"

#define TYPE_A1466_LPSS_SPI "a1466-lpss-spi"
#define TYPE_A1466_LPSS_DMA "a1466-lpss-dma"

OBJECT_DECLARE_SIMPLE_TYPE(A1466LpssSpiState, A1466_LPSS_SPI)
OBJECT_DECLARE_SIMPLE_TYPE(A1466LpssDmaState, A1466_LPSS_DMA)

#define A1466_INTEL_VENDOR_ID 0x8086
#define A1466_LPSS_SPI1_DEVICE_ID 0x9cba
#define A1466_LPSS_DMA_DEVICE_ID 0x9ce0

#define A1466_SPI_BAR_SIZE 0x1000
#define A1466_DMA_BAR_SIZE 0x1000

#define PXA_SSCR0 0x00
#define PXA_SSCR1 0x04
#define PXA_SSSR  0x08
#define PXA_SSITR 0x0c
#define PXA_SSDR  0x10
#define PXA_SSTO  0x28
#define PXA_SSPSP 0x2c

#define PXA_SSCR0_SSE (1u << 7)
#define PXA_SSSR_TNF  (1u << 2)
#define PXA_SSSR_RNE  (1u << 3)
#define PXA_SSSR_BSY  (1u << 4)

#define APPLE_SPI_PACKET_SIZE 256
#define APPLE_SPI_QUEUE_SIZE 64
#define APPLE_SPI_READ_PACKET 0x20
#define APPLE_SPI_DEVICE_KEYBOARD 0x01
#define APPLE_SPI_MESSAGE_KEYBOARD 0x0110

#define LPSS_PRIVATE_BASE 0x800
#define LPSS_PRIV_CS_CTRL (LPSS_PRIVATE_BASE + 0x18)

struct A1466LpssSpiState {
    PCIDevice parent_obj;
    MemoryRegion mmio;
    uint32_t sscr0;
    uint32_t sscr1;
    uint32_t ssitr;
    uint32_t ssto;
    uint32_t sspsp;
    uint32_t private_regs[8];
    QemuInputHandlerState *input_handler;
    uint8_t packets[APPLE_SPI_QUEUE_SIZE][APPLE_SPI_PACKET_SIZE];
    uint8_t queue_head;
    uint8_t queue_count;
    uint16_t transfer_offset;
    uint8_t rx_byte;
    bool rx_ready;
    uint8_t modifiers;
    uint8_t keys[6];
    uint8_t message_counter;
};

struct A1466LpssDmaState {
    PCIDevice parent_obj;
    MemoryRegion mmio;
};

static void put_le16(uint8_t *destination, uint16_t value)
{
    destination[0] = value;
    destination[1] = value >> 8;
}

static uint16_t apple_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; bit++) {
            crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1;
        }
    }
    return crc;
}

static void a1466_enqueue_keyboard_packet(A1466LpssSpiState *s)
{
    uint8_t *packet;
    uint8_t *message;
    uint8_t *keyboard;
    uint8_t slot;
    uint16_t crc;

    if (s->queue_count == APPLE_SPI_QUEUE_SIZE) {
        s->queue_head = (s->queue_head + 1) % APPLE_SPI_QUEUE_SIZE;
        s->queue_count--;
        s->transfer_offset = 0;
        s->rx_ready = false;
    }

    slot = (s->queue_head + s->queue_count) % APPLE_SPI_QUEUE_SIZE;
    packet = s->packets[slot];
    memset(packet, 0, APPLE_SPI_PACKET_SIZE);
    packet[0] = APPLE_SPI_READ_PACKET;
    packet[1] = APPLE_SPI_DEVICE_KEYBOARD;
    put_le16(&packet[6], 20);

    message = &packet[8];
    put_le16(message, APPLE_SPI_MESSAGE_KEYBOARD);
    message[3] = s->message_counter++;
    put_le16(&message[6], 12);

    keyboard = &message[8];
    keyboard[1] = s->modifiers;
    memcpy(&keyboard[3], s->keys, sizeof(s->keys));
    crc = apple_crc16(keyboard, 10);
    put_le16(&keyboard[10], crc);

    crc = apple_crc16(packet, APPLE_SPI_PACKET_SIZE - 2);
    put_le16(&packet[APPLE_SPI_PACKET_SIZE - 2], crc);
    s->queue_count++;
}

static uint8_t qcode_to_hid(QKeyCode qcode)
{
    switch (qcode) {
    case Q_KEY_CODE_A: return 0x04;
    case Q_KEY_CODE_B: return 0x05;
    case Q_KEY_CODE_C: return 0x06;
    case Q_KEY_CODE_D: return 0x07;
    case Q_KEY_CODE_E: return 0x08;
    case Q_KEY_CODE_F: return 0x09;
    case Q_KEY_CODE_G: return 0x0a;
    case Q_KEY_CODE_H: return 0x0b;
    case Q_KEY_CODE_I: return 0x0c;
    case Q_KEY_CODE_J: return 0x0d;
    case Q_KEY_CODE_K: return 0x0e;
    case Q_KEY_CODE_L: return 0x0f;
    case Q_KEY_CODE_M: return 0x10;
    case Q_KEY_CODE_N: return 0x11;
    case Q_KEY_CODE_O: return 0x12;
    case Q_KEY_CODE_P: return 0x13;
    case Q_KEY_CODE_Q: return 0x14;
    case Q_KEY_CODE_R: return 0x15;
    case Q_KEY_CODE_S: return 0x16;
    case Q_KEY_CODE_T: return 0x17;
    case Q_KEY_CODE_U: return 0x18;
    case Q_KEY_CODE_V: return 0x19;
    case Q_KEY_CODE_W: return 0x1a;
    case Q_KEY_CODE_X: return 0x1b;
    case Q_KEY_CODE_Y: return 0x1c;
    case Q_KEY_CODE_Z: return 0x1d;
    case Q_KEY_CODE_1: return 0x1e;
    case Q_KEY_CODE_2: return 0x1f;
    case Q_KEY_CODE_3: return 0x20;
    case Q_KEY_CODE_4: return 0x21;
    case Q_KEY_CODE_5: return 0x22;
    case Q_KEY_CODE_6: return 0x23;
    case Q_KEY_CODE_7: return 0x24;
    case Q_KEY_CODE_8: return 0x25;
    case Q_KEY_CODE_9: return 0x26;
    case Q_KEY_CODE_0: return 0x27;
    case Q_KEY_CODE_RET: return 0x28;
    case Q_KEY_CODE_ESC: return 0x29;
    case Q_KEY_CODE_BACKSPACE: return 0x2a;
    case Q_KEY_CODE_TAB: return 0x2b;
    case Q_KEY_CODE_SPC: return 0x2c;
    case Q_KEY_CODE_MINUS: return 0x2d;
    case Q_KEY_CODE_EQUAL: return 0x2e;
    case Q_KEY_CODE_BRACKET_LEFT: return 0x2f;
    case Q_KEY_CODE_BRACKET_RIGHT: return 0x30;
    case Q_KEY_CODE_BACKSLASH: return 0x31;
    case Q_KEY_CODE_SEMICOLON: return 0x33;
    case Q_KEY_CODE_APOSTROPHE: return 0x34;
    case Q_KEY_CODE_GRAVE_ACCENT: return 0x35;
    case Q_KEY_CODE_COMMA: return 0x36;
    case Q_KEY_CODE_DOT: return 0x37;
    case Q_KEY_CODE_SLASH: return 0x38;
    case Q_KEY_CODE_HOME: return 0x4a;
    case Q_KEY_CODE_END: return 0x4d;
    case Q_KEY_CODE_RIGHT: return 0x4f;
    case Q_KEY_CODE_LEFT: return 0x50;
    case Q_KEY_CODE_DOWN: return 0x51;
    case Q_KEY_CODE_UP: return 0x52;
    case Q_KEY_CODE_DELETE: return 0x4c;
    default: return 0;
    }
}

static int qcode_modifier_bit(QKeyCode qcode)
{
    switch (qcode) {
    case Q_KEY_CODE_CTRL: return 0;
    case Q_KEY_CODE_SHIFT: return 1;
    case Q_KEY_CODE_ALT: return 2;
    case Q_KEY_CODE_META_L: return 3;
    case Q_KEY_CODE_CTRL_R: return 4;
    case Q_KEY_CODE_SHIFT_R: return 5;
    case Q_KEY_CODE_ALT_R: return 6;
    case Q_KEY_CODE_META_R: return 7;
    default: return -1;
    }
}

static void a1466_keyboard_event(DeviceState *dev, QemuConsole *src,
                                 InputEvent *event)
{
    A1466LpssSpiState *s = A1466_LPSS_SPI(dev);
    InputKeyEvent *key = event->u.key.data;
    QKeyCode qcode = qemu_input_key_value_to_qcode(key->key);
    int modifier = qcode_modifier_bit(qcode);
    uint8_t usage;

    if (modifier >= 0) {
        if (key->down) {
            s->modifiers |= 1u << modifier;
        } else {
            s->modifiers &= ~(1u << modifier);
        }
        a1466_enqueue_keyboard_packet(s);
        return;
    }

    usage = qcode_to_hid(qcode);
    if (usage == 0) {
        return;
    }
    if (key->down) {
        for (unsigned i = 0; i < ARRAY_SIZE(s->keys); i++) {
            if (s->keys[i] == usage) {
                return;
            }
            if (s->keys[i] == 0) {
                s->keys[i] = usage;
                a1466_enqueue_keyboard_packet(s);
                return;
            }
        }
        return;
    }
    for (unsigned i = 0; i < ARRAY_SIZE(s->keys); i++) {
        if (s->keys[i] == usage) {
            memmove(&s->keys[i], &s->keys[i + 1],
                    ARRAY_SIZE(s->keys) - i - 1);
            s->keys[ARRAY_SIZE(s->keys) - 1] = 0;
            a1466_enqueue_keyboard_packet(s);
            return;
        }
    }
}

static const QemuInputHandler a1466_keyboard_handler = {
    .name = "A1466 Apple SPI Topcase Keyboard",
    .mask = INPUT_EVENT_MASK_KEY,
    .event = a1466_keyboard_event,
};

static uint64_t a1466_spi_read(void *opaque, hwaddr addr, unsigned size)
{
    A1466LpssSpiState *s = opaque;

    if (size != 4) {
        return UINT32_MAX;
    }

    switch (addr) {
    case PXA_SSCR0:
        return s->sscr0;
    case PXA_SSCR1:
        return s->sscr1;
    case PXA_SSSR:
        return PXA_SSSR_TNF |
               (s->rx_ready || s->queue_count ? PXA_SSSR_RNE : 0) |
               (s->transfer_offset ? PXA_SSSR_BSY : 0);
    case PXA_SSITR:
        return s->ssitr;
    case PXA_SSDR:
        if (s->rx_ready) {
            s->rx_ready = false;
            return s->rx_byte;
        }
        return 0;
    case PXA_SSTO:
        return s->ssto;
    case PXA_SSPSP:
        return s->sspsp;
    default:
        if (addr >= LPSS_PRIVATE_BASE &&
            addr < LPSS_PRIVATE_BASE + sizeof(s->private_regs)) {
            return s->private_regs[(addr - LPSS_PRIVATE_BASE) / 4];
        }
        return 0;
    }
}

static void a1466_spi_write(void *opaque, hwaddr addr, uint64_t value,
                            unsigned size)
{
    A1466LpssSpiState *s = opaque;
    uint32_t val = value;

    if (size != 4) {
        return;
    }

    switch (addr) {
    case PXA_SSCR0:
        s->sscr0 = val;
        break;
    case PXA_SSCR1:
        s->sscr1 = val;
        break;
    case PXA_SSSR:
        /* Status bits are synthesized; acknowledge writes are harmless. */
        break;
    case PXA_SSITR:
        s->ssitr = val;
        break;
    case PXA_SSDR:
        if (!(s->sscr0 & PXA_SSCR0_SSE)) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "a1466-lpss-spi: SSDR write while disabled\n");
        }
        if (s->queue_count && !s->rx_ready) {
            s->rx_byte = s->packets[s->queue_head][s->transfer_offset++];
            s->rx_ready = true;
            if (s->transfer_offset == APPLE_SPI_PACKET_SIZE) {
                s->queue_head = (s->queue_head + 1) % APPLE_SPI_QUEUE_SIZE;
                s->queue_count--;
                s->transfer_offset = 0;
            }
        }
        break;
    case PXA_SSTO:
        s->ssto = val;
        break;
    case PXA_SSPSP:
        s->sspsp = val;
        break;
    default:
        if (addr >= LPSS_PRIVATE_BASE &&
            addr < LPSS_PRIVATE_BASE + sizeof(s->private_regs)) {
            s->private_regs[(addr - LPSS_PRIVATE_BASE) / 4] = val;
        }
        break;
    }
}

static const MemoryRegionOps a1466_spi_ops = {
    .read = a1466_spi_read,
    .write = a1466_spi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t a1466_dma_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}

static void a1466_dma_write(void *opaque, hwaddr addr, uint64_t value,
                            unsigned size)
{
}

static const MemoryRegionOps a1466_dma_ops = {
    .read = a1466_dma_read,
    .write = a1466_dma_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void a1466_spi_reset(DeviceState *dev)
{
    A1466LpssSpiState *s = A1466_LPSS_SPI(dev);

    s->sscr0 = 0;
    s->sscr1 = 0;
    s->ssitr = 0;
    s->ssto = 0;
    s->sspsp = 0;
    memset(s->private_regs, 0, sizeof(s->private_regs));
    memset(s->packets, 0, sizeof(s->packets));
    memset(s->keys, 0, sizeof(s->keys));
    s->queue_head = 0;
    s->queue_count = 0;
    s->transfer_offset = 0;
    s->rx_byte = 0;
    s->rx_ready = false;
    s->modifiers = 0;
    s->message_counter = 0;

    /* The topcase starts deselected in software-controlled CS mode. */
    s->private_regs[(LPSS_PRIV_CS_CTRL - LPSS_PRIVATE_BASE) / 4] = 0x3;
}

static void a1466_spi_realize(PCIDevice *pdev, Error **errp)
{
    A1466LpssSpiState *s = A1466_LPSS_SPI(pdev);

    pci_config_set_interrupt_pin(pdev->config, 1);
    memory_region_init_io(&s->mmio, OBJECT(s), &a1466_spi_ops, s,
                          "a1466-lpss-spi-mmio", A1466_SPI_BAR_SIZE);
    pci_register_bar(pdev, 0,
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64,
                     &s->mmio);
    s->input_handler = qemu_input_handler_register(DEVICE(pdev),
                                                    &a1466_keyboard_handler);
    qemu_input_handler_activate(s->input_handler);
}

static void a1466_spi_exit(PCIDevice *pdev)
{
    A1466LpssSpiState *s = A1466_LPSS_SPI(pdev);

    if (s->input_handler) {
        qemu_input_handler_unregister(s->input_handler);
        s->input_handler = NULL;
    }
}

static void a1466_dma_realize(PCIDevice *pdev, Error **errp)
{
    A1466LpssDmaState *s = A1466_LPSS_DMA(pdev);

    pci_config_set_interrupt_pin(pdev->config, 1);
    memory_region_init_io(&s->mmio, OBJECT(s), &a1466_dma_ops, s,
                          "a1466-lpss-dma-mmio", A1466_DMA_BAR_SIZE);
    pci_register_bar(pdev, 0,
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64,
                     &s->mmio);
}

static void a1466_spi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->realize = a1466_spi_realize;
    pc->exit = a1466_spi_exit;
    pc->vendor_id = A1466_INTEL_VENDOR_ID;
    pc->device_id = A1466_LPSS_SPI1_DEVICE_ID;
    pc->revision = 4;
    pc->class_id = PCI_CLASS_COMMUNICATION_OTHER;
    dc->desc = "A1466 Broadwell LPSS SPI1 rehearsal device";
    device_class_set_legacy_reset(dc, a1466_spi_reset);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static void a1466_dma_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->realize = a1466_dma_realize;
    pc->vendor_id = A1466_INTEL_VENDOR_ID;
    pc->device_id = A1466_LPSS_DMA_DEVICE_ID;
    pc->revision = 4;
    pc->class_id = PCI_CLASS_SYSTEM_DMA;
    dc->desc = "A1466 Broadwell LPSS DMA rehearsal device";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo a1466_lpss_types[] = {
    {
        .name = TYPE_A1466_LPSS_SPI,
        .parent = TYPE_PCI_DEVICE,
        .instance_size = sizeof(A1466LpssSpiState),
        .class_init = a1466_spi_class_init,
        .interfaces = (const InterfaceInfo[]) {
            { INTERFACE_CONVENTIONAL_PCI_DEVICE },
            { },
        },
    },
    {
        .name = TYPE_A1466_LPSS_DMA,
        .parent = TYPE_PCI_DEVICE,
        .instance_size = sizeof(A1466LpssDmaState),
        .class_init = a1466_dma_class_init,
        .interfaces = (const InterfaceInfo[]) {
            { INTERFACE_CONVENTIONAL_PCI_DEVICE },
            { },
        },
    },
};

DEFINE_TYPES(a1466_lpss_types)
