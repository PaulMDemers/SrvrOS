/* Actual decoder, synthetic HID reports and deterministic repeat clock. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_KEYBOARD_HOST_TEST
#include "../kernel/src/arch/x86_64/keyboard.c"
static uint64_t now;
static bool foreground;
static unsigned signals;
uint64_t timer_monotonic_us(void) { return now; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q; }
bool process_interrupt_foreground(uint64_t signal) { assert(signal == SRV_SIGNAL_INT); signals++; return foreground; }
static void report(struct keyboard_hid_state *state, uint8_t mods, uint8_t key) {
    uint8_t bytes[8] = {mods, 0, key};
    keyboard_inject_hid_report(bytes, state);
}
static void expect(const char *s) {
    char c;
    while (*s) { assert(keyboard_try_read_char(&c)); assert(c == *s++); }
    assert(!keyboard_try_read_char(&c));
}
static void tap(struct keyboard_hid_state *s, uint8_t mods, uint8_t key) {
    report(s, mods, key); report(s, 0, 0);
}
int main(void) {
    struct keyboard_hid_state a = {0}, b = {0};
    tap(&a,0,4); tap(&a,2,4); tap(&a,0x20,0x1d); expect("aAZ");
    report(&a,0,0x39); report(&a,0,0x39); report(&a,0,0);
    tap(&a,0,4); tap(&a,2,4); tap(&a,0,0x1e); tap(&a,2,0x1e); expect("Aa1!");
    tap(&b,0,4); expect("a"); /* Independent keyboards. */
    tap(&a,0,0x39); tap(&a,0,4); expect("a");
    now=100; report(&a,0,4); report(&a,0,4); expect("a");
    keyboard_repeat_hid(&a,500099); expect("");
    keyboard_repeat_hid(&a,500100); expect("a");
    keyboard_repeat_hid(&a,900000); expect("a"); /* No catch-up burst. */
    report(&a,0x22,4); report(&a,0x20,4); /* Release only left Shift. */
    keyboard_repeat_hid(&a,940000); expect("A");
    report(&a,0,0); keyboard_repeat_hid(&a,2000000); expect("");
    uint8_t duplicate[8]={0,0,4,4}; keyboard_inject_hid_report(duplicate,&a); expect("a");
    report(&a,0,1); keyboard_repeat_hid(&a,4000000); expect("");
    report(&a,0,0); tap(&a,0,4); expect("a");
    tap(&a,2,0x2b); tap(&a,0,0x50); tap(&a,0,0x4c); expect("\x1b[Z\x1b[D\x1b[3~");
    tap(&a,1,4); expect("\1");
    foreground=true; report(&a,1,6); expect(""); assert(signals==1);
    keyboard_repeat_hid(&a,5000000); assert(signals==1); expect("");
    report(&a,0,0); foreground=false; tap(&a,0x10,6); expect("\3");
    puts("keyboard-host: PASS (case, Caps/Shift, devices, duplicates, rollover, repeat, editing, Ctrl-C)");
}
