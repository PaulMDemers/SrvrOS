/* Actual decoder, synthetic HID reports and deterministic repeat clock. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_KEYBOARD_HOST_TEST
#include "../kernel/src/arch/x86_64/keyboard.c"
static uint64_t now;
static bool foreground;
static unsigned signals,event_wakes;
uint64_t timer_monotonic_us(void) { return now; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q; }
void process_file_poll_wake(void) { event_wakes++; }
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
static struct srv_key_event take(void) {
    struct srv_key_event e; assert(keyboard_read_event(&e));
    assert(e.abi_version==SRV_KEY_EVENT_VERSION && e.struct_size==sizeof(e));
    return e;
}
static void structured(void) {
    struct keyboard_hid_state s={0}; struct srv_key_event e;
    keyboard_set_event_mode(true);
    assert(!keyboard_events_pending());unsigned wake_before=event_wakes;
    foreground=true; unsigned before=signals;
    report(&s,1,6); /* Ctrl-C is an event, never a process signal. */
    assert(keyboard_events_pending() && event_wakes>wake_before);
    e=take(); assert(e.usage==0xe0 && (e.flags&SRV_KEY_DOWN_EVENT));
    e=take(); assert(e.usage==6 && e.codepoint=='c' && (e.modifiers&SRV_KEY_CTRL));
    assert(signals==before); expect("");assert(!keyboard_events_pending());
    report(&s,0,0); e=take(); assert(e.usage==0xe0 && !e.flags);
    e=take(); assert(e.usage==6 && !e.codepoint && !e.flags);
    report(&s,2,SRV_KEY_LEFT); take();
    e=take(); assert(e.usage==SRV_KEY_LEFT && !e.codepoint && (e.modifiers&SRV_KEY_SHIFT));
    now=6000000; keyboard_repeat_hid(&s,now);
    e=take(); assert(e.usage==SRV_KEY_LEFT && (e.flags&SRV_KEY_REPEAT));
    report(&s,0,0); take(); take();
    tap(&s,0,0x39); e=take(); assert(e.modifiers&SRV_KEY_CAPS); take();
    tap(&s,0,4); e=take(); assert(e.codepoint=='A'); take();
    report(&s,0,4); take();
    report(&s,0,1); e=take(); assert(e.flags==SRV_KEY_CANCEL);
    report(&s,0,0); take();
    /* An overflowing queue always exposes cancellation before newer events. */
    for (int i=0;i<65;i++) tap(&s,0,4);
    e=take(); assert(e.flags==SRV_KEY_CANCEL);
    while (keyboard_read_event(&e)) {}
    report(&s,0,4); take();
    keyboard_set_event_mode(false);
    keyboard_repeat_hid(&s,now+1000000); expect(""); /* No held-key replay into console. */
    report(&s,0,0); tap(&s,0,4); expect("A");
    assert(!keyboard_read_event(&e));
    /* PS/2 maps to the same usage/modifier contract. */
    keyboard_set_event_mode(true);
    keyboard_decode_scancode(0x1d); e=take(); assert(e.usage==0xe0);
    keyboard_decode_scancode(0x2e); e=take(); assert(e.usage==6 && e.codepoint=='c');
    assert(e.modifiers&SRV_KEY_CTRL); assert(signals==before);
    keyboard_decode_scancode(0xae); e=take(); assert(e.usage==6 && !e.flags);
    keyboard_decode_scancode(0x9d); take();
    keyboard_decode_scancode(0xe0); keyboard_decode_scancode(0x4b);
    e=take(); assert(e.usage==SRV_KEY_LEFT && e.flags==SRV_KEY_DOWN_EVENT);
    keyboard_decode_scancode(0xe0); keyboard_decode_scancode(0xcb); take();
    keyboard_decode_scancode(0x2a); take(); keyboard_decode_scancode(0x1e);
    e=take(); assert(e.codepoint=='A');
    keyboard_decode_scancode(0x1e); e=take(); assert(e.flags&SRV_KEY_REPEAT);
    keyboard_set_event_mode(false); expect(""); foreground=false;
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
    structured();
    puts("keyboard-host: PASS console and structured HID/PS2 events, modifiers, release/repeat, Ctrl-C isolation, overflow/cancel, mode cleanup");
}
