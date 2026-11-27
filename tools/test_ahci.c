#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define AHCI_POLL_LIMIT 1000
#define AHCI_SERVICE_LIMIT 4
#define SRVROS_AHCI_HOST_TEST
#include "../kernel/src/drivers/ahci.c"
static struct hba_port model;
static struct hba_command_header header;
static struct hba_command_table table;
static uint8_t buffer[4096];
static uint64_t now, current_space;
static unsigned polls, behavior, issued_polls;
static bool services_enabled, in_service, io_active;
static unsigned yields, profile_depth;
static struct ahci_disk *active_disk;
static const char *failure_phase,*failure_reason;
static uint64_t failure_ci,failure_is,failure_elapsed;
static unsigned failure_logs;
static uint64_t probe_reads[68];
static unsigned probe_count;
static uint8_t primary_gpt[34 * AHCI_SECTOR_SIZE];
static uint8_t backup_gpt[33 * AHCI_SECTOR_SIZE], scratch_medium[3 * AHCI_IO_PAGE_BYTES];
static bool short_write, flush_error;
static unsigned model_writes, model_flushes;
static uint64_t model_scratch_lba = AIR61_SCRATCH_LBA;
static uint64_t batched_lba[128];
static unsigned batched_sectors[128], batch_count;
static volatile struct hba_port *reset_port;
static uint32_t *reset_fis;
static unsigned reset_pulses;
static struct hba_port *discovery_port;
enum reset_case { RESET_READY, RESET_NO_STATUS, RESET_FROZEN, RESET_SHORT_PULSE, RESET_STAYS_BUSY };
static enum reset_case reset_case;
void latency_enter(enum latency_site site) { assert(site==LATENCY_AHCI); profile_depth++; }
void latency_leave(enum latency_site site) { assert(site==LATENCY_AHCI && profile_depth); profile_depth--; }
uint64_t block_test_irq_save(void) { return 0; }
void block_test_irq_restore(uint64_t flags) { assert(flags == 0); }
bool scheduler_io_begin(void) {
    if (!services_enabled || in_service) return false;
    assert(!io_active); io_active=true; return true;
}
bool scheduler_io_active(void) { return io_active; }
void scheduler_io_end(void) { assert(io_active && !in_service); io_active=false; }
bool scheduler_io_yield(void) {
    if (!io_active) return false;
    assert(io_active && current_space==2 && profile_depth==0 && active_disk->busy);
    yields++;
    in_service=true;
    uint8_t saved[4096]; memcpy(saved,buffer,sizeof(saved));
    struct block_device dev={.private_data=active_disk};
    uint8_t byte=0xff;
    assert(!ahci_write(&dev,0,&byte,1)); /* Never overwrite an in-flight bounce buffer. */
    assert(!memcmp(saved,buffer,sizeof(saved)));
    struct ahci_disk another=*active_disk; another.busy=false;
    dev.private_data=&another;
    assert(!ahci_read(&dev,0,&byte,1)); /* Cross-device storage rejected in service scope. */
    in_service=false;
    if (behavior==5 || (behavior==10 && yields==6) || (behavior==15 && yields==AHCI_SERVICE_LIMIT) || (behavior==17 && yields==AHCI_SERVICE_LIMIT+3)) model.ci=0;
    if (behavior==6 || (behavior==16 && yields==AHCI_SERVICE_LIMIT) || (behavior==17 && yields==3)) model.tfd=0;
    return true;
}
void console_printf(const char *fmt, ...) {
    if (!strstr(fmt,"failed; disk offline phase=")) return;
    va_list args;va_start(args,fmt);
    (void)va_arg(args,uint64_t);(void)va_arg(args,uint64_t);
    failure_phase=va_arg(args,const char *);failure_reason=va_arg(args,const char *);
    (void)va_arg(args,uint64_t);failure_elapsed=va_arg(args,uint64_t);
    (void)va_arg(args,uint64_t);(void)va_arg(args,uint64_t);
    failure_ci=va_arg(args,uint64_t);failure_is=va_arg(args,uint64_t);
    (void)va_arg(args,uint64_t);(void)va_arg(args,uint64_t);
    va_end(args);failure_logs++;
}
void console_write(const char *s) { (void)s; }
uint64_t vmm_current_address_space(void) { return current_space; }
uint64_t vmm_kernel_address_space(void) { return 1; }
void vmm_switch_address_space(uint64_t s) { current_space=s; }
bool vmm_virt_to_phys(uint64_t v, uint64_t *p) { *p=v; return true; }
bool vmm_map_page(uint64_t v, uint64_t p, uint64_t f) { (void)v;(void)p;(void)f; return true; }
bool timer_reference_ready(void) { return true; }
bool timer_delay_us(uint64_t delay) {
    assert(reset_port && delay == 1000 && reset_port->sctl == 0x301 && !reset_pulses);
    reset_pulses++;
    if (reset_case == RESET_SHORT_PULSE) return false;
    now += delay;
    return true;
}
static void make_air61_identify(uint8_t *identify) {
    const char name[] = "APPLE SSD SD0128F";
    for (unsigned i = 0; i < 40; i++) identify[54 + (i ^ 1)] = i < sizeof(name) - 1 ? name[i] : ' ';
    for (unsigned i = 0; i < 8; i++) identify[200 + i] = AIR61_SSD_SECTORS >> (8 * i);
    identify[167] = 4;
}

