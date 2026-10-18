#ifndef DESKD_MENUS_H
#define DESKD_MENUS_H
#include <srvros/desktop_protocol.h>
#include <srvros/desktop_menu.h>
struct desk_menu_store {
    struct desk_menu_item items[DESK_APP_MENU_ITEMS],staging[DESK_APP_MENU_ITEMS];
    uint64_t revision,staging_revision;
    unsigned count,total,next;
};
/* Returns 1 complete validated replacement, 0 partial/invalid. */
int desk_menu_receive(struct desk_menu_store *store,const struct desk_packet *packet);
#endif
