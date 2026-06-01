#include <srvros/gui.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define SURFACE_W 240
#define SURFACE_H 140

static uint32_t pixels[SURFACE_W * SURFACE_H];

static void copy_text(char *to, const char *from) {
    uint64_t i = 0;
    while (from[i] != '\0' && i + 1 < GUI_TEXT_MAX) {
        to[i] = from[i];
        i++;
    }
    to[i] = '\0';
}

static void send_msg(uint64_t type, uint64_t surface_id, int64_t x, int64_t y,
    uint64_t width, uint64_t height, const char *text) {
    struct gui_message msg = {
        .type = type,
        .window_id = WIN,
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .value = (int64_t)surface_id,
    };
    copy_text(msg.text, text);
    gui_send(&msg);
}

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void draw_demo(uint64_t tick) {
    for (uint64_t y = 0; y < SURFACE_H; y++) {
        for (uint64_t x = 0; x < SURFACE_W; x++) {
            uint8_t r = (uint8_t)((x + tick * 3) & 0xff);
            uint8_t g = (uint8_t)((y * 2 + tick * 5) & 0xff);
            uint8_t b = (uint8_t)(((x ^ y) + tick * 2) & 0xff);
            pixels[y * SURFACE_W + x] = rgb(r, g, b);
        }
    }
    for (uint64_t y = 22; y < 72; y++) {
        for (uint64_t x = 32; x < 112; x++) {
            pixels[y * SURFACE_W + x] = rgb(0x11, 0x1b, 0x24);
        }
    }
    for (uint64_t y = 28; y < 66; y++) {
        for (uint64_t x = 126; x < 210; x++) {
            pixels[y * SURFACE_W + x] = rgb(0xf5, 0xb8, 0x4b);
        }
    }
}

int main(void) {
    uint64_t surface_id = 0;
    uint64_t start;
    srv_puts("surfacedemo: start\n");
    if (gui_surface_create(SURFACE_W, SURFACE_H, 0, &surface_id) != 0 || surface_id == 0) {
        srv_puts("surfacedemo: surface create failed\n");
        return 1;
    }

    draw_demo(0);
    if (gui_surface_blit(surface_id, 0, 0, SURFACE_W, SURFACE_H, pixels, SURFACE_W) != 0) {
        srv_puts("surfacedemo: surface blit failed\n");
        gui_surface_destroy(surface_id);
        return 1;
    }

    send_msg(GUI_MSG_V2_CREATE_SURFACE_WINDOW, surface_id,
        340, 190, SURFACE_W, SURFACE_H, "SURFACE DEMO");
    send_msg(GUI_MSG_V2_DAMAGE_SURFACE, surface_id,
        0, 0, SURFACE_W, SURFACE_H, "");

    start = (uint64_t)srv_ticks();
    for (;;) {
        uint64_t elapsed = (uint64_t)srv_ticks() - start;
        if (elapsed > 140) {
            break;
        }
        if ((elapsed % 20) == 0) {
            draw_demo(elapsed);
            gui_surface_blit(surface_id, 0, 0, SURFACE_W, SURFACE_H, pixels, SURFACE_W);
            send_msg(GUI_MSG_V2_DAMAGE_SURFACE, surface_id,
                0, 0, SURFACE_W, SURFACE_H, "");
        }
        srv_yield();
    }

    send_msg(GUI_MSG_V2_DESTROY_SURFACE, surface_id,
        0, 0, SURFACE_W, SURFACE_H, "");
    gui_surface_destroy(surface_id);
    srv_puts("surfacedemo: exited\n");
    return 0;
}