uint64_t timer_monotonic_us(void) {
    if (discovery_port) {
        struct ahci_disk *disk = &disks[0];
        now += 10;
        if (discovery_port->cmd & HBA_PORT_CMD_ST) {
            discovery_port->cmd &= ~HBA_PORT_CMD_FR;
            if (discovery_port->ci) {
                assert(disk->command_table->cfis[2] == ATA_CMD_IDENTIFY);
                assert(!(disk->command_list->flags & (1u << 6)));
                make_air61_identify(disk->io_buffer);
                disk->command_list->prdbc = AHCI_SECTOR_SIZE;
                discovery_port->ci = 0;
            }
        } else if (discovery_port->cmd & HBA_PORT_CMD_FRE) {
            discovery_port->cmd |= HBA_PORT_CMD_FR;
            if (reset_pulses && discovery_port->sctl == 0x300) {
                uint32_t *fis = pmm_phys_to_virt(disk->fis_phys);
                fis[0x40 / 4] = 0x00504034;
                discovery_port->tfd = 0x50;
            }
        }
        return now;
    }
    if (reset_port) {
        polls++;
        if (reset_case != RESET_FROZEN) now += 250000;
        if (reset_pulses && reset_port->sctl == 0x300 &&
            (reset_case == RESET_READY || reset_case == RESET_STAYS_BUSY)) {
            *reset_fis = 0x00504034;
            if (reset_case == RESET_READY) reset_port->tfd = 0x50;
        }
        return now;
    }
    if (behavior != 4 && behavior != 15 && behavior != 16 && behavior != 17) now += behavior >= 5 ? 1000 : 100000;
    polls++;
    if(behavior==14 && model.tfd && polls==2) { model.tfd=0;now+=AHCI_COMMAND_TIMEOUT_US; }
    if(model.ci) {
        if (behavior == 22) {
            uint8_t opcode = table.cfis[2];
            unsigned count = table.cfis[12];
            uint64_t lba = (uint64_t)table.cfis[4] | (uint64_t)table.cfis[5] << 8 |
                (uint64_t)table.cfis[6] << 16 | (uint64_t)table.cfis[8] << 24;
            size_t size = count * AHCI_SECTOR_SIZE;
            if (opcode == ATA_CMD_READ_DMA_EXT) {
                assert(count && count <= AIR61_SCRATCH_PAGE_SECTORS && !(header.flags & (1u << 6)));
                if (lba < 34) {
                    assert(lba + count <= 34);
                    memcpy(buffer, primary_gpt + lba * AHCI_SECTOR_SIZE, size);
                } else if (lba >= AIR61_SSD_SECTORS - 33) {
                    assert(lba + count <= AIR61_SSD_SECTORS);
                    memcpy(buffer, backup_gpt + (lba - (AIR61_SSD_SECTORS - 33)) * AHCI_SECTOR_SIZE, size);
                } else {
                    assert(lba >= model_scratch_lba && lba + count <= model_scratch_lba + 24);
                    memcpy(buffer, scratch_medium + (lba - model_scratch_lba) * AHCI_SECTOR_SIZE, size);
                }
            } else if (opcode == ATA_CMD_WRITE_DMA_EXT) {
                assert(count == 1 && (header.flags & (1u << 6)) &&
                       lba == model_scratch_lba + AIR61_SCRATCH_PAGE_SECTORS + model_writes);
                model_writes++;
                if (short_write) size--;
                memcpy(scratch_medium + (lba - model_scratch_lba) * AHCI_SECTOR_SIZE, buffer, size);
            } else {
                assert(opcode == ATA_CMD_FLUSH_CACHE_EXT && !(header.flags & (1u << 6)) &&
                       count == 0 && lba == 0 && model_writes == AIR61_SCRATCH_PAGE_SECTORS);
                model_flushes++;
                if (flush_error) model.is = HBA_PORT_IS_TFES;
            }
            header.prdbc = size;
            model.ci = 0;
        }
        if (behavior == 20 || behavior == 21) {
            unsigned count = table.cfis[12];
            uint64_t lba = (uint64_t)table.cfis[4] | (uint64_t)table.cfis[5] << 8 |
                (uint64_t)table.cfis[6] << 16 | (uint64_t)table.cfis[8] << 24;
            assert(table.cfis[2] == ATA_CMD_READ_DMA_EXT && count >= 1 && count <= 8 &&
                   table.cfis[13] == 0 && !(header.flags & (1u << 6)) && batch_count < 128);
            assert((table.prdt[0].dbc_i & 0x3fffff) == count * AHCI_SECTOR_SIZE - 1);
            batched_lba[batch_count] = lba;
            batched_sectors[batch_count++] = count;
            for (unsigned i = 0; i < count; i++) {
                uint8_t *sector = buffer + i * AHCI_SECTOR_SIZE;
                if (lba + i < 34) memcpy(sector, primary_gpt + (lba + i) * AHCI_SECTOR_SIZE, AHCI_SECTOR_SIZE);
                else memset(sector, lba + i, AHCI_SECTOR_SIZE);
            }
            header.prdbc = count * AHCI_SECTOR_SIZE - (behavior == 21);
            model.ci = 0;
        }
        if (behavior == 18 || behavior == 19) {
            assert(table.cfis[2] == ATA_CMD_READ_DMA_EXT && table.cfis[12] == 1 &&
                   !(header.flags & (1u << 6)) && probe_count < 68);
            probe_reads[probe_count++] = (uint64_t)table.cfis[4] |
                (uint64_t)table.cfis[5] << 8 | (uint64_t)table.cfis[6] << 16 |
                (uint64_t)table.cfis[8] << 24;
            header.prdbc = behavior == 18 ? 512 : 511;
            model.ci = 0;
        }
        if((behavior==11 || behavior==12) && ++issued_polls==2) {
            model.ci=0;now+=AHCI_COMMAND_TIMEOUT_US;
            if(behavior==12) model.is=HBA_PORT_IS_TFES;
        }
        if(behavior==0 || behavior==6 || behavior==8 || behavior==14 || behavior==16) model.ci=0;
        if(behavior==2) model.is=HBA_PORT_IS_TFES;
        if(behavior==3) { model.is=HBA_PORT_IS_TFES; model.ci=0; }
    }
    return now;
}
static struct ahci_disk setup(unsigned mode) {
    memset(&model,0,sizeof(model)); memset(&header,0,sizeof(header)); memset(&table,0,sizeof(table));
    assert(profile_depth==0);
    failure_logs=0;failure_phase=failure_reason=NULL;failure_ci=failure_is=failure_elapsed=0;
    services_enabled=in_service=io_active=false; yields=0; last_service_us=0; commands_issued=0;
    hba_physical=0x1000; now=0; polls=issued_polls=0; behavior=mode; current_space=2;
    return (struct ahci_disk){.port=&model,.command_list=&header,.command_table=&table,
        .io_buffer=buffer,.sectors=8,.flush_command=ATA_CMD_FLUSH_CACHE_EXT};
}
static unsigned dma_calls,dma_fail,dma_frees;
static bool dma_live[5];
static _Alignas(4096) uint8_t dma_pages[5][4096];
uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag) {
    assert(tag==PMM_FRAME_DMA);unsigned n=++dma_calls;
    if(n==dma_fail)return 0;
    assert(n<=4);dma_live[n]=true;return n*4096;
}
void *pmm_phys_to_virt(uint64_t p) { assert(p/4096<=4);return dma_pages[p/4096]; }
void pmm_free_frame(uint64_t p) { assert(p && p/4096<=4 && dma_live[p/4096]);dma_live[p/4096]=false;dma_frees++; }
static void allocation_contract(void) {
    for(unsigned fail=1;fail<=4;fail++) {
        struct ahci_disk d={0};dma_calls=dma_frees=0;dma_fail=fail;memset(dma_live,0,sizeof(dma_live));
        assert(!allocate_port_memory(&d));
        assert(dma_frees==dma_calls-1);
        assert(!d.command_list_phys && !d.fis_phys && !d.command_table_phys && !d.io_buffer_phys);
        for(unsigned i=1;i<=4;i++)assert(!dma_live[i]);
    }
}
static void air61_probe_contract(void) {
    struct pci_device pci = {.bus=4, .vendor_id=0x1b4b, .device_id=0x9183,
        .revision=0x14, .class_code=1, .subclass=6, .prog_if=1};
    assert(air61_controller(&pci));
    pci.revision++;
    assert(!air61_controller(&pci));
    uint8_t identify[512] = {0};
    make_air61_identify(identify);
    assert(air61_identify(identify));
    identify[213] = 0x50;
    identify[235] = 2;
    assert(!air61_identify(identify));
    identify[235] = 1;
    assert(air61_identify(identify));
    identify[54] ^= 1;
    assert(!air61_identify(identify));

    struct ahci_disk disk = setup(18);
    probe_count = 0;
    assert(air61_read_samples(&disk, 0) && probe_count == 68);
    for (unsigned i = 0; i < 34; i++) assert(probe_reads[i] == i);
    for (unsigned i = 0; i < 33; i++) assert(probe_reads[34 + i] == AIR61_SSD_SECTORS - 33 + i);
    assert(probe_reads[67] == 2048);
    assert(!air61_read_sector(&disk, ATA_CMD_WRITE_DMA_EXT, 0));

    disk = setup(19);
    probe_count = 0;
    assert(!air61_read_samples(&disk, 0) && probe_count == 1);
    disk = setup(18);
    probe_count = 0;
    now = AIR61_SSD_PROBE_US;
    assert(!air61_read_samples(&disk, 0) && !probe_count);
}

