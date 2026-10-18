#ifndef SRVROS_DESK_WIDGETS_H
#define SRVROS_DESK_WIDGETS_H

#include <srvros/desk_render.h>

/* Client-side controls: no IPC, syscalls, allocation, file access or callbacks.
 * Apps own state and dispatch normalized events in content coordinates. Do not
 * change state fields directly; configure again on layout/enable changes.
 * Drawing intersects and restores the canvas clip, including an ancestor's clip.
 * Frames are half-open, with representable int32_t right/bottom coordinates. */
struct desk_theme {
    uint32_t surface, text, muted, accent, selected, track, thumb, border;
    const struct desk_font *body, *heading;
    int32_t spacing, padding, control_height;
};
extern const struct desk_theme desk_theme_quiet;

enum desk_widget_event_type {
    DESK_WIDGET_POINTER, DESK_WIDGET_CANCEL, DESK_WIDGET_ACTIVATE,
    DESK_WIDGET_FOCUS_IN, DESK_WIDGET_FOCUS_OUT
};
struct desk_widget_event {
    enum desk_widget_event_type type;
    int32_t x, y;
    int down, pressed; /* Primary button held / initial press edge. */
};
enum desk_widget_result {
    DESK_WIDGET_REDRAW = 1, DESK_WIDGET_CHANGED = 2, DESK_WIDGET_ACTIVATED = 4
};
int desk_widget_contains(struct desk_rect frame, int32_t x, int32_t y);
struct desk_rect desk_widget_inset(struct desk_rect frame, int32_t inset);
void desk_label_draw(struct desk_canvas *canvas, struct desk_rect frame,
                     const struct desk_font *font, const char *text, uint32_t color);
/* Single-line label with a UTF-8 ellipsis when content exceeds its frame.
 * Uses the full frame width (independent of the damage clip). If even the
 * ellipsis cannot fit, draw nothing. No allocation or source modification. */
void desk_label_ellipsis(struct desk_canvas *canvas, struct desk_rect frame,
                         const struct desk_font *font, const char *text, uint32_t color);

/* Zero-initialize. Reconfiguration cancels capture when geometry changes.
 * content and viewport use the same units as offset (normally pixels).
 * Both are clamped to nonnegative; zero viewport disables interaction. */
struct desk_scrollbar {
    struct desk_rect track;
    int32_t content, viewport, offset, grab;
    int dragging, horizontal;
};
struct desk_scrollbar_geometry {
    struct desk_rect thumb;
    int32_t limit, travel;
};
void desk_scrollbar_configure(struct desk_scrollbar *bar, struct desk_rect track,
                             int32_t content, int32_t viewport);
/* Horizontal variant has identical paging/capture rules along the x axis.
 * Calling the other configure function changes axis and cancels capture. */
void desk_scrollbar_configure_horizontal(struct desk_scrollbar *bar,struct desk_rect track,
                                         int32_t content,int32_t viewport);
int32_t desk_scrollbar_limit(const struct desk_scrollbar *bar);
struct desk_scrollbar_geometry desk_scrollbar_geometry(const struct desk_scrollbar *bar);
unsigned desk_scrollbar_set(struct desk_scrollbar *bar, int64_t offset);
unsigned desk_scrollbar_scroll(struct desk_scrollbar *bar, int64_t delta);
unsigned desk_scrollbar_event(struct desk_scrollbar *bar, struct desk_widget_event event);
void desk_scrollbar_draw(struct desk_canvas *canvas, const struct desk_scrollbar *bar,
                        const struct desk_theme *theme);

/* Zero-initialize. Pointer activation occurs only on release inside after a
 * press inside; CANCEL/focus loss/disable/resize disarm. ACTIVATE is an abstract
 * keyboard action from the app's focused-control router (not a raw key code).
 * Return ACTIVATED to the app; controls never perform application actions. */
struct desk_button {
    struct desk_rect frame;
    int enabled, focused, armed, inside, centered;
};
void desk_button_configure(struct desk_button *button, struct desk_rect frame, int enabled);
void desk_button_centered(struct desk_button *button,int centered);
unsigned desk_button_event(struct desk_button *button, struct desk_widget_event event);
void desk_button_draw(struct desk_canvas *canvas, const struct desk_button *button,
                      const char *label, const struct desk_theme *theme);

/* Checkbox/radio share button capture and focus semantics. A checkbox toggles;
 * a radio only selects itself. The owning app maintains mutual exclusion of a
 * radio group by configuring its peers after activation. */
struct desk_check { struct desk_button button; int checked, radio; };
void desk_check_configure(struct desk_check *check, struct desk_rect frame,
                          int enabled, int checked, int radio);
unsigned desk_check_event(struct desk_check *check, struct desk_widget_event event);
void desk_check_draw(struct desk_canvas *canvas, const struct desk_check *check,
                     const char *label, const struct desk_theme *theme);

#endif
