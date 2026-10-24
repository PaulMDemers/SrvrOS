#include <srvros/console.h>
#include <srvros/keyboard.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/serial.h>
#include <srvros/timer.h>

#include "io.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define KEYBOARD_BUFFER_SIZE 128
#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL 0x02
#define PS2_STATUS_ABSENT 0xff
#define PS2_DRAIN_LIMIT 256
#define PS2_WAIT_LIMIT 100000

#define PS2_CMD_READ_CONFIG 0x20
#define PS2_CMD_WRITE_CONFIG 0x60
#define PS2_CMD_ENABLE_FIRST 0xae

#define KEYBOARD_CMD_ENABLE_SCANNING 0xf4
#define PS2_ACK 0xfa

static char buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint64_t read_index;
static volatile uint64_t write_index;
static volatile uint64_t pushed_count;
static volatile uint64_t dropped_count;
static volatile uint64_t irq_count;
static volatile uint64_t scancode_count;
static volatile uint8_t last_scancode;
static bool data_enabled;
static bool shift_down;
static uint8_t shift_keys;
static bool caps_lock, caps_key_down;
static bool ctrl_down;
static bool extended_scancode;
static struct scheduler_wait_queue keyboard_wait_queue;
static bool event_mode;
static uint64_t event_generation;
static struct srv_key_event event_queue[128];
static unsigned event_head, event_count;
static uint8_t ps2_modifiers;
static bool ps2_pressed[256];

static const char scancode_set1[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0,
};

static const char scancode_set1_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0,
};

static const char hid_usage[256] = {
    [0x04] = 'a', [0x05] = 'b', [0x06] = 'c', [0x07] = 'd',
    [0x08] = 'e', [0x09] = 'f', [0x0a] = 'g', [0x0b] = 'h',
    [0x0c] = 'i', [0x0d] = 'j', [0x0e] = 'k', [0x0f] = 'l',
    [0x10] = 'm', [0x11] = 'n', [0x12] = 'o', [0x13] = 'p',
    [0x14] = 'q', [0x15] = 'r', [0x16] = 's', [0x17] = 't',
    [0x18] = 'u', [0x19] = 'v', [0x1a] = 'w', [0x1b] = 'x',
    [0x1c] = 'y', [0x1d] = 'z',
    [0x1e] = '1', [0x1f] = '2', [0x20] = '3', [0x21] = '4',
    [0x22] = '5', [0x23] = '6', [0x24] = '7', [0x25] = '8',
    [0x26] = '9', [0x27] = '0', [0x28] = '\n', [0x29] = 27,
    [0x2a] = '\b', [0x2b] = '\t', [0x2c] = ' ', [0x2d] = '-',
    [0x2e] = '=', [0x2f] = '[', [0x30] = ']', [0x31] = '\\',
    [0x33] = ';', [0x34] = '\'', [0x35] = '`', [0x36] = ',',
    [0x37] = '.', [0x38] = '/',
};

static const char hid_usage_shift[256] = {
    [0x04] = 'A', [0x05] = 'B', [0x06] = 'C', [0x07] = 'D',
    [0x08] = 'E', [0x09] = 'F', [0x0a] = 'G', [0x0b] = 'H',
    [0x0c] = 'I', [0x0d] = 'J', [0x0e] = 'K', [0x0f] = 'L',
    [0x10] = 'M', [0x11] = 'N', [0x12] = 'O', [0x13] = 'P',
    [0x14] = 'Q', [0x15] = 'R', [0x16] = 'S', [0x17] = 'T',
    [0x18] = 'U', [0x19] = 'V', [0x1a] = 'W', [0x1b] = 'X',
    [0x1c] = 'Y', [0x1d] = 'Z',
    [0x1e] = '!', [0x1f] = '@', [0x20] = '#', [0x21] = '$',
    [0x22] = '%', [0x23] = '^', [0x24] = '&', [0x25] = '*',
    [0x26] = '(', [0x27] = ')', [0x28] = '\n', [0x29] = 27,
    [0x2a] = '\b', [0x2b] = '\t', [0x2c] = ' ', [0x2d] = '_',
    [0x2e] = '+', [0x2f] = '{', [0x30] = '}', [0x31] = '|',
    [0x33] = ':', [0x34] = '"', [0x35] = '~', [0x36] = '<',
    [0x37] = '>', [0x38] = '?',
};

static uint64_t keyboard_irq_save(void) {
#ifdef SRVROS_KEYBOARD_HOST_TEST
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}
static void keyboard_irq_restore(uint64_t flags) {
#ifdef SRVROS_KEYBOARD_HOST_TEST
    (void)flags;
#else
    if (flags & (1ull << 9)) __asm__ volatile ("sti" : : : "memory");
#endif
}