static void put_le(uint8_t *out, uint64_t value, unsigned size) {
    for (unsigned i = 0; i < size; i++) out[i] = value >> (8 * i);
}

static uint32_t fixture_crc(const uint8_t *bytes, unsigned count) {
    uint32_t crc = UINT32_MAX;
    for (unsigned i = 0; i < count; i++) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static void air61_partition_contract(void) {
    primary_gpt[450] = 0xee;
    put_le(primary_gpt + 454, 1, 4);
    put_le(primary_gpt + 458, AIR61_SSD_SECTORS - 1, 4);
    put_le(primary_gpt + 510, 0xaa55, 2);
    uint8_t *gpt = primary_gpt + 512;
    uint8_t *entries = primary_gpt + 1024;
    memcpy(gpt, "EFI PART", 8);
    put_le(gpt + 8, 0x10000, 4);
    put_le(gpt + 12, 92, 4);
    put_le(gpt + 24, 1, 8);
    put_le(gpt + 32, AIR61_SSD_SECTORS - 1, 8);
    put_le(gpt + 40, 34, 8);
    put_le(gpt + 48, AIR61_SSD_SECTORS - 34, 8);
    gpt[56] = 1;
    put_le(gpt + 72, 2, 8);
    put_le(gpt + 80, 128, 4);
    put_le(gpt + 84, 128, 4);
    memcpy(entries, "\x28\x73\x2a\xc1\x1f\xf8\xd2\x11\xba\x4b\x00\xa0\xc9\x3e\xc9\x3b", 16);
    entries[16] = 1;
    put_le(entries + 32, 2048, 8);
    put_le(entries + 40, 4095, 8);
    put_le(gpt + 88, fixture_crc(entries, 128 * 128), 4);
    put_le(gpt + 16, fixture_crc(gpt, 92), 4);

    block_init();
    struct ahci_disk disk = setup(20);
    disk.probe_partition = true;
    batch_count = 0;
    assert(air61_register_probe(&disk, 0) && batch_count >= 4);
    const uint64_t want[] = {2048, 2080, 2088, 4088};
    for (unsigned i = 0; i < 4; i++) {
        unsigned n = batch_count - 4 + i;
        assert(batched_lba[n] == want[i] && batched_sectors[n] == 8);
    }
    struct block_device *view = block_find("airssdp1");
    assert(view && block_parent(view) == disk.block && !view->write && !disk.block->write);
    uint8_t data[4096];
    assert(block_read(view, 32 * 512, data, sizeof(data)));
    for (unsigned i = 0; i < sizeof(data); i++) assert(data[i] == (uint8_t)(2080 + i / 512));
    unsigned before = batch_count;
    assert(!block_enable_partition_writes(view) && !block_write(view, 0, data, sizeof(data)));
    assert(!block_read(view, 2047 * 512, data, sizeof(data)) && batch_count == before);
    assert(!issue_command(&disk, ATA_CMD_FLUSH_CACHE_EXT, 0, 0, false));
    block_invalidate(disk.block);
    assert(!block_read(view, 0, data, sizeof(data)) && batch_count == before);

    block_init();
    disk = setup(21);
    disk.probe_partition = true;
    batch_count = 0;
    assert(!air61_register_probe(&disk, 0) && batch_count == 1);
    block_init();
    disk = setup(20);
    disk.probe_partition = true;
    now = AIR61_SSD_PROBE_US;
    batch_count = 0;
    assert(!air61_register_probe(&disk, 0) && batch_count == 0);
    puts("air61-ahci-partition: PASS GPT views, four 4 KiB reads, deadline, short DMA and write refusal");
}

static void start_scratch_model(struct ahci_disk *disk) {
    block_init();
    *disk = setup(22);
    disk->probe_partition = true;
    disk->sectors = AIR61_SSD_SECTORS;
    disk->block = block_register("ahci0", AHCI_SECTOR_SIZE, disk->sectors, ahci_read, NULL, disk);
    assert(disk->block && !disk->block->write && !disk->block->flush);
    disk->block->max_transfer_bytes = AHCI_IO_PAGE_BYTES;
    model_writes = model_flushes = 0;
    short_write = flush_error = false;
}

static void air61_scratch_proof(uint8_t proof[64]) {
    uint8_t *gpt = primary_gpt + 512;
    uint8_t *entries = primary_gpt + 1024;
    put_le(gpt + 88, fixture_crc(entries, 128 * 128), 4);
    put_le(gpt + 16, 0, 4);
    put_le(gpt + 16, fixture_crc(gpt, 92), 4);
    memcpy(backup_gpt, entries, 128 * 128);
    uint8_t *backup_header = backup_gpt + 128 * 128;
    memcpy(backup_header, gpt, 512);
    put_le(backup_header + 24, AIR61_SSD_SECTORS - 1, 8);
    put_le(backup_header + 32, 1, 8);
    put_le(backup_header + 72, AIR61_SSD_SECTORS - 33, 8);
    put_le(backup_header + 16, 0, 4);
    put_le(backup_header + 16, fixture_crc(backup_header, 92), 4);
    sha256_digest(primary_gpt, sizeof(primary_gpt), proof);
    sha256_digest(backup_gpt, sizeof(backup_gpt), proof + 32);
}

static void air61_scratch_contract(void) {
    uint8_t *entries = primary_gpt + 1024;
    uint8_t *entry = entries + 128;
    memcpy(entry, "\xf1\x65\x03\x7a\xa3\xb9\xea\x40\x8e\x6a\xd1\xe2\xba\x82\x40\x70", 16);
    entry[16] = 2;
    put_le(entry + 32, AIR61_SCRATCH_LBA, 8);
    put_le(entry + 40, AIR61_SCRATCH_LBA + 23, 8);
    uint8_t proof[64], initial[sizeof(scratch_medium)];
    air61_scratch_proof(proof);
    for (unsigned i = 0; i < 3; i++) air61_scratch_page(proof, i, initial + i * AHCI_IO_PAGE_BYTES);
    memcpy(scratch_medium, initial, sizeof(initial));

    struct ahci_disk disk;
    start_scratch_model(&disk);
    assert(!issue_command(&disk, ATA_CMD_WRITE_DMA_EXT, AIR61_SCRATCH_LBA + 8, 1, true));
    disk.scratch_lba = AIR61_SCRATCH_LBA;
    disk.scratch_armed = true;
    assert(!issue_command(&disk, ATA_CMD_WRITE_DMA_EXT, AIR61_SCRATCH_LBA + 7, 1, true));
    assert(!issue_command(&disk, ATA_CMD_WRITE_DMA_EXT, AIR61_SCRATCH_LBA + 8, 2, true));
    assert(!ahci_flush(disk.block) && !commands_issued);
    disk.scratch_armed = false;
    proof[32] ^= 1;
    assert(!air61_scratch_io(&disk, proof, false, false) && !model_writes && !model_flushes);
    proof[32] ^= 1;

    start_scratch_model(&disk);
    assert(air61_scratch_io(&disk, proof, false, false) && model_writes == 8 && model_flushes == 1);
    assert(!memcmp(scratch_medium, initial, AHCI_IO_PAGE_BYTES));
    assert(!memcmp(scratch_medium + 2 * AHCI_IO_PAGE_BYTES, initial + 2 * AHCI_IO_PAGE_BYTES, AHCI_IO_PAGE_BYTES));
    assert(memcmp(scratch_medium + AHCI_IO_PAGE_BYTES, initial + AHCI_IO_PAGE_BYTES, AHCI_IO_PAGE_BYTES));
    start_scratch_model(&disk);
    assert(air61_scratch_io(&disk, proof, true, false) && !model_writes && !model_flushes);
    assert(!air61_scratch_io(&disk, proof, false, false) && !model_writes && !model_flushes);
    scratch_medium[0] ^= 1;
    assert(!air61_scratch_io(&disk, proof, true, false) && !model_writes && !model_flushes);

    start_scratch_model(&disk);
    memcpy(scratch_medium, initial, sizeof(initial));
    short_write = true;
    assert(!air61_scratch_io(&disk, proof, false, false) && model_writes == 1 && !model_flushes && !disk.scratch_armed);
    start_scratch_model(&disk);
    memcpy(scratch_medium, initial, sizeof(initial));
    flush_error = true;
    assert(!air61_scratch_io(&disk, proof, false, false) && model_writes == 8 && model_flushes == 1 && disk.failed);
    puts("air61-ahci-scratch: PASS grant bounds, fixture refusal, exact write/flush, reboot verify, short DMA and flush failure");
}

static void air61_work_contract(void) {
    uint8_t *efi = primary_gpt + 1024;
    uint8_t *linux_root = efi + 128;
    uint8_t *work = linux_root + 128;
    memcpy(work, linux_root, 128);
    work[16] = 3;
    put_le(efi + 40, 2203647, 8);
    memcpy(linux_root, "\xaf\x3d\xc6\x0f\x83\x84\x72\x47\x8e\x79\x3d\x69\xd8\x47\x7d\xe4", 16);
    put_le(linux_root + 32, 2203648, 8);
    put_le(linux_root + 40, AIR61_WORK_LBA - 1, 8);
    put_le(work + 32, AIR61_WORK_LBA, 8);
    put_le(work + 40, AIR61_WORK_LBA + AIR61_WORK_SECTORS - 1, 8);

    uint8_t proof[64], initial[sizeof(scratch_medium)];
    struct ahci_disk disk;
    model_scratch_lba = AIR61_WORK_GUARDS_LBA;
    put_le(linux_root + 40, AIR61_WORK_LBA - 2, 8);
    air61_scratch_proof(proof);
    start_scratch_model(&disk);
    assert(!air61_scratch_io(&disk, proof, false, true) && !model_writes && !model_flushes);
    put_le(linux_root + 40, AIR61_WORK_LBA - 1, 8);
    air61_scratch_proof(proof);
    for (unsigned i = 0; i < 3; i++) air61_scratch_page(proof, i, initial + i * AHCI_IO_PAGE_BYTES);
    memcpy(scratch_medium, initial, sizeof(initial));
    start_scratch_model(&disk);
    assert(!air61_scratch_io(&disk, proof, false, false) && !model_writes && !model_flushes);

    disk.scratch_lba = AIR61_WORK_GUARDS_LBA;
    disk.scratch_armed = true;
    assert(!issue_command(&disk, ATA_CMD_WRITE_DMA_EXT, AIR61_WORK_LBA - 1, 1, true));
    assert(!issue_command(&disk, ATA_CMD_WRITE_DMA_EXT, AIR61_WORK_GUARDS_LBA, 1, true));
    assert(!issue_command(&disk, ATA_CMD_WRITE_DMA_EXT, AIR61_WORK_GUARDS_LBA + 16, 1, true));
    assert(!ahci_flush(disk.block) && !model_writes && !model_flushes);

    start_scratch_model(&disk);
    assert(air61_scratch_io(&disk, proof, false, true) && model_writes == 8 && model_flushes == 1);
    assert(!memcmp(scratch_medium, initial, AHCI_IO_PAGE_BYTES));
    assert(!memcmp(scratch_medium + 2 * AHCI_IO_PAGE_BYTES, initial + 2 * AHCI_IO_PAGE_BYTES, AHCI_IO_PAGE_BYTES));
    start_scratch_model(&disk);
    assert(air61_scratch_io(&disk, proof, true, true) && !model_writes && !model_flushes);
    puts("air61-ahci-work: PASS exact internal layout, protected ranges, write/flush and read-only reboot verification");
}

static void air61_port_trace_contract(void) {
    static struct hba_memory controller;
    struct ahci_port_trace trace = {0};
    struct ahci_disk disk = {0};
    volatile struct hba_port *port = &controller.ports[0];
    hba = &controller;
    controller.ghc = HBA_GHC_AHCI_ENABLE;
    port->cmd = HBA_PORT_CMD_ST | HBA_PORT_CMD_FRE;
    port->tfd = 0x80;
    port->fb = 0xdeadb000;
    dma_calls = dma_frees = dma_fail = 0;
    trace_port(&trace, AHCI_TRACE_FIRMWARE, port, 0, NULL);
    trace_port(&trace, AHCI_TRACE_BEFORE_STOP, port, 0, NULL);
    assert(trace.states[AHCI_TRACE_FIRMWARE].fb == 0xdeadb000);
    assert(!trace.states[AHCI_TRACE_FIRMWARE].fis_valid);
    assert(prepare_port(&disk, 0, port, &trace));
    enable_fis(&disk, &trace);
    start_port(&disk, &trace);
    assert(dma_calls == 4 && !dma_frees);
    assert(trace.states[AHCI_TRACE_BEFORE_STOP].cmd == (HBA_PORT_CMD_ST | HBA_PORT_CMD_FRE));
    assert(trace.states[AHCI_TRACE_AFTER_STOP].cmd == 0);
    assert(trace.states[AHCI_TRACE_BUFFERS].fb == disk.fis_phys);
    assert(trace.states[AHCI_TRACE_BUFFERS].fis_valid && !trace.states[AHCI_TRACE_BUFFERS].d2h);
    assert(trace.states[AHCI_TRACE_FIS_ENABLED].cmd == HBA_PORT_CMD_FRE);
    assert(trace.states[AHCI_TRACE_ENGINE_STARTED].cmd == (HBA_PORT_CMD_ST | HBA_PORT_CMD_FRE));
    uint32_t *fis = pmm_phys_to_virt(disk.fis_phys);
    fis[0x20 / 4] = 0x1122335f;
    fis[0x40 / 4] = 0x44556634;
    fis[0x58 / 4] = 0x778899a1;
    trace_port(&trace, AHCI_TRACE_IDENTIFY_RETURN, port, 0, &disk);
    const struct ahci_port_state *last = &trace.states[AHCI_TRACE_IDENTIFY_RETURN];
    assert(last->fis_valid && last->pio == 0x1122335f && last->d2h == 0x44556634 && last->sdb == 0x778899a1);
    assert(last->tfd == 0x80 && !last->ci && port->tfd == 0x80);
    port->fb = 0xdeadb000;
    trace_port(&trace, AHCI_TRACE_RETIRED, port, 0, &disk);
    assert(!trace.states[AHCI_TRACE_RETIRED].fis_valid && !trace.states[AHCI_TRACE_RETIRED].d2h);
    hba = NULL;
    puts("air61-ahci-port: PASS stop/FRE/ST snapshots, owned status FIS, foreign buffer refusal");
}

static void air61_reset_contract(void) {
    static struct hba_memory controller;
    hba = &controller;
    reset_port = &controller.ports[0];
    struct ahci_disk disk = {.port = reset_port, .fis_phys = 4096};
    reset_fis = (uint32_t *)pmm_phys_to_virt(disk.fis_phys) + 0x40 / 4;
    reset_port->cmd = HBA_PORT_CMD_FRE | HBA_PORT_CMD_FR;
    reset_port->ssts = 0x133;
    reset_port->sig = SATA_SIG_ATA;
    reset_port->fb = disk.fis_phys;
    reset_port->sctl = 0x300;
    for (reset_case = RESET_READY; reset_case <= RESET_STAYS_BUSY; reset_case++) {
        struct ahci_port_trace trace = {0};
        *reset_fis = 0;
        reset_port->tfd = 0x80;
        polls = reset_pulses = 0;
        now = 0;
        enum ahci_probe_result status = air61_link_reset(&disk, &trace);
        assert(status == (reset_case == RESET_READY ? AHCI_PROBE_PASS : AHCI_PROBE_FAILED));
        assert(reset_pulses == 1 && trace.reset_attempts == 1 && reset_port->sctl == 0x300);
        assert(trace.states[AHCI_TRACE_RESET_ASSERTED].sctl == 0x301);
        assert(trace.states[AHCI_TRACE_RESET_RELEASED].sctl == 0x300);
        assert(!(reset_port->cmd & HBA_PORT_CMD_ST) && !reset_port->ci && !reset_port->sact);
        if (reset_case == RESET_READY) assert(!strcmp(trace.reset_result, "ready") && trace.reset_wait_us < 2000000);
        else if (reset_case == RESET_SHORT_PULSE) assert(!strcmp(trace.reset_result, "pulse-unproved"));
        else {
            assert(!strcmp(trace.reset_result, "timeout"));
            if (reset_case == RESET_FROZEN) assert(!trace.reset_wait_us && polls <= AHCI_POLL_LIMIT + 10);
            else assert(trace.reset_wait_us == AHCI_COMMAND_TIMEOUT_US);
        }
    }
    struct ahci_port_trace trace = {0};
    reset_pulses = 0;
    *reset_fis = 0;
    reset_port->ci = 1;
    assert(air61_link_reset(&disk, &trace) == AHCI_PROBE_FAILED && !trace.reset_attempts);
    reset_port->ci = 0;
    disk.port_index = 1;
    assert(air61_link_reset(&disk, &trace) == AHCI_PROBE_FAILED && !trace.reset_attempts);
    disk.port_index = 0;
    *reset_fis = 0x00504034;
    assert(air61_link_reset(&disk, &trace) == AHCI_PROBE_FAILED && !trace.reset_attempts);
    assert(reset_port->sctl == 0x300 && !reset_pulses);
    reset_port = NULL;
    hba = NULL;
    puts("air61-ahci-reset: PASS one pulse, control restore, fresh FIS, BSY and clock bounds, foreign and active refusal");
}

static void air61_normal_init_contract(void) {
    static struct hba_memory controller;
    for (unsigned read_only = 0; read_only <= 1; read_only++) {
        struct ahci_port_trace trace = {0};
        (void)setup(0);
        block_init();
        memset(disks, 0, sizeof(disks));
        disk_count = dma_calls = reset_pulses = 0;
        hba = &controller;
        discovery_port = &controller.ports[0];
        *discovery_port = (struct hba_port){.ssts=0x133, .sig=SATA_SIG_ATA, .sctl=0x300,
            .tfd=read_only ? 0x80 : 0x50};
        reset_port = discovery_port;
        reset_case = RESET_READY;
        if (read_only) {
            discovery_port->ci = 1;
            assert(init_port(0, discovery_port, &trace) == AHCI_PROBE_UNSAFE);
            assert(!dma_calls && !reset_pulses && !disk_count);
            discovery_port->ci = 0;
            memset(&trace, 0, sizeof(trace));
        }
        assert(init_port(0, discovery_port, read_only ? &trace : NULL) == AHCI_PROBE_PASS);
        struct block_device *view = block_find("ahci0");
        assert(disk_count == 1 && view == disks[0].block && view->read == ahci_read && reset_pulses == read_only);
        if (read_only) {
            assert(!view->write && !view->flush && view->max_transfer_bytes == AHCI_IO_PAGE_BYTES);
            assert(trace.states[AHCI_TRACE_IDENTIFY_RETURN].valid && !trace.states[AHCI_TRACE_IDENTIFY_RETURN].ci);
            assert(air61_retire(&disks[0], &trace, AHCI_PROBE_PASS) == AHCI_PROBE_PASS);
            assert(view->offline && !trace.states[AHCI_TRACE_RETIRED].cmd);
        } else {
            assert(view->write == ahci_write && view->flush == ahci_flush && !view->max_transfer_bytes);
        }
        reset_port = discovery_port = NULL;
        hba = NULL;
        disk_count = 0;
    }
    puts("air61-ahci-discovery: PASS ordinary and read-only registration, one reset, ownership and retirement");
}
int main(void) {
    allocation_contract();
    air61_probe_contract();
    air61_partition_contract();
    air61_scratch_contract();
    air61_work_contract();
    air61_port_trace_contract();
    air61_reset_contract();
    air61_normal_init_contract();
    struct ahci_disk disk=setup(0);
    assert(issue_command(&disk,ATA_CMD_IDENTIFY,0,0,false));
    assert(header.prdt_length==1 && (table.prdt[0].dbc_i&0x3fffff)==511 && current_space==2);
    assert(issue_command(&disk,ATA_CMD_WRITE_DMA_EXT,0x123456789aull,1,true));
    assert(header.flags&(1<<6)); assert(table.cfis[4]==0x9a && table.cfis[9]==0x12);
    struct block_device dev={.private_data=&disk}; assert(ahci_flush(&dev));
    assert(header.prdt_length==0 && table.cfis[2]==ATA_CMD_FLUSH_CACHE_EXT);
    for(unsigned mode=1;mode<=3;mode++) {
        disk=setup(mode); assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
        assert(disk.failed && current_space==2 && polls<30);
        assert(failure_logs==1 && !strcmp(failure_phase,"completion"));
        assert(!strcmp(failure_reason,mode==1?"deadline":"task-file"));
        if(mode==1)assert(failure_elapsed>=AHCI_COMMAND_TIMEOUT_US && failure_ci==1);
        else assert(failure_is&HBA_PORT_IS_TFES);
        unsigned old=polls; assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false)); assert(old==polls);
    }
    disk=setup(0); model.tfd=0x80; assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(disk.failed && model.ci==0 && polls<30);
    assert(!strcmp(failure_phase,"ready") && !strcmp(failure_reason,"deadline"));
    struct block_device published={.used=true};
    disk=setup(4);disk.block=&published;
    assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false) && published.offline && published.used);
    disk=setup(4); assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(disk.failed && polls<=1002 && !strcmp(failure_reason,"poll-limit"));
    disk=setup(5); services_enabled=true; active_disk=&disk;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields>=1 && !disk.busy && !io_active && current_space==2 && profile_depth==0);
    disk=setup(6); services_enabled=true; active_disk=&disk; model.tfd=0x80;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields>=1 && !disk.busy && current_space==2);
    disk=setup(7); services_enabled=true; active_disk=&disk;
    assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields>0 && disk.failed && !disk.busy && !io_active && profile_depth==0 && current_space==2);
    disk=setup(5); services_enabled=true; active_disk=&disk;
    memset(buffer,0x5a,sizeof(buffer));
    dev.private_data=&disk; uint8_t value=0x31;
    assert(ahci_write(&dev,10,&value,1));
    assert(buffer[9]==0x5a && buffer[10]==value && buffer[11]==0x5a && yields>=2 && !disk.busy);
    disk=setup(4); services_enabled=true; active_disk=&disk;
    assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields==AHCI_SERVICE_LIMIT && polls<AHCI_POLL_LIMIT && disk.failed && !disk.busy && !io_active);
    assert(!strcmp(failure_reason,"service-limit"));
    disk=setup(10); services_enabled=true; active_disk=&disk;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields>AHCI_SERVICE_LIMIT && !disk.failed); /* Healthy time keeps its full deadline. */
    disk=setup(8); services_enabled=true; active_disk=&disk;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields==1 && current_space==2); /* Fast completion still services a long sequence. */
    disk=setup(11);
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(!disk.failed && issued_polls==2 && !model.ci && current_space==2);
    disk=setup(12);assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(disk.failed && !model.ci && (model.is&HBA_PORT_IS_TFES));
    disk=setup(14);model.tfd=0x80;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false) && !disk.failed);
    disk=setup(15);services_enabled=true;active_disk=&disk;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields==AHCI_SERVICE_LIMIT && !disk.failed);
    disk=setup(16);services_enabled=true;active_disk=&disk;model.tfd=0x80;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields==AHCI_SERVICE_LIMIT && !disk.failed);
    disk=setup(17);services_enabled=true;active_disk=&disk;model.tfd=0x80;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields==AHCI_SERVICE_LIMIT+3 && !disk.failed && failure_logs==0);
    puts("ahci-host: PASS completion/readiness at deadline/service bounds, phase budgets, error precedence and diagnostics");
    uint8_t identify[512]={0}; identify[167]=0x60; assert(identify_flush_command(identify)==ATA_CMD_FLUSH_CACHE_EXT);
    identify[167]=0x50; assert(identify_flush_command(identify)==ATA_CMD_FLUSH_CACHE);
    identify[167]=0x10; assert(identify_flush_command(identify)==0);
    puts("ahci-host: PASS (DMA/flush descriptors, stuck busy/CI, errors, quarantine, address-space restore, ownership, service windows)");
}
