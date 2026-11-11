#include <limine.h>
#include <stdint.h>

__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
    .revision = 0
};

#if defined(SRVROS_INTEL_HDA_OWNER) && !defined(SRVROS_INTEL_HDA_LEASE)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_hda_owner_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#endif

#ifdef SRVROS_INTEL_READER_TRIAL
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_reader_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#elif defined(SRVROS_INTEL_HDA_LEASE)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_hda_lease_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#endif

#ifdef SRVROS_INTEL_HDA_OBSERVER
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_hda_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#endif

#ifdef SRVROS_INTEL_RING_TRANSITION
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_ring_transition_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#elif defined(SRVROS_INTEL_HDA_SHUTDOWN)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_hda_shutdown_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_GPU_HOLD)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_gpu_hold_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_JOINT_ADMIT)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_joint_admit_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#elif defined(SRVROS_INTEL_JOINT)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_joint_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_REHOLD)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_rehold_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_HELD_INTERVAL)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_held_interval_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_PREPARE_RESET)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_held_reset_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_HELD_REQUESTER)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_held_requester_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_REQUESTER_AUDIT)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_requester_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_ENGINE_PREPARE)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_engine_prepare_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_TAKEOVER_PREFLIGHT)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_takeover_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_SHUTDOWN_TRIAL)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_shutdown_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#elif defined(SRVROS_INTEL_BCS_SHUTDOWN_TRIAL)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_bcs_shutdown_firmware_request = { .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
#elif defined(SRVROS_INTEL_BCS_RESET_TRIAL)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_bcs_reset_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#elif defined(SRVROS_INTEL_RESET_READY_TRIAL)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_reset_ready_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#elif defined(SRVROS_INTEL_COMPLETION_TRIAL)
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_completion_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#elif defined(SRVROS_INTEL_WAKE_TRIAL)
/* Separate diagnostic image only; ordinary boot has no new request or writer. */
__attribute__((used, section(".limine_requests")))
volatile struct limine_firmware_type_request intel_wake_firmware_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0
};
#endif

__attribute__((used, section(".limine_requests")))
volatile struct limine_executable_cmdline_request executable_cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

#ifdef SRVROS_AP_PROBE
/* Separate diagnostic ELF only: Limine acts on this before parsing cmdline. */
__attribute__((used, section(".limine_requests")))
volatile struct limine_mp_request ap_probe_request = {
    .id = LIMINE_MP_REQUEST_ID, .revision = 0, .flags = 0
};
#endif
