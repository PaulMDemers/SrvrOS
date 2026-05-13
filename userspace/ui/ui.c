#include <srvros/conio.h>
#include <srvros/gfx.h>
#include <srvros/mouse.h>
#include <srvros/sys.h>

#include <stdint.h>

static void print_u64(uint64_t value) {
    char digits[21];
    uint64_t count = 0;

    if (value == 0) {
        cputs("0");
        return;
    }

    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (count > 0) {
        char c = digits[--count];
        srv_write(SRV_STDOUT, &c, 1);
    }
}

static void delay(void) {
    srv_yield();
}

static void draw_bars(uint64_t width) {
    static const uint32_t colors[] = {
        0x1f6feb,
        0x2ea043,
        0xd29922,
        0xf85149,
        0xa371f7,
    };
    uint64_t bar_width = width / (sizeof(colors) / sizeof(colors[0]));
    if (bar_width == 0) {
        bar_width = 1;
    }

    for (uint64_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        fillrect(i * bar_width, 96, bar_width, 24, colors[i]);
    }
}

static void draw_cursor(uint64_t x, uint64_t y, uint32_t color) {
    fillrect(x, y, 18, 3, color);
    fillrect(x, y, 3, 18, color);
    fillrect(x + 3, y + 3, 3, 3, color);
    fillrect(x + 6, y + 6, 3, 3, color);
}

static uint64_t clamp_add(uint64_t value, int32_t delta, uint64_t min, uint64_t max) {
    int64_t next = (int64_t)value + delta;
    if (next < (int64_t)min) {
        return min;
    }
    if (next > (int64_t)max) {
        return max;
    }
    return (uint64_t)next;
}

int main(void) {
    struct conio_info term = { 0, 0 };
    struct gfx_info gfx = { 0, 0, 0 };
    struct mouse_event mouse = { 0, 0, 0, 0, 0 };
    uint64_t x = 80;
    uint64_t y = 150;
    uint64_t mouse_x = 220;
    uint64_t mouse_y = 220;
    uint8_t mouse_buttons = 0;

    clrscr();
    conio_info(&term);
    gfx_info(&gfx);

    gotoxy(2, 1);
    cputs("srvros ui demo");
    gotoxy(2, 3);
    cputs("console cells: ");
    print_u64(term.columns);
    cputs(" x ");
    print_u64(term.rows);
    gotoxy(2, 4);
    cputs("framebuffer: ");
    print_u64(gfx.width);
    cputs(" x ");
    print_u64(gfx.height);
    cputs(" pitch ");
    print_u64(gfx.pitch);
    gotoxy(2, 6);
    cputs("Mouse moves crosshair. WASD moves square. Q exits.");

    draw_bars(gfx.width);
    fillrect(x, y, 48, 48, 0x7dd3fc);
    draw_cursor(mouse_x, mouse_y, 0xffffff);

    for (;;) {
        int key = kbhit();
        if (key == 'q' || key == 'Q' || key == 27) {
            break;
        }

        uint64_t old_x = x;
        uint64_t old_y = y;
        if ((key == 'a' || key == 'A') && x >= 8) {
            x -= 8;
        } else if ((key == 'd' || key == 'D') && x + 56 < gfx.width) {
            x += 8;
        } else if ((key == 'w' || key == 'W') && y >= 128) {
            y -= 8;
        } else if ((key == 's' || key == 'S') && y + 56 < gfx.height) {
            y += 8;
        }

        if (x != old_x || y != old_y) {
            fillrect(old_x, old_y, 48, 48, 0x0b101a);
            fillrect(x, y, 48, 48, 0x7dd3fc);
            gotoxy(2, 8);
            cputs("square: ");
            print_u64(x);
            cputs(", ");
            print_u64(y);
            cputs("        ");
        }

        if (mouse_scan(&mouse) > 0) {
            draw_cursor(mouse_x, mouse_y, 0x0b101a);
            if (gfx.width > 20) {
                mouse_x = clamp_add(mouse_x, mouse.dx, 0, gfx.width - 20);
            }
            if (gfx.height > 20) {
                mouse_y = clamp_add(mouse_y, mouse.dy, 0, gfx.height - 20);
            }
            mouse_buttons = mouse.buttons;
            draw_cursor(mouse_x, mouse_y, mouse_buttons != 0 ? 0xf85149 : 0xffffff);
            gotoxy(2, 9);
            cputs("mouse: ");
            print_u64(mouse_x);
            cputs(", ");
            print_u64(mouse_y);
            cputs(" buttons ");
            print_u64(mouse_buttons);
            cputs("        ");
        }

        delay();
    }

    gotoxy(2, 10);
    cputs("ui demo exited\n");
    return 0;
}