void keyboard_set_event_mode(bool enabled) {
    uint64_t flags=keyboard_irq_save();
    if (event_mode!=enabled) {
        event_mode=enabled; event_generation++;
        event_head=event_count=0; read_index=write_index=0;
        shift_down=ctrl_down=caps_key_down=extended_scancode=false;
        shift_keys=ps2_modifiers=0;
        for (unsigned i=0;i<256;i++) ps2_pressed[i]=false;
    }
    keyboard_irq_restore(flags);
}
static void push_event(uint8_t usage,uint8_t modifiers,bool caps,bool down,bool repeat) {
    uint64_t flags=keyboard_irq_save();
    struct srv_key_event e={.abi_version=SRV_KEY_EVENT_VERSION,.struct_size=sizeof(e),
        .time_us=timer_monotonic_us(),.usage=usage,
        .modifiers=modifiers|(caps?SRV_KEY_CAPS:0),
        .flags=(down?SRV_KEY_DOWN_EVENT:0)|(repeat?SRV_KEY_REPEAT:0)};
    if (!usage) e.flags=SRV_KEY_CANCEL;
    if (down) {
        unsigned char c=(modifiers&SRV_KEY_SHIFT)?hid_usage_shift[usage]:hid_usage[usage];
        if (caps && usage>=4 && usage<=0x1d) c^=0x20;
        if (c>=32 && c<127) e.codepoint=c;
    }
    if (event_count==128) {
        event_head=event_count=0;
        struct srv_key_event cancel=e;
        cancel.usage=cancel.codepoint=0; cancel.flags=SRV_KEY_CANCEL;
        event_queue[event_count++]=cancel;
    }
    event_queue[(event_head+event_count++)%128]=e;
    scheduler_wake_all(&keyboard_wait_queue);
    process_file_poll_wake();
    keyboard_irq_restore(flags);
}
bool keyboard_events_pending(void) {
    uint64_t flags=keyboard_irq_save();
    bool pending=event_mode && event_count;
    keyboard_irq_restore(flags);return pending;
}
bool keyboard_read_event(struct srv_key_event *out) {
    uint64_t flags=keyboard_irq_save();
    if (!event_mode || !event_count) { keyboard_irq_restore(flags); return false; }
    *out=event_queue[event_head]; event_head=(event_head+1)%128; event_count--;
    keyboard_irq_restore(flags); return true;
}

static void push_char(char c) {
    uint64_t flags = keyboard_irq_save();
    if (event_mode) { keyboard_irq_restore(flags); return; }
    uint64_t next = (write_index + 1) % KEYBOARD_BUFFER_SIZE;
    if (next == read_index) {
        dropped_count++;
        keyboard_irq_restore(flags);
        return;
    }

    buffer[write_index] = c;
    write_index = next;
    pushed_count++;
    scheduler_wake_all(&keyboard_wait_queue);
    process_file_poll_wake();
    keyboard_irq_restore(flags);
}

static void push_escape_sequence(const char *sequence) {
    while (*sequence != '\0') {
        push_char(*sequence++);
    }
}

static bool wait_input_empty(void) {
    for (uint64_t i = 0; i < PS2_WAIT_LIMIT; i++) {
        if ((inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL) == 0) {
            return true;
        }
        __asm__ volatile ("pause");
    }
    return false;
}

static bool wait_output_full(void) {
    for (uint64_t i = 0; i < PS2_WAIT_LIMIT; i++) {
        if ((inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) != 0) {
            return true;
        }
        __asm__ volatile ("pause");
    }
    return false;
}

static bool controller_write(uint8_t command) {
    if (!wait_input_empty()) {
        return false;
    }
    outb(PS2_COMMAND, command);
    return true;
}

static bool data_write(uint8_t value) {
    if (!wait_input_empty()) {
        return false;
    }
    outb(PS2_DATA, value);
    return true;
}

static bool data_read(uint8_t *value) {
    if (value == 0 || !wait_output_full()) {
        return false;
    }
    *value = inb(PS2_DATA);
    return true;
}

