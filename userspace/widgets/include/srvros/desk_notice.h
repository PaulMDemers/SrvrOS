#ifndef SRVROS_DESK_NOTICE_H
#define SRVROS_DESK_NOTICE_H
#include <srvros/desk_widgets.h>
#define DESK_NOTICE_HEADING 64
#define DESK_NOTICE_MESSAGE 192
enum desk_notice_tone { DESK_NOTICE_INFO,DESK_NOTICE_SUCCESS,DESK_NOTICE_WARNING };
/* Owned text snapshot for an informational card. Interaction and expiry are
 * application policy; this control neither takes focus nor reads the clock. */
struct desk_notice {
    struct desk_rect frame;
    const struct desk_font *body;
    char heading[DESK_NOTICE_HEADING],message[DESK_NOTICE_MESSAGE];
    enum desk_notice_tone tone;
    unsigned message_bottom; /* Reserved pixels below the body for owner content. */
    unsigned heading_trailing; /* Reserved pixels for an owner-drawn accessory. */
};
void desk_notice_init(struct desk_notice *notice);
int desk_notice_set(struct desk_notice *notice,const char *heading,const char *message,enum desk_notice_tone tone);
void desk_notice_configure(struct desk_notice *notice,struct desk_rect frame,const struct desk_font *body);
void desk_notice_draw(struct desk_canvas *canvas,const struct desk_notice *notice,const struct desk_theme *theme);
/* Determinate progress. Value is clamped to maximum; maximum zero is empty.
 * Both values use caller-defined units. Rendering is allocation/clock-free. */
void desk_progress_draw(struct desk_canvas *canvas,struct desk_rect frame,uint32_t value,uint32_t maximum,const struct desk_theme *theme);
#endif
