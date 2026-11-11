/* Reuse real process/VFS/exFAT and the existing independent CPU0 turn model.
 * This replay matches captured lengths, not a physical USB command schedule. */
#define reserved_append_case reserved_append_original_full_case
#define reserved_append_note_target_io reserved_append_original_target_io
#include "test_reserved_append_process.c"
#undef reserved_append_case
#undef reserved_append_note_target_io

static uint64_t boundary_delay, boundary_delay_applied;

void reserved_append_note_target_io(bool writing, uint64_t absolute,
    uint64_t offset, uint64_t bytes) {
    reserved_append_original_target_io(writing, absolute, offset, bytes);
    if (boundary_delay && !boundary_delay_applied && reserved_barrier &&
        writing && sync_fixture_content_role == 2) {
        reserved_append_model_delay_us(boundary_delay);
        boundary_delay_applied = boundary_delay;
    }
}

void reserved_append_case(const char *mode, struct block_device *device) {
    enum {
        BYTES = 1366 * 768 * 4 + 54,
        PREFIX = 2555904,
        CAPTURED = 3145728,
        LIVE = 3407872
    };
    assert(!strcmp(mode, "control") || !strcmp(mode, "delay"));
    boundary_delay = !strcmp(mode, "delay") ? 359848 : 0;
    reserved_enabled = true;
    reserved_expected = malloc(BYTES);
    reserved_shadow = malloc(BYTES);
    assert(reserved_expected && reserved_shadow);
    for (unsigned i = 0; i < BYTES; i++)
        reserved_expected[i] = (uint8_t)(i * 37 + 11);

    reserved_writer = (struct process){
        .allocated = true, .active = true, .pid = 11, .image_generation = 17
    };
    reserved_foreign = (struct process){
        .allocated = true, .active = true, .pid = 7, .image_generation = 18
    };
    reserved_writer.fsize_limit.current = UINT64_MAX;
    reserved_foreign.fsize_limit.current = UINT64_MAX;
    for (unsigned i = 0; i < 3; i++) {
        reserved_writer.files[i] = (struct process_file){
            .used = true, .type = PROCESS_FILE_STDIO, .handle = i
        };
    }
    reserved_current = &reserved_writer;
    int64_t opened = process_file_open_write_raw(&reserved_writer,
        "/vol/work/capture.tmp", SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_EXCL);
    assert(opened == 3);
    reserved_fd = (uint64_t)opened;
    reserved_image = write_files[reserved_writer.files[reserved_fd].handle - 1].content;
    assert(process_file_reserve_append_ram(&reserved_writer, reserved_fd, BYTES) == 0);

    while (reserved_bytes < PREFIX) ordinary_chunk();
    assert(reserved_bytes == PREFIX && content_flush(reserved_image) == 0);
    assert(reserved_image->committed_size == PREFIX && !reserved_image->dirty);
    while (reserved_bytes < CAPTURED) ordinary_chunk();
    assert(reserved_bytes == CAPTURED && reserved_chunks == 48);

    assert(sync_timing_enable());
    reserved_current = &reserved_foreign;
    reserved_process_flags = 0x200;
    reserved_barrier = true;
    reserved_foreign_syncs++;
    struct reserved_append_io before, after;
    reserved_append_io_snapshot(&before);
    assert(reserved_sync(device, 1, CAPTURED) == 0);
    assert(reserved_process_flags == 0x200);
    reserved_barrier = false;
    reserved_append_io_snapshot(&after);

    assert(reserved_fast == 4 && reserved_misses == 1 && reserved_queued);
    assert(reserved_image->size == LIVE && reserved_image->committed_size == CAPTURED);
    assert(reserved_image->dirty && !reserved_image->writeback.active);
    struct sync_timing_snapshot snapshot;
    sync_timing_snapshot(&snapshot);
    const struct sync_content_call captured = snapshot.records[slot].worst.content.worst;
    assert(captured.size == CAPTURED && captured.dirty_after == 1);
    assert(captured.prefix.append_calls == 4 && captured.prefix.append_bytes == 262144);
    assert(captured.prefix.live_size == LIVE && captured.prefix.committed_size == CAPTURED);
    assert(reserved_actual_prefix_bytes == PREFIX && boundary_delay_applied == boundary_delay);
    assert(captured.io[2].write_bytes == CAPTURED - PREFIX);
    printf("commit-boundary: mode=%s prefix=%u captured=%u live=%u appended=%llu "
        "fast=%u refused_fifth=%u dirty=%u callbacks=%llu reads=%llu read_bytes=%llu "
        "writes=%llu write_bytes=%llu flushes=%llu content_us=%llu data_us=%llu "
        "prefix_us=%llu model_delay_us=%llu\n", mode, PREFIX, CAPTURED, LIVE,
        (unsigned long long)captured.prefix.append_bytes, reserved_fast,
        reserved_misses, reserved_image->dirty,
        (unsigned long long)(after.callbacks - before.callbacks),
        (unsigned long long)(after.reads - before.reads),
        (unsigned long long)(after.read_bytes - before.read_bytes),
        (unsigned long long)(after.writes - before.writes),
        (unsigned long long)(after.write_bytes - before.write_bytes),
        (unsigned long long)(after.flushes - before.flushes),
        (unsigned long long)(captured.end_us - captured.start_us),
        (unsigned long long)captured.io[2].write_us,
        (unsigned long long)captured.io[3].read_us,
        (unsigned long long)boundary_delay_applied);
    sync_fixture_media("/vol/work/capture.tmp", reserved_expected, CAPTURED);

    reserved_queued = false;
    while (reserved_bytes < BYTES) ordinary_chunk();
    assert(reserved_chunks == 65 && reserved_ordinary == 61 && reserved_fast == 4);
    reserved_current = &reserved_writer;
    assert(process_file_flush(&reserved_writer, reserved_fd) == 0);
    reserved_image = NULL;
    assert(process_file_close(&reserved_writer, reserved_fd) == 0);
    assert(process_file_sync_all() == 0 && device->flush(device));
    fresh_verify("/vol/work/capture.tmp");
    assert(!vfs_lookup("/vol/work/capture.bmp"));
    assert(!vfs_namespace_system_protects("/vol/work/capture.tmp"));
    assert(!vfs_namespace_system_protects("/vol/work/capture.bmp"));
    assert(!process_has_open_vfs_prefix("/vol/work/capture.tmp"));
    assert(!process_has_open_vfs_prefix("/vol/work/capture.bmp"));
    assert(exfat_rename("/vol/work/capture.tmp", "/vol/work/capture.bmp"));
    assert(process_file_sync_all() == 0 && device->flush(device));
    fresh_verify("/vol/work/capture.bmp");
    sync_fixture_media("/vol/work/anchor.txt", (const uint8_t *)"unchanged", 9);
    struct exfat_check_result check = {0};
    assert(exfat_check("/vol", &check));
    assert(!check.errors && !check.leaked_clusters && !check.fat_bitmap_mismatches);
    assert(exfat_unmount("/vol"));
    free(reserved_expected);
    free(reserved_shadow);
}
