/* Execute the real console against a guarded, padded RAM framebuffer. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
static bool test_irqs = true, expected_serial_irqs = true;
static uint64_t test_irq_save(void) { uint64_t f=test_irqs ? 512 : 0; test_irqs=false; return f; }
static void test_irq_restore(uint64_t f) { if (f & 512) test_irqs=true; }
#define SRVROS_CONSOLE_TEST_IRQ_SAVE test_irq_save
#define SRVROS_CONSOLE_TEST_IRQ_RESTORE test_irq_restore
#define SRVROS_CONSOLE_HOST_TEST
#include "../kernel/src/console.c"

static char serial_log[65536];
static size_t serial_count;
static workqueue_fn queued;
static bool queue_full;
void bootlog_putc(char c) { (void)c; }
void serial_putc(char c) { assert(test_irqs == expected_serial_irqs); assert(serial_count < sizeof(serial_log)); serial_log[serial_count++] = c; }
bool workqueue_schedule_io_safe(workqueue_fn fn, void *arg) {
    assert(arg == NULL);
    if (queue_full) return false;
    queued = fn;
    return true;
}
static void drain(void) { while (queued) { workqueue_fn fn = queued; queued = NULL; fn(NULL); } }

#define W 83
#define H 71
#define STRIDE 88
static uint32_t storage[STRIDE * H + 2];
static struct limine_framebuffer frame;
static void setup(bool deferred) {
    for (unsigned i = 0; i < sizeof(storage)/sizeof(storage[0]); i++) storage[i] = 0x12345678;
    test_irqs = expected_serial_irqs = true;
    serial_count = 0; queued = NULL; queue_full = false;
    frame = (struct limine_framebuffer){.address=storage+1, .width=W, .height=H, .pitch=STRIDE*4,
        .bpp=32, .memory_model=LIMINE_FRAMEBUFFER_RGB,
        .red_mask_size=8, .red_mask_shift=16, .green_mask_size=8, .green_mask_shift=8, .blue_mask_size=8};
    console_init(&frame);
    assert(columns == 10 && rows == 4);
    if (deferred) console_enable_deferred();
}
static void check_guards(void) {
    assert(storage[0] == 0x12345678 && storage[STRIDE * H + 1] == 0x12345678);
    for (unsigned y = 0; y < H; y++) for (unsigned x = W; x < STRIDE; x++)
        assert(storage[1+y*STRIDE+x] == 0x12345678);
}
static void test_font_and_bytes(void) {
    setup(false);
    for (unsigned c = 33; c <= 126; c++) {
        unsigned ink = 0;
        for (unsigned row = 0; row < 7; row++) ink |= srv_font5x7[c-32][row];
        assert(ink != 0);
    }
    for (char c = 'a'; c <= 'z'; c++) assert(memcmp(srv_font5x7[c-32], srv_font5x7[c-'a'+'A'-32],7));
    console_write("aA{}~");
    assert(serial_count == 5 && memcmp(serial_log,"aA{}~",5) == 0);
    assert(memcmp(cells[0],"aA{}~",5) == 0);
    bool different = false;
    for (unsigned y=0;y<16;y++) for(unsigned x=0;x<8;x++) different |= storage[1+y*STRIDE+x] != storage[1+y*STRIDE+x+8];
    assert(different);
    console_write("\x1b[2J\x1b[2;3HQ\x1b[1DZ\x1b[K");
    assert(cells[1][2] == 'Z' && cells[1][3] == ' ');
    check_guards();
}
static void test_chunking_and_scroll(void) {
    const char *text = "AaZz{}[]!\n0123456789wrap\rEDIT\x1b[K\nline3\nline4\nline5\bX\tq\x1b[1A\x1b[3Gend";
    uint32_t reference[STRIDE * H + 2];
    setup(false);
    for(size_t i=0;i<strlen(text);i++) console_putc(text[i]);
    memcpy(reference,storage,sizeof(storage));
    setup(true);
    console_write(text);
    assert(stats.scrolls > 0 && queued);
    /* Poison visible pixels: redraw must derive from text, never framebuffer. */
    for(unsigned y=0;y<64;y++) for(unsigned x=0;x<80;x++) storage[1+y*STRIDE+x]=0xdeadbeef;
    for(unsigned row=0;row<rows;row++) dirty_range(row,0,columns);
    drain();
    assert(memcmp(reference,storage,sizeof(storage)) == 0);
    check_guards();
    setup(true);
    for(size_t i=0;i<strlen(text);i+=3) console_write_n(text+i,strlen(text+i)<3?strlen(text+i):3);
    drain(); assert(memcmp(reference,storage,sizeof(storage)) == 0);
    setup(true); queue_full=true; console_write("fallback"); assert(!queued); check_guards();
}
static void test_batch_counters(void) {
    setup(true);
    uint64_t start=stats.presents;
    for(unsigned i=0;i<1000;i++) console_write("hello\n");
    assert(stats.presents == start);
    drain();
    assert(stats.presents == start+1 && stats.scrolls == 997);
    assert(stats.cells_painted < 100); /* Initial screen + one coalesced update. */
    assert(memcmp(cells[2],"hello",5)==0);
    setup(false);
    console_printf("%s %u %x %%", "Aa",(uint64_t)42,(uint64_t)0xab);
    const char *expected="Aa 42 0x00000000000000ab %";
    assert(serial_count == strlen(expected) && !memcmp(serial_log,expected,serial_count));
    assert(stats.presents == 2); /* Initial clear plus one formatted batch. */
}
static void test_formats_and_invalid(void) {
    setup(false);
    console_printf("%d %d %d %d %d %u", (int64_t)0, (int64_t)-42000,
        (int64_t)105000, (int64_t)INT64_MIN, (int64_t)INT64_MAX, (uint64_t)7654321);
    const char *signed_expected="0 -42000 105000 -9223372036854775808 9223372036854775807 7654321";
    assert(serial_count==strlen(signed_expected)&&!memcmp(serial_log,signed_expected,serial_count));
    frame.red_mask_shift=0; frame.blue_mask_shift=16;
    console_init(&frame); console_write("a"); check_guards();
    assert(storage[1] == color(0x0b,0x10,0x1a));
    frame.pitch=1; console_init(&frame); assert(!framebuffer_console_ready);
    console_write("still serial");
}
static void test_muted_output(void) {
    setup(true);
    uint32_t before[STRIDE * H + 2];
    console_write("pending");
    memcpy(before, storage, sizeof(before));
    console_set_framebuffer_muted(true);
    drain(); console_write("hidden"); console_clear(); console_flush();
    assert(memcmp(before, storage, sizeof(before)) == 0);
    console_set_framebuffer_muted(false);
    console_write("visible"); drain();
    assert(memcmp(cells[0], "visible", 7) == 0);
    check_guards();
}
static void test_emergency(void) {
    setup(true);
    presenting = true; /* Fatal exception while a presenter is interrupted. */
    console_set_framebuffer_muted(true);
    console_emergency();
    console_clear();
    console_write("Panic aA");
    assert(!presenting && !deferred_output && !framebuffer_console_muted);
    assert(memcmp(cells[0], "Panic aA", 8) == 0);
    check_guards();
}
static void test_serial_irq_state(void) {
    setup(true);
    console_write("Serial waits outside model lock");
    test_irqs = expected_serial_irqs = false;
    console_write("Caller IRQ state preserved");
    assert(!test_irqs);
    test_irqs = expected_serial_irqs = true;
}
int main(void) {
    test_serial_irq_state();
    test_font_and_bytes(); test_chunking_and_scroll(); test_batch_counters(); test_formats_and_invalid(); test_emergency(); test_muted_output();
    puts("console-host-test: PASS (95 glyphs, mixed case, ANSI, chunking, scroll, coalescing, guards, formats)");
}
