#include "../userspace/desktools/menus.c"
#include <assert.h>
#include <stdio.h>
void srv_puts(const char *text) { (void)text; }
void tool_content_focus(struct tool *t,int active) { desk_ui_window_focus(&t->ui,active);if(t->search_open) desk_searchbar_focus(&t->search,active && t->search_focus); }
static struct tool t;
int main(void) {
    t.role=DESK_APP_TEXT;t.window.width=720;t.window.height=480;t.window.focused=1;
    desk_ui_init(&t.ui);desk_popup_init(&t.context);desk_ui_window_focus(&t.ui,1);
    desk_text_buffer_init(&t.body_text,t.body_store[0],t.body_store[1],sizeof(t.body_store[0]));desk_text_buffer_set(&t.body_text,"Selected text remains selected",30);
    desk_edit_init(&t.body,&t.body_text,1);desk_edit_configure(&t.body,(struct desk_rect){16,100,688,320},&desk_font_16,1);desk_ui_add(&t.ui,TOOL_BODY,DESK_CONTROL_EDIT,&t.body,NULL);desk_ui_focus(&t.ui,TOOL_BODY);
    desk_text_buffer_select(&t.body_text,3,12);tool_context_open(&t,40,120,0);
    assert(t.context.opened && t.body_text.anchor==3 && t.body_text.caret==12 && !t.context.items[3].flags);
    desk_popup_cancel(&t.context);t.clip.mode=1;tool_context_open(&t,40,120,0);assert(t.context.items[3].flags&DESK_MENU_DISABLED);t.clip.mode=0;
    desk_popup_cancel(&t.context);t.modal=1;tool_context_open(&t,40,120,0);assert(!t.context.opened);t.modal=0;
    tool_context_open(&t,0,0,1);assert(t.context.opened && t.context.selected==3); /* Undo/redo disabled; Cut is first enabled. */
    desk_popup_cancel(&t.context);t.role=DESK_APP_FILES;desk_table_init(&t.table);
    static const struct desk_table_column columns[]={{"Name",0,0,0}};desk_table_columns(&t.table,columns,1);
    struct desk_table_row rows[]={{1,{"Folder"}},{2,{"File"}}};desk_table_rows(&t.table,rows,2);desk_table_configure(&t.table,(struct desk_rect){16,100,688,250},1);
    desk_ui_add(&t.ui,TOOL_LIST,DESK_CONTROL_TABLE,&t.table,NULL);t.file_count=2;t.entries[0].directory=1;
    assert(desk_list_index_at(&t.table.body,40,145)==0);
    struct desk_rect hit_row=desk_list_row_rect(&t.table.body,0);assert(desk_widget_contains(hit_row,40,145));
    assert(!desk_list_row_rect(&t.table.body,2).w && desk_list_index_at(&t.table.body,702,145)==-1);
    tool_context_open(&t,40,145,0);assert(t.context.opened && t.table.body.selected==0 && (t.context.items[2].flags&DESK_MENU_DISABLED));
    desk_popup_cancel(&t.context);tool_context_open(&t,40,175,0);assert(t.table.body.selected==1 && !t.context.items[2].flags);
    desk_popup_cancel(&t.context);tool_context_open(&t,40,300,0);assert(t.table.body.selected==-1 && (t.context.items[0].flags&DESK_MENU_DISABLED));
    desk_popup_cancel(&t.context);t.job.busy=1;tool_context_open(&t,40,145,0);assert(!t.context.opened);t.job.busy=0;
    desk_ui_focus(&t.ui,TOOL_LIST);desk_list_select(&t.table.body,1);tool_context_open(&t,0,0,1);assert(t.context.opened && t.table.body.selected==1);
    puts("tool-context: PASS selection preservation, clipboard busy/disabled state, modal guards, clicked file selection without activation, folder restrictions, empty area and keyboard anchors");
}
