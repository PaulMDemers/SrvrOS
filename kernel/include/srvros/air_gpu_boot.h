#ifndef SRVROS_AIR_GPU_BOOT_H
#define SRVROS_AIR_GPU_BOOT_H
#include <stdint.h>

/* Private EFI configuration table GUID d554348c-09d3-4f11-a672-edd76ec85769.
 * In-memory EFI_GUID as two little-endian words on x86-64. */
#define AIR_BOOT_GUID_LO 0x4f1109d3d554348cULL
#define AIR_BOOT_GUID_HI 0x6957c86ed7ed72a6ULL

/* Same-boot record published before entering Limine. All addresses
 * are physical except for the seven GGTT entries, which are raw PTEs. */
#define AIR_BOOT_SLOT 11744u
#define AIR_BOOT_PAGES 7u
#define AIR_BOOT_FRAMES 19u
#define AIR_BOOT_PROTOCOL_LIMIT 16u
#define AIR_BOOT_BCS_MARKER_VA 0x03eee000u
#define AIR_BOOT_BCS_MARKER_PTE 0x8feef001ULL
enum air_boot_bcs_status {
    AIR_BOOT_BCS_UNAVAILABLE, AIR_BOOT_BCS_IMAGE, AIR_BOOT_BCS_MAPPING,
    AIR_BOOT_BCS_SAMPLE, AIR_BOOT_BCS_OBSERVED
};
struct air_boot_bcs {
    uint64_t pte;
    uint32_t expected, first, second, timeouts;
    uint32_t status, reserved;
};
enum air_boot_protocol_role {
    AIR_BOOT_GOP, AIR_BOOT_GOP_CHILD, AIR_BOOT_PCI_DRIVER, AIR_BOOT_EG2_DRIVER,
    AIR_BOOT_PROTOCOL_ROLES
};
enum air_boot_state { AIR_BOOT_NO_GPU, AIR_BOOT_PROBING, AIR_BOOT_CAPTURED };
struct air_gpu_boot {
    char signature[4];
    uint32_t length;
    uint8_t revision,checksum;
    char oem[6],table_id[8];
    uint32_t oem_revision,creator,creator_revision;
    uint32_t state;
    uint64_t mmio,aperture,framebuffer,framebuffer_bytes,ram;
    uint64_t entries[AIR_BOOT_PAGES];
    uint32_t pci_id,subsystem,plane,surface,first,driver_opens,child_opens;
    uint32_t protocols,firmware_files,step;
    uint8_t driver_file_ids[4][16]; /* Firmware volume file GUIDs of bound EFI drivers. */
    uint8_t protocol_ids[32][16]; /* Raw EFI_GUID representation. */
    /* OEM revision 2. Route CRC32 in bits 63:32 and one-based GOP and
     * EG2 child ordinals in bits 15:8 and 7:0. Zero withholds selection. */
    uint64_t display_branch;
    /* OEM revision 3. Zero counts indicate unavailable inventories. */
    uint8_t protocol_counts[AIR_BOOT_PROTOCOL_ROLES];
    uint8_t handle_protocol_ids[AIR_BOOT_PROTOCOL_ROLES][AIR_BOOT_PROTOCOL_LIMIT][16];
    /* OEM revision 4. Only an observed marker supplies evidence words. */
    struct air_boot_bcs bcs;
};
#define AIR_BOOT_MARKER 0x53525652414d0000ULL
#endif
