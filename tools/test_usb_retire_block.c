/* Actual block/cache/claim + USB retirement; no controller emulation needed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
static uint64_t block_test_irq_save(void) { return 0; }
static void block_test_irq_restore(uint64_t f) { (void)f; }
static uint64_t usb_test_irq_save(void) { return 0; }
static void usb_test_irq_restore(uint64_t f) { (void)f; }
#define SRVROS_BLOCK_HOST_TEST
#define SRVROS_BLOCK_CLAIM_HOST_TEST
#define SRVROS_USB_STORAGE_HOST_TEST
#include "../kernel/src/block.c"
#include "../kernel/src/block_claim.c"
#include "../kernel/src/drivers/usb_storage.c"
static unsigned reads;
bool scheduler_io_begin(void) { return false; }
bool scheduler_io_active(void) { return false; }
bool scheduler_io_yield(void) { return false; }
void scheduler_io_end(void) { abort(); }
void scheduler_io_checkpoint(void) { abort(); }
bool timer_reference_ready(void) { return false; }
uint64_t timer_monotonic_us(void) { return 0; }
void latency_usb(uint64_t bytes,uint8_t opcode) { (void)bytes;(void)opcode; }
void console_printf(const char *format,...) { (void)format; }
static bool cached_read(struct block_device *d,uint64_t o,void *b,uint64_t n) {
    (void)d;(void)o;reads++;memset(b,0x5a,n);return true;
}
int main(void) {
    block_init();
    struct usb_storage storage={.sector_size=512,.sectors=16};
    struct block_device *root=block_register("usb-retired",512,16,cached_read,NULL,&storage);
    struct block_device *healthy=block_register("healthy",512,16,cached_read,NULL,NULL);
    assert(root && healthy);storage.block=root;
    uint8_t buffer[512];assert(block_read(root,0,buffer,sizeof(buffer)) && reads==1);
    assert(block_read(healthy,0,buffer,sizeof(buffer)) && reads==2);
    root->read=read_blocks;root->write=write_blocks;root->flush=flush;
    block_read_fn old_read=root->read;block_write_fn old_write=root->write;block_flush_fn old_flush=root->flush;
    struct block_device alias={.used=true,.block_size=512,.block_count=16,.backing=root,.read=cached_read};
    uint64_t claim=block_claim_mount(root);assert(claim);
    storage.busy=true;assert(!usb_storage_retire(&storage));storage.busy=false;
    assert(usb_storage_retire(&storage) && usb_storage_is_device(root));
    assert(root->used && !root->private_data && root->offline && root->block_count==16);
    memset(buffer,0xcc,sizeof(buffer));
    assert(!block_read(root,0,buffer,sizeof(buffer)) && !block_read(&alias,0,buffer,sizeof(buffer)));
    assert(!old_read(root,0,buffer,sizeof(buffer)) && !old_write(root,0,buffer,sizeof(buffer)) && !old_flush(root));
    for(unsigned i=0;i<sizeof(buffer);i++)assert(buffer[i]==0xcc);
    /* Do not turn uncertain writable storage into an apparent read-only medium. */
    assert(!block_flush_all() && !usb_storage_disable_volume_writes(root));
    assert(block_read(healthy,0,buffer,sizeof(buffer)) && reads==2);
    assert(block_claim_release(claim) && !block_claim_release(claim));
    assert(usb_storage_retire(&storage) && !block_flush_all());
    puts("usb-retire-block: PASS offline cache/aliases, saved callbacks, claims, healthy cache and global flush refusal");
}
