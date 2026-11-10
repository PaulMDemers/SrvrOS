/* Actual scheduler and sync producer composed in separate translation units.
 * Only USB/controller and IRQ backing are host seams, not observer outcomes. */
#include <assert.h>
#include <stdint.h>
uint64_t capture_gate_sync_fixture_irq(void);
static uint64_t sync_test_irq_save(void) {assert(!(capture_gate_sync_fixture_irq()&0x200));return 0;}
static void sync_test_irq_restore(uint64_t f) {assert(!f && !(capture_gate_sync_fixture_irq()&0x200));}
#define SRVROS_SYNC_TIMING_HOST_TEST
#include "../kernel/src/sync_timing.c"
bool usb_storage_timing_aux_begin(void) {return true;}
void usb_storage_timing_aux_end(struct usb_storage_timing *out) {*out=(struct usb_storage_timing){.valid=1};}
bool usb_storage_content_begin(void) {return true;}
void usb_storage_content_end(struct usb_storage_content_timing *out) {*out=(struct usb_storage_content_timing){.valid=1};}
unsigned usb_storage_content_role_begin(unsigned role) {(void)role;assert(0);return ~0u;}
void usb_storage_content_role_end(unsigned token) {(void)token;assert(0);}
void capture_gate_sync_fixture_begin(void) {
    assert(sync_timing_enable() && sync_timing_begin(77));
    assert(sync_timing_content_begin(2,444,42,65536));
    sync_timing_content_io(2,1,65536,50,true);
}
void capture_gate_sync_fixture_end(void) {
    sync_timing_content_end(true,false);sync_timing_files_done(true);sync_timing_end(true);
    assert(!observed.active && !observed.invalid && !observed.records[0].invalid);
    assert(observed.records[0].worst.content.worst.identity==444);
}
