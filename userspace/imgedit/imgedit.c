#include <srvros/bmp.h>
#include <srvros/gui.h>
#include <srvros/sys.h>

#include <stddef.h>
#include <stdint.h>

#define WIN 1
#define CANVAS 100
#define STATUS 101
#define BTN_BLACK 200
#define BTN_RED 201
#define BTN_GREEN 202
#define BTN_BLUE 203
#define BTN_WHITE 204
#define BTN_CLEAR 205
#define BTN_SAVE 206

#define CANVAS_W 160
#define CANVAS_H 120
#define BRUSH 4

static uint32_t pixels[CANVAS_W * CANVAS_H];
static uint8_t bmp_buffer[54 + CANVAS_W * CANVAS_H * 4];

static void copy_text(char *to, const char *from) {
    uint64_t i = 0;
    while (from[i] != '\0' && i + 1 < GUI_TEXT_MAX) {
        to[i] = from[i];
        i++;
    }
    to[i] = '\0';
}

static void send_msg(uint64_t type, uint64_t control, int64_t x, int64_t y,
    uint64_t width, uint64_t height, int64_t value, const char *text) {
    struct gui_message msg = {
        .type = type,
        .window_id = WIN,
        .control_id = control,
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .value = value,
    };
    copy_text(msg.text, text);
    gui_send(&msg);
}

static void set_status(const char *text) {
    send_msg(GUI_MSG_SET_TEXT, STATUS, 0, 0, 0, 0, 0, text);
}

static void set_pixel(uint64_t x, uint64_t y, uint32_t color) {
    if (x >= CANVAS_W || y >= CANVAS_H) {
        return;
    }
    pixels[y * CANVAS_W + x] = color;
    send_msg(GUI_MSG_SET_IMAGE_PIXEL, CANVAS, (int64_t)x, (int64_t)y, 0, 0, color, "");
}

static void paint_at(int64_t x, int64_t y, uint32_t color) {
    for (int64_t py = y - (BRUSH / 2); py < y + (int64_t)(BRUSH / 2); py++) {
        for (int64_t px = x - (BRUSH / 2); px < x + (int64_t)(BRUSH / 2); px++) {
            if (px >= 0 && py >= 0) {
                set_pixel((uint64_t)px, (uint64_t)py, color);
            }
        }
    }
}

static void fill_canvas_local(uint32_t color) {
    for (uint64_t i = 0; i < CANVAS_W * CANVAS_H; i++) {
        pixels[i] = color;
    }
}

static void fill_canvas(uint32_t color) {
    fill_canvas_local(color);
    send_msg(GUI_MSG_FILL_IMAGE, CANVAS, 0, 0, 0, 0, color, "");
}

static void save_canvas(const char *path) {
    size_t size = 0;
    if (bmp_encode_rgba32(pixels, CANVAS_W, CANVAS_H,
            bmp_buffer, sizeof(bmp_buffer), &size) < 0) {
        set_status("BMP ENCODE FAILED");
        return;
    }
    if (srv_fs_write(path, bmp_buffer, size) < 0) {
        set_status("SAVE FAILED");
        return;
    }
    set_status("SAVED");
}

int main(int argc, char **argv) {
    uint32_t color = 0x000000;
    const char *path = argc > 1 ? argv[1] : "/fat/paint.bmp";

    srv_puts("imgedit: start\n");
    fill_canvas_local(0xffffff);
    send_msg(GUI_MSG_CREATE_WINDOW, 0, 250, 130, 304, 230, 0, "IMAGE EDIT");
    send_msg(GUI_MSG_ADD_IMAGE, CANVAS, 18, 38, CANVAS_W, CANVAS_H, 0xffffff, "");
    send_msg(GUI_MSG_ADD_LABEL, STATUS, 190, 38, 88, 52, 0, "BMP READY");
    send_msg(GUI_MSG_ADD_BUTTON, BTN_BLACK, 18, 172, 44, 24, 0, "BLK");
    send_msg(GUI_MSG_ADD_BUTTON, BTN_RED, 66, 172, 44, 24, 0, "RED");
    send_msg(GUI_MSG_ADD_BUTTON, BTN_GREEN, 114, 172, 44, 24, 0, "GRN");
    send_msg(GUI_MSG_ADD_BUTTON, BTN_BLUE, 162, 172, 44, 24, 0, "BLU");
    send_msg(GUI_MSG_ADD_BUTTON, BTN_WHITE, 210, 172, 44, 24, 0, "WHT");
    send_msg(GUI_MSG_ADD_BUTTON, BTN_CLEAR, 18, 200, 72, 24, 0, "CLEAR");
    send_msg(GUI_MSG_ADD_BUTTON, BTN_SAVE, 98, 200, 72, 24, 0, "SAVE");

    for (;;) {
        struct gui_message event;
        if (gui_recv(&event) <= 0) {
            srv_yield();
            continue;
        }
        if (event.type == GUI_MSG_EVENT_CLOSE) {
            srv_puts("imgedit: close\n");
            return 0;
        }
        if (event.type == GUI_MSG_EVENT_IMAGE_CLICK && event.control_id == CANVAS) {
            paint_at(event.x, event.y, color);
            continue;
        }
        if (event.type != GUI_MSG_EVENT_CLICK) {
            continue;
        }
        if (event.control_id == BTN_BLACK) {
            color = 0x000000;
            set_status("BLACK");
        } else if (event.control_id == BTN_RED) {
            color = 0xd9534f;
            set_status("RED");
        } else if (event.control_id == BTN_GREEN) {
            color = 0x2ea043;
            set_status("GREEN");
        } else if (event.control_id == BTN_BLUE) {
            color = 0x1f6feb;
            set_status("BLUE");
        } else if (event.control_id == BTN_WHITE) {
            color = 0xffffff;
            set_status("WHITE");
        } else if (event.control_id == BTN_CLEAR) {
            fill_canvas(0xffffff);
            set_status("CLEARED");
        } else if (event.control_id == BTN_SAVE) {
            save_canvas(path);
        }
    }
}