bool keyboard_init(void) {
    uint8_t status = inb(PS2_STATUS);
    if (status == PS2_STATUS_ABSENT) {
        console_write("keyboard: ps/2 controller unavailable\n");
        return false;
    }

    uint64_t drained = 0;
    while ((status & PS2_STATUS_OUTPUT_FULL) != 0 && drained < PS2_DRAIN_LIMIT) {
        (void)inb(PS2_DATA);
        drained++;
        status = inb(PS2_STATUS);
        if (status == PS2_STATUS_ABSENT) {
            console_write("keyboard: ps/2 controller unavailable during drain\n");
            return false;
        }
    }

    if ((status & PS2_STATUS_OUTPUT_FULL) != 0) {
        console_write("keyboard: ps/2 drain limit reached, skipping controller\n");
        return false;
    }

    uint8_t config = 0;
    if (!controller_write(PS2_CMD_READ_CONFIG) || !data_read(&config)) {
        console_write("keyboard: ps/2 config read failed\n");
        return true;
    }

    config |= 0x41;
    config &= (uint8_t)~0x10;
    if (!controller_write(PS2_CMD_WRITE_CONFIG) || !data_write(config)) {
        console_write("keyboard: ps/2 config write failed\n");
        return true;
    }

    if (!controller_write(PS2_CMD_ENABLE_FIRST)) {
        console_write("keyboard: ps/2 first port enable failed\n");
        return true;
    }

    uint8_t response = 0;
    if (data_write(KEYBOARD_CMD_ENABLE_SCANNING) && data_read(&response) && response == PS2_ACK) {
        data_enabled = true;
        console_write("keyboard: ps/2 controller drained, irq enabled, scanning enabled\n");
    } else {
        console_printf("keyboard: ps/2 scanning enable no ack response=%x\n", (uint64_t)response);
    }
    return true;
}

static void keyboard_decode_scancode(uint8_t scancode) {
    scancode_count++;
    last_scancode = scancode;
    bool released = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7f;

    if (scancode == 0xe0) {
        extended_scancode = true;
        return;
    }

    if (event_mode) {
        static const uint8_t usages[128]={
            [1]=0x29,[2]=0x1e,[3]=0x1f,[4]=0x20,[5]=0x21,[6]=0x22,[7]=0x23,
            [8]=0x24,[9]=0x25,[10]=0x26,[11]=0x27,[12]=0x2d,[13]=0x2e,[14]=0x2a,[15]=0x2b,
            [16]=0x14,[17]=0x1a,[18]=0x08,[19]=0x15,[20]=0x17,[21]=0x1c,[22]=0x18,
            [23]=0x0c,[24]=0x12,[25]=0x13,[26]=0x2f,[27]=0x30,[28]=0x28,[29]=0xe0,
            [30]=0x04,[31]=0x16,[32]=0x07,[33]=0x09,[34]=0x0a,[35]=0x0b,[36]=0x0d,
            [37]=0x0e,[38]=0x0f,[39]=0x33,[40]=0x34,[41]=0x35,[42]=0xe1,[43]=0x31,
            [44]=0x1d,[45]=0x1b,[46]=0x06,[47]=0x19,[48]=0x05,[49]=0x11,[50]=0x10,
            [51]=0x36,[52]=0x37,[53]=0x38,[54]=0xe5,[56]=0xe2,[57]=0x2c,[58]=0x39,
            [59]=0x3a,[60]=0x3b,[61]=0x3c,[62]=0x3d,[63]=0x3e,[64]=0x3f,[65]=0x40,
            [66]=0x41,[67]=0x42,[68]=0x43,[87]=0x44,[88]=0x45
        };
        uint8_t usage=usages[key];
        if (extended_scancode) {
            switch (key) {
            case 0x1d: usage=0xe4; break; case 0x38: usage=0xe6; break;
            case 0x5b: usage=0xe3; break; case 0x5c: usage=0xe7; break;
            case 0x48: usage=SRV_KEY_UP; break; case 0x50: usage=SRV_KEY_DOWN; break;
            case 0x4b: usage=SRV_KEY_LEFT; break; case 0x4d: usage=SRV_KEY_RIGHT; break;
            case 0x47: usage=SRV_KEY_HOME; break; case 0x4f: usage=SRV_KEY_END; break;
            case 0x53: usage=SRV_KEY_DELETE; break; case 0x52: usage=SRV_KEY_INSERT; break;
            case 0x49: usage=SRV_KEY_PAGEUP; break; case 0x51: usage=SRV_KEY_PAGEDOWN; break;
            case 0x1c: usage=SRV_KEY_ENTER; break; default: usage=0; break;
            }
        }
        extended_scancode=false;
        if (!usage) return;
        bool repeat=ps2_pressed[usage] && !released;
        ps2_pressed[usage]=!released;
        if (usage>=0xe0 && usage<=0xe7) {
            if (released) ps2_modifiers&=(uint8_t)~(1u<<(usage-0xe0));
            else ps2_modifiers|=(uint8_t)(1u<<(usage-0xe0));
        }
        if (usage==0x39 && !released && !repeat) caps_lock=!caps_lock;
        push_event(usage,ps2_modifiers,caps_lock,!released,repeat);
        return;
    }

    if (extended_scancode) {
        extended_scancode = false;
        if (released) {
            return;
        }
        switch (key) {
            case 0x48:
                push_escape_sequence("\x1b[A");
                return;
            case 0x50:
                push_escape_sequence("\x1b[B");
                return;
            case 0x4b:
                push_escape_sequence("\x1b[D");
                return;
            case 0x4d:
                push_escape_sequence("\x1b[C");
                return;
            case 0x47:
                push_escape_sequence("\x1b[H");
                return;
            case 0x4f:
                push_escape_sequence("\x1b[F");
                return;
            case 0x53:
                push_escape_sequence("\x1b[3~");
                return;
        }
        return;
    }

    if (key == 42 || key == 54) {
        uint8_t mask = key == 42 ? 1 : 2;
        if (released) shift_keys &= (uint8_t)~mask; else shift_keys |= mask;
        shift_down = shift_keys != 0;
        return;
    }
    if (key == 29) {
        ctrl_down = !released;
        return;
    }

    if (key == 58) {
        if (!released && !caps_key_down) caps_lock = !caps_lock;
        caps_key_down = !released;
        return;
    }
    if (released) {
        return;
    }
    if (key == 0x0f && shift_down) {
        push_escape_sequence("\x1b[Z");
        return;
    }

    char c = key < sizeof(scancode_set1) ?
        (shift_down ? scancode_set1_shift[key] : scancode_set1[key]) :
        0;
    if (caps_lock && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) c ^= 0x20;
    if (ctrl_down && (key == 46 || key == 32 || key == 44)) {
        c = key == 46 ? 3 : (key == 32 ? 4 : 26);
    }
    if (c == 3 && process_interrupt_foreground(SRV_SIGNAL_INT)) {
        scheduler_wake_all(&keyboard_wait_queue);
        return;
    }
    if (c != 0) {
        push_char(c);
    }
}

