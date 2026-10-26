#include "../userspace/desktools/menus.c"
#include <assert.h>
#include <srvros/desk_connection.h>
#include <stdio.h>
static unsigned selected,canceled;
void tool_picker_cancel(struct tool *t) {t->picker_mode=0;canceled++;}
void tool_text_dialog(struct tool *t,unsigned id) {assert(id==1003);t->modal=0;canceled++;}
void tool_text_request(struct tool *t,int pending) {(void)t;selected=1000+(unsigned)pending;}
void tool_new_window(struct tool *t) {(void)t;selected=MENU_NEW_WINDOW;}
void tool_text_save(struct tool *t,int choose) {(void)t;assert(choose);selected=MENU_SAVE_AS;}
void tool_text_action(struct tool *t,struct desk_action a) {(void)t;assert(a.flags&DESK_WIDGET_ACTIVATED);selected=a.id;}
static struct tool t;
int main(void) {
    t.role=DESK_APP_TEXT;struct desk_connection connection={0};t.window.connection=&connection;
    struct srv_key_event key={.usage=0x16,.modifiers=1,.flags=SRV_KEY_DOWN_EVENT};
    assert(tool_command_key(&t,&key) && selected==TOOL_SAVE);
    selected=0;assert(tool_command_action(&t,TOOL_SAVE) && selected==TOOL_SAVE);
    selected=0;t.job.busy=1;assert(!tool_command_key(&t,&key));assert(tool_command_action(&t,TOOL_SAVE) && !selected);
    struct desk_command commands[7];text_commands(&t,commands);
    struct desk_menu_item menu;assert(!desk_command_menu(&commands[2],&menu) && (menu.flags&DESK_MENU_DISABLED));
    key.usage=0x11;key.modifiers=0x82;assert(tool_command_key(&t,&key) && selected==MENU_NEW_WINDOW);
    t.job.busy=0;t.modal=1;selected=0;assert(tool_command_action(&t,MENU_CLOSE) && !selected);
    key.usage=0x14;key.modifiers=0x80;assert(tool_command_key(&t,&key) && t.quit_application && canceled==1 && !t.modal);
    t.quit_application=0;t.picker_mode=DESK_PICKER_SAVE;assert(tool_command_action(&t,MENU_QUIT) && t.quit_application && canceled==2 && !t.picker_mode);
    key.usage=0x16;key.flags|=SRV_KEY_REPEAT;selected=0;assert(!tool_command_key(&t,&key) && !selected);
    t.role=DESK_APP_FILES;assert(!tool_command_action(&t,TOOL_OPEN));
    puts("tool-commands: PASS Text key/button/menu guard parity, busy save refusal, independent new window, modal close protection and common Quit cancellation");
}
