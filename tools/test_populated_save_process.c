/* Real complete-save fixture. Roles wrap the existing producer entry points.
 * Independent CPU0 turns and advancing realtime are deterministic host models.
 * Extra per-barrier media checks are measured separately from save operations. */
#include <srvros/exfat.h>
#include <srvros/vfs.h>
static bool profile_check_read(const struct vfs_node *, const uint8_t **, uint64_t *);
static bool profile_allocation_check(const char *, struct exfat_check_result *);
#define vfs_read_all profile_check_read
#define exfat_check profile_allocation_check
#define reserved_append_case profile_original_case
#define reserved_append_backend_step profile_original_backend_step
#define reserved_append_read_observed profile_original_read_observed
#define reserved_append_note_target_io profile_original_target_io
#define sync_timing_content_role_begin profile_original_role_begin
#define sync_timing_content_role_end profile_original_role_end
#include "test_reserved_append_process.c"
#undef vfs_read_all
#undef exfat_check
#undef reserved_append_case
#undef reserved_append_backend_step
#undef reserved_append_read_observed
#undef reserved_append_note_target_io
#undef sync_timing_content_role_begin
#undef sync_timing_content_role_end

static bool profiling, validating;
static unsigned profile_role = 6, profile_depth, profile_round;
static struct { unsigned previous, original; } profile_stack[32];
/* Phase0 includes the complete save. Phase1 is its foreign-barrier subset. */
static struct reserved_append_io profile_io[2][7], profile_validation;
void profile_set_realtime(uint64_t);

static bool profile_check_read(const struct vfs_node *node,
    const uint8_t **data, uint64_t *size) {
    bool previous = validating;
    validating = true;
    bool ok = vfs_read_all(node, data, size);
    validating = previous;
    return ok;
}

static bool profile_allocation_check(const char *path, struct exfat_check_result *result) {
    /* The following allocation traversal/unmount validates the disposable
     * backend; these operations are outside screenshot saving. */
    profiling = false;
    return exfat_check(path, result);
}

unsigned sync_timing_content_role_begin(unsigned role) {
    assert(role < 7 && profile_depth < 32);
    unsigned index = profile_depth++;
    profile_stack[index].previous = profile_role;
    profile_stack[index].original = profile_original_role_begin(role);
    profile_role = role;
    return index;
}

void sync_timing_content_role_end(unsigned index) {
    assert(profile_depth == index + 1);
    profile_original_role_end(profile_stack[index].original);
    profile_role = profile_stack[index].previous;
    profile_depth--;
}

void reserved_append_backend_step(void) {
    if (reserved_barrier && profile_round != reserved_foreign_syncs) {
        profile_round = reserved_foreign_syncs;
        profile_set_realtime(1792326896000000ull + (uint64_t)profile_round * 1000000);
    }
    profile_original_backend_step();
}

static void profile_record(unsigned kind, uint64_t bytes) {
    if (!profiling) return;
    for (unsigned phase = 0; phase < 2; phase++) {
        if (phase && !reserved_barrier) continue;
        if (validating && phase) continue;
        struct reserved_append_io *row = validating ? &profile_validation : &profile_io[phase][profile_role];
        row->callbacks++;
        if (kind == 0) { row->reads++; row->read_bytes += bytes; }
        else if (kind == 1) { row->writes++; row->write_bytes += bytes; }
        else row->flushes++;
    }
}

void reserved_append_read_observed(uint64_t bytes, bool ok) {
    profile_original_read_observed(bytes, ok);
    profile_record(0, bytes);
}

void reserved_append_note_target_io(bool writing, uint64_t absolute,
    uint64_t offset, uint64_t bytes) {
    profile_original_target_io(writing, absolute, offset, bytes);
    profile_record(writing ? 1 : 2, bytes);
}

void reserved_append_case(const char *mode, struct block_device *device) {
    profiling = true;
    profile_original_case(mode, device);
    assert(!profile_depth);
    for (unsigned phase = 0; phase < 2; phase++) {
        for (unsigned role = 0; role < 7; role++) {
            const struct reserved_append_io *row = &profile_io[phase][role];
            printf("save-profile: phase=%u role=%u callbacks=%llu reads=%llu read_bytes=%llu "
                "writes=%llu write_bytes=%llu flushes=%llu\n", phase, role,
                (unsigned long long)row->callbacks, (unsigned long long)row->reads,
                (unsigned long long)row->read_bytes, (unsigned long long)row->writes,
                (unsigned long long)row->write_bytes, (unsigned long long)row->flushes);
        }
    }
    printf("save-validation: callbacks=%llu read_bytes=%llu\n",
        (unsigned long long)profile_validation.callbacks,
        (unsigned long long)profile_validation.read_bytes);
}