void keyboard_handle_irq(void) {
    irq_count++;
    keyboard_decode_scancode(inb(PS2_DATA));
}

void keyboard_wake_waiters(void) {
    scheduler_wake_all(&keyboard_wait_queue);
    process_file_poll_wake();
}

void keyboard_inject_char(char c) {
    if (c != 0) {
        push_char(c);
    }
}

static bool hid_usage_was_down(const uint8_t report[8], uint8_t usage) {
    for (uint64_t i = 2; i < 8; i++) {
        if (report[i] == usage) {
            return true;
        }
    }
    return false;
}

static void inject_escape_sequence(const char *sequence) {
    while (*sequence != '\0') {
        keyboard_inject_char(*sequence++);
    }
}

static bool emit_hid_usage(uint8_t usage, uint8_t modifiers, bool caps, bool repeat) {
    if (event_mode) {
        push_event(usage,modifiers,caps,true,repeat);
        return usage!=0x39 && usage<0xe0;
    }
    bool shift = (modifiers & 0x22) != 0;
    bool ctrl = (modifiers & 0x11) != 0;
    switch (usage) {
        case 0x2b: if (shift) { inject_escape_sequence("\x1b[Z"); return true; } break;
        case 0x4a: inject_escape_sequence("\x1b[H"); return true;
        case 0x4c: inject_escape_sequence("\x1b[3~"); return true;
        case 0x4d: inject_escape_sequence("\x1b[F"); return true;
        case 0x4f: inject_escape_sequence("\x1b[C"); return true;
        case 0x50: inject_escape_sequence("\x1b[D"); return true;
        case 0x51: inject_escape_sequence("\x1b[B"); return true;
        case 0x52: inject_escape_sequence("\x1b[A"); return true;
    }
    char c = shift ? hid_usage_shift[usage] : hid_usage[usage];
    if (usage >= 0x04 && usage <= 0x1d) {
        if (caps) c ^= 0x20;
        if (ctrl) c = (char)(usage - 0x04 + 1);
    }
    if (c == 3 && process_interrupt_foreground(SRV_SIGNAL_INT)) {
        keyboard_wake_waiters();
    } else if (c != 0) {
        keyboard_inject_char(c);
    }
    return c != 0;
}

