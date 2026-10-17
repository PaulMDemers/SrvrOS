#include <srvros/apple_trackpad.h>

/* Wire offsets documented by Linux v6.12 drivers/input/mouse/bcm5974.c.
 * Original bounded decoder for SrvrOS; no Linux driver implementation imported.
 * Transport/mode selection and contact tracking are intentionally separate. */
static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1]<<8);
}
static int16_t signed16(const uint8_t *p) {
    uint16_t n=le16(p);
    return n<32768 ? (int16_t)n : (int16_t)((int32_t)n-65536);
}

int apple_trackpad_decode(const uint8_t *report, size_t length,
    struct apple_trackpad_frame *frame) {
    if (!report || !frame) return -1;
    if (length==2) return 0;
    if (length<38 || length>APPLE_TRACKPAD_REPORT_MAX || (length-38)%28) return -1;
    struct apple_trackpad_frame next={0};
    next.button=report[23]!=0;
    for (size_t offset=38;offset<length;offset+=28) {
        const uint8_t *p=report+offset;
        int16_t touch=signed16(p+16);
        if (touch<0) return -1;
        if (!touch) continue; /* Unused slots are not active contacts. */
        struct apple_trackpad_contact *c=&next.contacts[next.count++];
        c->origin=le16(p);
        c->x=signed16(p+2); c->y=signed16(p+4);
        c->tool_major=signed16(p+10); c->tool_minor=signed16(p+12);
        c->touch_major=touch; c->touch_minor=signed16(p+18);
    }
    *frame=next;
    return 1;
}
