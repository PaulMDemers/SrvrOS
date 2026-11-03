/* Isolated driver protocol/lifetime test; transport state-machine tests are in
 * test_virtio_pci.c. Compile with kernel/include + Limine protocol includes. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VIRTIO_BLOCK_HOST_TEST
#define SRVROS_SERVICE_WINDOW_HOST_TEST
#include "../kernel/src/drivers/virtio_block.c"

#define TEST_FRAMES 10u
#define TEST_DMA 0x100000ull
#define DISK_BYTES 65536u
struct virtio_pci_device { unsigned index; bool active, stopped, pending; };
static struct virtio_pci_device transports[2];
static struct pci_device pci_devices[3];
static struct block_device blocks[2];
static _Alignas(16) uint8_t memory[TEST_FRAMES][4096];
static uint8_t media[2][DISK_BYTES];
static bool live[TEST_FRAMES];
static struct virtio_pci_buffer posted[2][3];
static unsigned posted_count[2], submitted[3], stop_calls, free_calls, allocated;
static unsigned registered, available_pci, setup_calls, start_calls, checkpoint_calls;
static unsigned fail_allocation, allocation_calls, delay_polls, polls, io_depth;
static uint64_t cr3, expected_cr3, features, capacity, clock_value, clock_step;
static bool irqs, service_available, foreign_gate, nested_attempt, nested_refused;
static bool fail_open, fail_setup, fail_start, fail_register, fail_stop, fail_submit;
static bool poll_failure, bad_token, short_length, oversized_length, never_complete, timer_ready;
static uint8_t device_status;

uint64_t vmm_kernel_address_space(void) { return 0x1000; }
uint64_t vmm_current_address_space(void) { return cr3; }
void vmm_switch_address_space(uint64_t value) { assert(!irqs && value); cr3 = value; }
uint64_t service_window_test_irq_save(void) { uint64_t flags = irqs ? 512 : 0; irqs = false; return flags; }
void service_window_test_irq_restore(uint64_t flags) { irqs = (flags & 512) != 0; }
bool scheduler_io_begin(void) {
    assert(!irqs);
    if (!service_available || foreign_gate) return false;
    ++io_depth; return true;
}
bool scheduler_io_active(void) { return foreign_gate || io_depth; }
void scheduler_io_end(void) { assert(!irqs && io_depth); --io_depth; }
void scheduler_io_checkpoint(void) {
    assert(!irqs && io_depth && cr3 == expected_cr3);
    ++checkpoint_calls;
    if (nested_attempt) {
        nested_attempt = false;
        uint8_t data[512];
        unsigned before = submitted[0] + submitted[1] + submitted[2];
        nested_refused = !blocks[0].read(&blocks[0], 0, data, sizeof(data));
        assert(before == submitted[0] + submitted[1] + submitted[2]);
    }
}
bool timer_reference_ready(void) { return timer_ready; }
uint64_t timer_monotonic_us(void) { clock_value += clock_step; return clock_value; }
void virtio_block_test_pause(void) {}
void console_printf(const char *format, ...) { (void)format; }
uint64_t pci_device_count(void) { return available_pci; }
const struct pci_device *pci_device_at(uint64_t index) { return index < available_pci ? &pci_devices[index] : NULL; }
static void transport_context(void) { assert(cr3 == 0x1000 && !irqs); }
bool virtio_pci_open_boot_block(const struct pci_device *pci, uint64_t allowed,
    uint64_t required, uint32_t min, struct virtio_pci_device **out) {
    transport_context(); assert(allowed == (VBLK_F_RO | VBLK_F_FLUSH) && !required && min == 8);
    assert(pci->device < 2); *out = &transports[pci->device];
    if (fail_open) { (*out)->stopped = true; return false; }
    return true;
}
void virtio_pci_get_admission(const struct virtio_pci_device *t, struct virtio_pci_admission *out) { (void)t; *out = (struct virtio_pci_admission){0}; }
enum virtio_pci_status virtio_pci_get_status(const struct virtio_pci_device *t) { (void)t; return VIRTIO_PCI_OK; }
const char *virtio_pci_status_name(enum virtio_pci_status status) { (void)status; return "ok"; }
uint64_t virtio_pci_features(const struct virtio_pci_device *t) { (void)t; transport_context(); return features | VIRTIO_PCI_VERSION_1; }
bool virtio_pci_config_read64(struct virtio_pci_device *t, uint32_t offset, uint64_t *value) {
    (void)t; transport_context(); assert(!offset); *value = capacity; return true;
}
bool virtio_pci_queue_setup(struct virtio_pci_device *t, uint16_t queue, uint16_t size) {
    (void)t; transport_context(); assert(queue == 0 && size == 8); ++setup_calls; return !fail_setup;
}
bool virtio_pci_start(struct virtio_pci_device *t) {
    transport_context(); ++start_calls; if (fail_start) return false; t->active = true; return true;
}
bool virtio_pci_stop(struct virtio_pci_device *t) {
    transport_context(); ++stop_calls;
    if (fail_stop) return false;
    t->active = false; t->stopped = true; t->pending = false; return true;
}
bool virtio_pci_submit(struct virtio_pci_device *t, uint16_t q,
    const struct virtio_pci_buffer *buffers, uint16_t count, uint16_t *token) {
    transport_context(); assert(q == 0 && t->active && !t->pending && (count == 2 || count == 3));
    if (fail_submit) return false;
    struct block_request *request = pmm_phys_to_virt(buffers[0].physical);
    assert(buffers[0].length == 16 && !buffers[0].device_write && !request->reserved);
    assert(buffers[count - 1].length == 1 && buffers[count - 1].device_write);
    assert(*(uint8_t *)pmm_phys_to_virt(buffers[count - 1].physical) == 0xff);
    if (request->type == VBLK_FLUSH) {
        assert(count == 2 && !request->sector && (features & VBLK_F_FLUSH)); ++submitted[2];
    } else {
        assert(count == 3 && buffers[1].length && !(buffers[1].length % 512) && buffers[1].length <= VBLK_BYTES);
        assert(request->sector * 512 + buffers[1].length <= DISK_BYTES);
        assert(buffers[1].device_write == (request->type == VBLK_IN));
        assert(request->type == VBLK_IN || (request->type == VBLK_OUT && !(features & VBLK_F_RO)));
        ++submitted[request->type];
    }
    memcpy(posted[t->index], buffers, sizeof(*buffers) * count); posted_count[t->index] = count;
    t->pending = true; polls = 0; *token = 5; return true;
}
int virtio_pci_poll(struct virtio_pci_device *t, uint16_t q, struct virtio_pci_completion *completion) {
    transport_context(); assert(q == 0 && t->active && t->pending);
    if (poll_failure) return -1;
    if (never_complete || ++polls <= delay_polls) return 0;
    struct virtio_pci_buffer *buffers = posted[t->index];
    struct block_request *request = pmm_phys_to_virt(buffers[0].physical);
    unsigned count = posted_count[t->index];
    if (count == 3 && device_status == 0) {
        void *bounce = pmm_phys_to_virt(buffers[1].physical);
        uint8_t *disk = media[t->index] + request->sector * 512;
        if (request->type == VBLK_IN) memcpy(bounce, disk, buffers[1].length);
        else memcpy(disk, bounce, buffers[1].length);
    }
    *(uint8_t *)pmm_phys_to_virt(buffers[count - 1].physical) = device_status;
    completion->token = bad_token ? 7 : 5;
    completion->length = request->type == VBLK_IN ? buffers[1].length + 1 : 1;
    if (short_length) --completion->length;
    if (oversized_length) ++completion->length;
    t->pending = false; return 1;
}
uint64_t pmm_alloc_frames_tagged(uint64_t count, enum pmm_frame_tag tag) {
    assert(tag == PMM_FRAME_DMA && count && count <= 4); transport_context();
    ++allocation_calls; if (allocation_calls == fail_allocation) return 0;
    for (unsigned i = 0; i + count <= TEST_FRAMES; ++i) {
        bool free = true; for (unsigned j = 0; j < count; ++j) if (live[i + j]) free = false;
        if (!free) continue;
        for (unsigned j = 0; j < count; ++j) { live[i + j] = true; ++allocated; }
        return TEST_DMA + i * 4096;
    }
    return 0;
}
uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag) { return pmm_alloc_frames_tagged(1, tag); }
void *pmm_phys_to_virt(uint64_t physical) {
    assert(physical >= TEST_DMA && physical < TEST_DMA + TEST_FRAMES * 4096);
    unsigned i = (unsigned)((physical - TEST_DMA) / 4096);
    assert(live[i]); return memory[i] + (physical & 4095);
}
void pmm_free_frame_range(uint64_t physical, uint64_t count) {
    transport_context(); assert(stop_calls && !fail_stop);
    unsigned i = (unsigned)((physical - TEST_DMA) / 4096);
    for (unsigned j = 0; j < count; ++j) { assert(i + j < TEST_FRAMES && live[i + j]); live[i + j] = false; ++free_calls; }
}
void pmm_free_frame(uint64_t physical) { pmm_free_frame_range(physical, 1); }
struct block_device *block_register(const char *name, uint64_t block_size, uint64_t sectors,
    block_read_fn read, block_write_fn write, void *data) {
    transport_context(); assert(block_size == 512 && sectors == capacity && registered < 2);
    if (fail_register) return NULL;
    struct block_device *b = &blocks[registered++];
    *b = (struct block_device){.used = true, .name = name, .block_size = block_size,
        .block_count = sectors, .read = read, .write = write, .private_data = data};
    return b;
}
void block_invalidate(struct block_device *block) { block->offline = true; }
uint64_t block_register_gpt_partitions(struct block_device *block) {
    assert(!io_depth && cr3 == 0x1000 && block->read); return 0;
}
uint64_t block_register_mbr_partitions(struct block_device *block) {
    assert(!io_depth && cr3 == 0x1000 && block->read); return 0;
}
static void fixture(void) {
    memset(disks, 0, sizeof(disks)); memset(blocks, 0, sizeof(blocks)); memset(transports, 0, sizeof(transports));
    memset(live, 0, sizeof(live)); memset(submitted, 0, sizeof(submitted));
    attempted = false; disk_count = 0;
    for (unsigned d = 0; d < 2; ++d) {
        transports[d].index = d;
        pci_devices[d] = (struct pci_device){.vendor_id = 0x1af4, .device_id = 0x1042, .device = (uint8_t)d};
        for (unsigned i = 0; i < DISK_BYTES; ++i) media[d][i] = (uint8_t)(i * 31 + (i >> 8));
    }
    available_pci = 1; cr3 = expected_cr3 = 0x1000; features = VBLK_F_FLUSH;
    capacity = DISK_BYTES / 512; clock_value = 0; clock_step = 1;
    irqs = service_available = timer_ready = true;
    foreign_gate = nested_attempt = nested_refused = false;
    fail_open = fail_setup = fail_start = fail_register = fail_stop = fail_submit = false;
    poll_failure = bad_token = short_length = oversized_length = never_complete = false;
    device_status = 0; stop_calls = free_calls = allocated = registered = setup_calls = start_calls = 0;
    checkpoint_calls = fail_allocation = allocation_calls = delay_polls = polls = io_depth = 0;
}
static struct block_device *start(void) {
    assert(virtio_block_init() == 1 && !io_depth && irqs && allocated == 5);
    assert(blocks[0].block_size == 512 && blocks[0].max_transfer_bytes == 16384);
    cr3 = expected_cr3 = 0x2000;
    return &blocks[0];
}
static void owner_restored(void) { assert(cr3 == expected_cr3 && irqs && !io_depth && !disks[0].busy); }
static void test_io(void) {
    fixture(); struct block_device *b = start();
    uint8_t data[VBLK_BYTES * 2], original[DISK_BYTES]; memcpy(original, media[0], sizeof(original));
    delay_polls = 128; nested_attempt = true;
    assert(b->read(b, 512, data, sizeof(data)) && !memcmp(data, original + 512, sizeof(data)));
    assert(submitted[0] == 2 && checkpoint_calls >= 5 && nested_refused); owner_restored();
    memset(data, 0x3c, sizeof(data)); delay_polls = 0;
    assert(b->write(b, 512, data, sizeof(data)) && !memcmp(media[0] + 512, data, sizeof(data)));
    assert(submitted[1] == 2 && submitted[2] == 0); owner_restored();
    assert(!memcmp(media[0], original, 512));
    assert(!memcmp(media[0] + 512 + sizeof(data), original + 512 + sizeof(data), DISK_BYTES - 512 - sizeof(data)));
    assert(b->flush(b) && submitted[2] == 1); owner_restored();
    assert(!free_calls && !stop_calls); virtio_block_print_status();
}
static void test_partial(void) {
    fixture(); struct block_device *b = start();
    uint8_t data[1025], before[DISK_BYTES]; memcpy(before, media[0], sizeof(before)); memset(data, 0xa3, sizeof(data));
    assert(b->write(b, 7, data, sizeof(data)));
    assert(submitted[0] == 2 && submitted[1] == 3);
    assert(!memcmp(media[0], before, 7) && !memcmp(media[0] + 7, data, sizeof(data)));
    assert(!memcmp(media[0] + 7 + sizeof(data), before + 7 + sizeof(data), DISK_BYTES - 7 - sizeof(data)));
    memset(data, 0, sizeof(data)); assert(b->read(b, 7, data, sizeof(data)));
    for (unsigned i = 0; i < sizeof(data); ++i) assert(data[i] == 0xa3);
    owner_restored();
    unsigned commands = (unsigned)disks[0].commands;
    assert(!b->read(b, DISK_BYTES - 4, data, 8)); assert(!b->write(b, UINT64_MAX, data, 1));
    assert(!b->read(b, 0, data, UINT64_MAX)); assert(!b->read(b, 0, NULL, 1));
    assert(b->read(b, DISK_BYTES, data, 0)); assert(disks[0].commands == commands); owner_restored();
}
static void test_read_only(void) {
    for (unsigned mode = 0; mode < 2; ++mode) {
        fixture(); features = mode ? VBLK_F_RO | VBLK_F_FLUSH : 0;
        struct block_device *b = start(); uint8_t data[512];
        assert(!b->write && !b->flush && b->read(b, 0, data, sizeof(data)));
        assert(!write_blocks(b, 0, data, sizeof(data)) && !flush_blocks(b));
        assert(!submitted[1] && !submitted[2]); owner_restored();
    }
}
static void test_faults(void) {
    for (unsigned mode = 0; mode < 10; ++mode) {
        fixture(); struct block_device *b = start(); uint8_t data[512]; memset(data, 0x7a, sizeof(data));
        if (mode == 0) fail_submit = true;
        if (mode == 1) poll_failure = true;
        if (mode == 2) bad_token = true;
        if (mode == 3) short_length = true;
        if (mode == 4) oversized_length = true;
        if (mode == 5) device_status = 1;
        if (mode == 6) device_status = 2;
        if (mode == 7) device_status = 0xff;
        if (mode == 8) { never_complete = true; clock_step = 1000000; }
        if (mode == 9) ++capacity;
        assert(!b->read(b, 0, data, sizeof(data)));
        for (unsigned i = 0; i < sizeof(data); ++i) assert(data[i] == 0x7a);
        assert(b->offline && disks[0].failed && stop_calls == 1 && free_calls == allocated && !disks[0].quarantined);
        assert(!b->read(b, 0, data, sizeof(data)) && !b->write(b, 0, data, sizeof(data)) && !b->flush(b));
        owner_restored();
    }
    for (unsigned write = 0; write < 2; ++write) {
        fixture(); struct block_device *b = start(); uint8_t data[512] = {0}; device_status = 1;
        assert(!(write ? b->write(b, 0, data, sizeof(data)) : b->flush(b)));
        assert(b->offline && free_calls == allocated && disks[0].failures == 1); owner_restored();
    }
}
static void test_quarantine_and_fallback(void) {
    fixture(); struct block_device *b = start(); uint8_t data[512];
    fail_stop = true; never_complete = true; clock_step = 1000000;
    assert(!b->read(b, 0, data, sizeof(data)) && b->offline && disks[0].quarantined);
    assert(!free_calls && allocated == 5 && transports[0].pending); owner_restored();
    fixture(); b = start(); service_available = false; timer_ready = false; delay_polls = 64;
    assert(b->read(b, 0, data, sizeof(data)) && !checkpoint_calls); owner_restored();
    fixture(); b = start(); foreign_gate = true;
    assert(!b->read(b, 0, data, sizeof(data)) && !disks[0].commands && !stop_calls);
    foreign_gate = false; owner_restored();
    fixture(); b = start(); never_complete = true; timer_ready = false;
    assert(!b->read(b, 0, data, sizeof(data)) && b->offline && free_calls == allocated); owner_restored();
}
static void test_init_failures(void) {
    for (unsigned mode = 0; mode < 9; ++mode) {
        fixture();
        if (mode == 0) fail_open = true;
        if (mode == 1) fail_setup = true;
        if (mode == 2) fail_start = true;
        if (mode == 3) fail_register = true;
        if (mode == 4) fail_allocation = 1;
        if (mode == 5) fail_allocation = 2;
        if (mode == 6) capacity = 0;
        if (mode == 7) capacity = UINT64_MAX;
        if (mode == 8) { fail_allocation = 2; fail_stop = true; }
        assert(!virtio_block_init());
        assert(disks[0].failed && irqs && !io_depth && cr3 == 0x1000);
        if (mode == 8) assert(allocated == 1 && !free_calls && disks[0].quarantined);
        else assert(free_calls == allocated);
    }
    fixture(); cr3 = 0x2000; assert(!virtio_block_init() && !attempted); cr3 = 0x1000;
    available_pci = 2; assert(virtio_block_init() == 2 && registered == 2 && allocated == 10);
    assert(!strcmp(blocks[0].name, "virtio0") && !strcmp(blocks[1].name, "virtio1"));
    assert(!virtio_block_init());
}
int main(void) {
    test_io(); test_partial(); test_read_only(); test_faults();
    test_quarantine_and_fallback(); test_init_failures();
    puts("virtio-block: batched and partial I/O, flush, RO, bounds, CR3/gates, failures and quarantine passed");
    return 0;
}
