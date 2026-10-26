#include <unistd.h>
int command_probe_chdir(const char *path);
#define chdir command_probe_chdir
#define main terminal_main
#include "../userspace/deskterm/deskterm.c"
#undef main
#undef chdir
#include <assert.h>
#include <stdio.h>
static unsigned invoked;
static void record(void *context,unsigned id) {(void)context;invoked=id;}
int main(void) {
    struct srv_key_event key={.flags=SRV_KEY_DOWN_EVENT,.modifiers=SRV_KEY_CTRL,.usage=6};
    assert(!terminal_shortcut(&key)); /* Ctrl-C belongs to the shell. */
    key.usage=7;assert(!terminal_shortcut(&key)); /* Ctrl-D */
    key.usage=0x0f;assert(!terminal_shortcut(&key)); /* Ctrl-L */
    key.usage=6;key.modifiers|=SRV_KEY_SHIFT;assert(terminal_shortcut(&key)==2);
    key.modifiers=0x80;assert(terminal_shortcut(&key)==2); /* Right Meta-C */
    key.modifiers=0x12;assert(terminal_shortcut(&key)==2); /* Left Shift/right Ctrl */
    key.flags|=SRV_KEY_REPEAT;assert(terminal_shortcut(&key)==2); /* Consumed, never shell input. */
    key.flags=SRV_KEY_DOWN_EVENT|SRV_KEY_CANCEL;assert(!terminal_shortcut(&key));
    key.flags=0;assert(!terminal_shortcut(&key));
    key=(struct srv_key_event){.flags=SRV_KEY_DOWN_EVENT,.modifiers=SRV_KEY_SHIFT,.usage=SRV_KEY_PAGEUP};
    assert(terminal_shortcut(&key)==TERM_PAGE_UP);
    struct desk_command commands[9];unsigned count=terminal_commands(commands);
    assert(!desk_commands_enabled(commands,count,2) && !desk_commands_enabled(commands,count,3));
    app.child=1;app.view.caret=5;terminal_commands(commands);
    assert(desk_commands_enabled(commands,count,2) && desk_commands_enabled(commands,count,3));
    assert(desk_commands_invoke(commands,count,3,record,NULL) && invoked==3);
    app.clipboard.mode=1;terminal_commands(commands);invoked=0;
    assert(!desk_commands_invoke(commands,count,3,record,NULL) && !invoked);
    app.modal=1;terminal_commands(commands);assert(!desk_commands_enabled(commands,count,1) && !desk_commands_enabled(commands,count,TERM_CLOSE));
    app.modal=0;app.clipboard.mode=0;app.exited=1;terminal_commands(commands);
    assert(desk_commands_enabled(commands,count,2) && !desk_commands_enabled(commands,count,3) && !desk_commands_enabled(commands,count,4));
    struct desk_menu_item item;assert(!desk_command_menu(&commands[2],&item) && (item.flags&DESK_MENU_DISABLED));
    puts("terminal-commands: PASS shell control-key preservation, Meta/Ctrl-Shift bindings, repeat/cancel classification and shared modal/session/clipboard guards");
}
