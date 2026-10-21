#ifndef SRVROS_WORKSPACE_IMAGE_H
#define SRVROS_WORKSPACE_IMAGE_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/exfat.h>
#define WORKSPACE_IMAGE_BYTES (16u * 1024u * 1024u)
#define WORKSPACE_IMAGE_PATH "/work/srvros-work/battery-fs.img"
/* CPU0 diagnostic caller only. Open admits a prepared exclusive file, parks
 * the outer workspace and mounts /battery-test through a bounded block view.
 * Close unmounts/revokes even after I/O failure; failures inhibit outer remount.
 * No ordinary desktop caller or boot path enables this capability. */
bool workspace_image_open(void);
bool workspace_image_busy(void);
bool workspace_image_ready(void);
bool workspace_image_flush(void);
/* One filesystem operation per call; exclusion is released before returning.
 * Cycle identities must be sequential. A failed operation cannot be resumed.
 * This bounds work units, not wall time of USB/media or battery service waits. */
#define WORKSPACE_IMAGE_STEPS 18u
bool workspace_image_step(uint64_t cycle, bool *complete);
bool workspace_image_pending(void);
bool workspace_image_cycle(uint64_t cycle);
bool workspace_image_close(void);

enum workspace_image_io { WORKSPACE_IMAGE_READ, WORKSPACE_IMAGE_WRITE,
    WORKSPACE_IMAGE_FLUSH, WORKSPACE_IMAGE_IO_COUNT };
struct workspace_image_io_sample { uint64_t count,bytes,total_us,max_us; };
struct workspace_image_profile {
    uint64_t cycle,step,elapsed_us;
    struct workspace_image_io_sample io[EXFAT_IO_PHASE_COUNT][WORKSPACE_IMAGE_IO_COUNT];
};
/* Last successful step only, copied under the diagnostic caller's exclusion.
 * Callback wall time includes admitted services; it is not CPU or input latency. */
bool workspace_image_last_profile(struct workspace_image_profile *out);
#endif