void keyboard_inject_hid_report(const uint8_t report[8], struct keyboard_hid_state *state) {
    if (report == 0 || state == 0) return;
    /* ErrorRollOver/POSTFail/ErrorUndefined are not key releases. Stop repeat
     * until a valid report establishes the held keys again. */
    for (uint64_t i = 2; i < 8; i++) {
        if (report[i] >= 1 && report[i] <= 3) {
            state->repeat_usage = 0;
            if (event_mode) push_event(0,report[0],state->caps_lock,false,false);
            return;
        }
    }
    if (event_mode) {
        for (unsigned bit=0;bit<8;bit++) if ((report[0]^state->previous_report[0])&(1u<<bit))
            push_event(0xe0+bit,report[0],state->caps_lock,(report[0]&(1u<<bit))!=0,false);
        for (unsigned i=2;i<8;i++) {
            uint8_t usage=state->previous_report[i];
            bool duplicate=false;
            for (unsigned j=2;j<i;j++) duplicate|=state->previous_report[j]==usage;
            if (usage>3 && !duplicate && !hid_usage_was_down(report,usage))
                push_event(usage,report[0],state->caps_lock,false,false);
        }
    }
    for (uint64_t i = 2; i < 8; i++) {
        if (report[i] == 0x39 && !hid_usage_was_down(state->previous_report, 0x39)) {
            state->caps_lock = !state->caps_lock;
            break;
        }
    }
    for (uint64_t i = 2; i < 8; i++) {
        uint8_t usage = report[i];
        if (usage == 0 || (!event_mode && usage == 0x39) || hid_usage_was_down(state->previous_report, usage)) continue;
        bool duplicate = false;
        for (uint64_t j = 2; j < i; j++) duplicate |= report[j] == usage;
        if (duplicate) continue;
        if (emit_hid_usage(usage, report[0], state->caps_lock,false)) {
            state->repeat_usage = usage;
            state->repeat_generation=event_generation;
            state->repeat_due_us = timer_monotonic_us() + 500000;
        }
    }
    if (!hid_usage_was_down(report, state->repeat_usage)) state->repeat_usage = 0;
    for (uint64_t i = 0; i < 8; i++) state->previous_report[i] = report[i];
}

void keyboard_repeat_hid(struct keyboard_hid_state *state, uint64_t now_us) {
    if (state == 0 || state->repeat_usage == 0 || state->repeat_generation!=event_generation || now_us < state->repeat_due_us) return;
    /* Avoid catch-up bursts after the CPU was occupied, and repeated signals. */
    state->repeat_due_us = now_us + 33333;
    if ((state->previous_report[0] & 0x11) == 0)
        (void)emit_hid_usage(state->repeat_usage, state->previous_report[0], state->caps_lock,true);
}

bool keyboard_try_read_char(char *out) {
    if (read_index == write_index) {
        return false;
    }

    *out = buffer[read_index];
    read_index = (read_index + 1) % KEYBOARD_BUFFER_SIZE;
    return true;
}

bool keyboard_scan_char(char *out) {
    return keyboard_try_read_char(out) || serial_try_read_char(out);
}

static bool keyboard_has_char(void *arg) {
    (void)arg;
    return process_should_exit_current() ||
        process_signal_pending_current() ||
        read_index != write_index ||
        serial_has_char();
}

char keyboard_read_char(void) {
    char c;

    while (!keyboard_try_read_char(&c) && !serial_try_read_char(&c)) {
        if (process_should_exit_current()) {
            return 0;
        }
        if (!scheduler_wait(&keyboard_wait_queue, keyboard_has_char, 0)) {
            __asm__ volatile ("pause");
        }
    }

    return c;
}

uint64_t keyboard_buffered_count(void) {
    if (write_index >= read_index) {
        return write_index - read_index;
    }
    return KEYBOARD_BUFFER_SIZE - read_index + write_index;
}

uint64_t keyboard_pushed_count(void) {
    return pushed_count;
}

uint64_t keyboard_dropped_count(void) {
    return dropped_count;
}

uint64_t keyboard_irq_count(void) {
    return irq_count;
}

uint64_t keyboard_scancode_count(void) {
    return scancode_count;
}

uint64_t keyboard_last_scancode(void) {
    return last_scancode;
}

bool keyboard_data_enabled(void) {
    return data_enabled;
}

void keyboard_print_status(void) {
    console_printf("input-ps2: enabled=%u irq=%u scancodes=%u last=%x keybuf=%u pushed=%u dropped=%u\n",
        data_enabled ? 1ull : 0ull,
        irq_count,
        scancode_count,
        (uint64_t)last_scancode,
        keyboard_buffered_count(),
        pushed_count,
        dropped_count);
}
