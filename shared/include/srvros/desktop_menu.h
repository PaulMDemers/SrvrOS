#ifndef SRVROS_DESKTOP_MENU_H
#define SRVROS_DESKTOP_MENU_H
#include <stdint.h>
#define DESK_APP_MENU_ITEMS 32
#define DESK_MENU_HEADING 1u
#define DESK_MENU_SEPARATOR 2u
#define DESK_MENU_DISABLED 4u
#define DESK_MENU_CHECKED 8u
/* Flat groups: each heading is followed by its rows. Action IDs are nonzero
 * and unique; headings/separators use zero. Labels are UTF-8, shortcuts are
 * display text. No client callbacks or pointers cross the desktop transport. */
struct desk_menu_item {
    uint32_t id,flags;
    char label[40],shortcut[24];
};
_Static_assert(sizeof(struct desk_menu_item)==72,"menu wire item");
#endif
