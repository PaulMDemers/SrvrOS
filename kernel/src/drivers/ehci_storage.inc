/* One synchronous high-speed BOT transfer. The controller owns both DMA
 * pages until the async schedule stops, or until controller retirement. */
#define EHCI_FLASH_BULK_PACKET 512u
#define EHCI_FLASH_BULK_IN 0x81u
#define EHCI_FLASH_BULK_OUT 0x02u
#define EHCI_BULK_WAIT_US 5000000u
#define EHCI_BULK_POLL_US 100u

_Static_assert(USB_STORAGE_BULK_BYTES <= EHCI_PAGE_BYTES, "bulk buffer must fit in its DMA page");

struct ehci_flash_io {
    struct ehci_control_path *path;
    uint32_t physical;
    uint8_t *buffer;
    uint32_t endpoint_state[2];
};

static struct ehci_desktop_workspace {
    struct ehci_probe_report report;
    struct ehci_control_path path;
    struct ehci_flash_io flash;
    struct usb_storage storage;
    const struct pci_device *device;
    uint64_t root, page, bulk_page;
    uint16_t memory_command;
    bool unsafe;
} desktop_workspace;

static bool captured_flash_interface(const uint8_t *config, uint16_t length) {
    if (!config || length < 9 || config[4] != 1 || config[5] != 1) return false;
    bool interface = false, in = false, out = false;
    for (unsigned offset = 9; offset < length;) {
        if (length - offset < 2) return false;
        const uint8_t *entry = config + offset;
        unsigned size = entry[0];
        if (size < 2 || size > length - offset) return false;
        if (entry[1] == USB_DESC_INTERFACE) {
            if (interface || size != 9 || entry[2] || entry[3] || entry[4] != 2 ||
                entry[5] != 8 || entry[6] != 6 || entry[7] != 0x50) return false;
            interface = true;
        } else if (entry[1] == USB_DESC_ENDPOINT) {
            if (!interface || size != 7 || entry[3] != 2 || entry[4] || entry[5] != 2) return false;
            if (entry[2] == EHCI_FLASH_BULK_IN && !in) in = true;
            else if (entry[2] == EHCI_FLASH_BULK_OUT && !out) out = true;
            else return false;
        }
        offset += size;
    }
    return in && out;
}

static bool flash_bulk(void *context, bool in, void *data, uint32_t length, uint32_t *actual) {
    struct ehci_flash_io *flash = context;
    struct ehci_control_path *path = flash->path;
    volatile uint32_t *op = path->op;
    *actual = 0;
    if (!length || length > USB_STORAGE_BULK_BYTES || !schedules_idle(path)) return false;
    struct ehci_dma *dma = path->dma;
    const volatile struct ehci_dma *live = path->dma;
    unsigned ep = (in ? EHCI_FLASH_BULK_IN : EHCI_FLASH_BULK_OUT) & 15;
    uint32_t queue = path->physical + offsetof(struct ehci_dma, qh);
    uint32_t transfer = path->physical + offsetof(struct ehci_dma, setup_td);
    *dma = (struct ehci_dma){0};
    dma->qh.link = queue | EHCI_LINK_QH;
    dma->qh.endpoint = path->report->flash_address | (ep << 8) | EHCI_QH_SPEED_HIGH |
        EHCI_QH_HEAD | (EHCI_FLASH_BULK_PACKET << EHCI_QH_PACKET_SHIFT);
    dma->qh.capabilities = EHCI_QH_MULT_ONE;
    dma->qh.overlay.next = transfer;
    dma->qh.overlay.alternate = EHCI_LINK_END;
    /* The high-speed OUT endpoint can carry PING state across requests. */
    dma->qh.overlay.token = flash->endpoint_state[in];
    dma->setup_td.next = dma->setup_td.alternate = EHCI_LINK_END;
    dma->setup_td.token = (length << EHCI_TD_BYTES_SHIFT) | (in ? EHCI_TD_PID_IN : EHCI_TD_PID_OUT) |
        EHCI_TD_RETRIES | EHCI_TD_ACTIVE;
    dma->setup_td.buffer[0] = flash->physical;
    uint8_t *bytes = data;
    if (!in) for (uint32_t i = 0; i < length; i++) flash->buffer[i] = bytes[i];
    __asm__ volatile ("mfence" : : : "memory");
    op[EHCI_USBCMD] |= EHCI_CMD_ASYNC;
    bool enabled = wait_mmio(&op[EHCI_USBSTS], EHCI_STS_ASYNC, EHCI_STS_ASYNC, EHCI_HALT_US);
    bool completed = false;
    if (enabled) {
        uint64_t start = timer_monotonic_us();
        do {
            uint32_t token = live->setup_td.token;
            if ((op[EHCI_USBSTS] & (EHCI_STS_HALTED | EHCI_STS_HOST_ERROR)) || (token & EHCI_TD_ERROR)) break;
            if (!(token & EHCI_TD_ACTIVE)) { completed = true; break; }
        } while (timer_delay_us(EHCI_BULK_POLL_US) && timer_monotonic_us() - start < EHCI_BULK_WAIT_US);
    }
    op[EHCI_USBCMD] &= ~EHCI_CMD_ASYNC;
    if (!wait_mmio(&op[EHCI_USBSTS], EHCI_STS_ASYNC, 0, EHCI_HALT_US) || !completed) return false;
    __asm__ volatile ("mfence" : : : "memory");
    uint32_t residual = (live->setup_td.token >> EHCI_TD_BYTES_SHIFT) & EHCI_TD_BYTES_MASK;
    if (residual > length || (!in && residual)) return false;
    flash->endpoint_state[in] = live->qh.overlay.token & (EHCI_TD_DATA1 | (in ? 0u : 1u));
    *actual = length - residual;
    if (in) for (uint32_t i = 0; i < *actual; i++) bytes[i] = flash->buffer[i];
    return true;
}

