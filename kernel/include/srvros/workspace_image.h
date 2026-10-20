#ifndef SRVROS_WORKSPACE_IMAGE_H
#define SRVROS_WORKSPACE_IMAGE_H
#include <stdbool.h>
#include <stdint.h>
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
bool workspace_image_cycle(uint64_t cycle);
bool workspace_image_close(void);
#endif
