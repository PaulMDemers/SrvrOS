#ifndef SRVROS_DESK_COMMANDS_H
#define SRVROS_DESK_COMMANDS_H
#include <srvros/desktop.h>
#include <srvros/key_event.h>
#include <srvros/desktop_menu.h>
/* Snapshot supplied by the app each turn: flags use DESK_MENU_DISABLED/CHECKED.
 * CTRL in a binding means the primary modifier (Ctrl or Meta); other supported
 * modifiers match exactly. Repeated/canceled key events never invoke commands.
 * Label/shortcut are borrowed only for the duration of a call. */
struct desk_command {
    unsigned id,flags,usage,modifiers;
    const char *label,*shortcut;
};
unsigned desk_commands_key(const struct desk_command *,unsigned,const struct srv_key_event *);
int desk_commands_enabled(const struct desk_command *,unsigned,unsigned id);
/* 1 invoked,0 unknown/disabled/ambiguous. Menu, button and key callers use the
 * same guard and handler; this does not queue or replay stale actions. */
int desk_commands_invoke(const struct desk_command *,unsigned,unsigned,
                         void (*)(void *,unsigned),void *);
int desk_command_menu(const struct desk_command *,struct desk_menu_item *);
#endif