static bool prepare_flash_io(struct ehci_control_path *path, struct ehci_flash_io *flash, uint64_t *bulk_page) {
    struct ehci_probe_report *report = path->report;
    if (!report->flash_target.address) {
        console_write("ehci-log: captured flash interface unavailable\n");
        return false;
    }
    const struct usb_setup_packet configuration = { 0, USB_REQ_SET_CONFIGURATION, 1, 0, 0 };
    unsigned received;
    if (!control_transfer(path, &report->flash_target, &configuration, NULL, &received)) {
        console_write("ehci-log: SET_CONFIGURATION failed\n");
        return false;
    }
    uint64_t physical = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    if (!physical || physical > UINT32_MAX) {
        if (physical) pmm_free_frame(physical);
        console_write("ehci-log: no 32-bit bulk DMA page\n");
        return false;
    }
    *bulk_page = physical;
    *flash = (struct ehci_flash_io) { .path = path, .physical = (uint32_t)physical, .buffer = pmm_phys_to_virt(physical) };
    return true;
}

#define EHCI_TRACKPAD_REPORT "/work/srvros-work/ehci-trackpad-0000000000000000-startup.txt"
#define EHCI_ETHERNET_REPORT "/work/srvros-work/ehci-ethernet-0000000000000000-startup.txt"
#define EHCI_BCM4331_REPORT "/work/srvros-work/ehci-wifi4331-0000000000000000-startup.txt"
#define EHCI_LID_REPORT "/work/srvros-work/ehci-lidstate-0000000000000000-startup.txt"

static bool persist_flash_report(const char *release, bool startup, const char *path) {
    char filename[] = EHCI_TRACKPAD_REPORT;
    _Static_assert(sizeof(EHCI_TRACKPAD_REPORT) == sizeof(EHCI_ETHERNET_REPORT) &&
        sizeof(EHCI_TRACKPAD_REPORT) == sizeof(EHCI_BCM4331_REPORT) &&
        sizeof(EHCI_TRACKPAD_REPORT) == sizeof(EHCI_LID_REPORT), "report path lengths");
    for (unsigned i = 0; i < sizeof filename; i++) filename[i] = path[i];
    const unsigned release_offset = sizeof("/work/srvros-work/ehci-trackpad-") - 1;
    for (unsigned i = 0; i < 16; i++) filename[release_offset + i] = release[i];
    if (!startup) {
        static const char suffix[] = ".txt";
        for (unsigned i = 0; i < sizeof(suffix); i++) filename[release_offset + 16 + i] = suffix[i];
    }
    return !vfs_lookup(filename) && bootlog_persist(filename);
}

static void run_flash_log(struct ehci_control_path *path, uint64_t *bulk_page) {
    struct ehci_probe_report *report = path->report;
    struct ehci_flash_io flash;
    if (!prepare_flash_io(path, &flash, bulk_page)) return;
    static struct usb_storage startup_storage, final_storage;
    struct usb_storage *storage = report->log_startup ? &startup_storage : &final_storage;
    if (usb_storage_attach(storage, &flash, flash_bulk, false) && workspace_mount(report->log_volume)) {
        bool written = persist_flash_report(report->log_release, report->log_startup, EHCI_TRACKPAD_REPORT);
        bool unmounted = workspace_unmount();
        report->log_saved = written && unmounted;
        if (report->log_saved) console_printf("ehci-log: saved /work/srvros-work/ehci-trackpad-%s%s.txt\n",
            report->log_release, report->log_startup ? "-startup" : "");
    }
    if (!report->log_saved) console_write("ehci-log: result file could not be saved and unmounted\n");
    if (storage->block && storage->failed) usb_storage_print_status(storage->block);
    if (!usb_storage_retire(storage)) report->log_saved = false;
}
