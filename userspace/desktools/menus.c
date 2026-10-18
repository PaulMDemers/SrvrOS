#include "app.h"
#include <string.h>
enum { MENU_CLOSE=100,MENU_LOCATION,MENU_NEW_WINDOW,MENU_NEW_FOLDER,MENU_STANDARD,MENU_LARGER,MENU_SAVE_AS,MENU_EDIT=200 };
static struct desk_edit *edit_target(struct tool *t,unsigned *id) {
    if(t->modal || t->picker_mode || t->prompt_mode || t->ui.focus<0 || t->ui.focus>=(int)t->ui.count) return NULL;
    if(t->search_open && t->search_focus) { *id=TOOL_FIND_QUERY;return &t->search.query; }
    struct desk_control *c=&t->ui.controls[t->ui.focus];
    if(c->kind!=DESK_CONTROL_EDIT || !((struct desk_edit *)c->state)->enabled) return NULL;
    *id=c->id;return c->state;
}
static void append(struct desk_menu_item *items,unsigned *count,unsigned id,const char *label,const char *shortcut,unsigned flags) {
    struct desk_menu_item *m=&items[(*count)++];m->id=id;m->flags=flags;
    strcpy(m->label,label);if(shortcut) strcpy(m->shortcut,shortcut);
}
void tool_menu_refresh(struct tool *t) {
    struct desk_menu_item items[DESK_APP_MENU_ITEMS]={0};unsigned count=0;
    unsigned blocked=(t->modal || t->picker_mode || t->prompt_mode)?DESK_MENU_DISABLED:0,busy=blocked|(t->job.busy?DESK_MENU_DISABLED:0);
    append(items,&count,0,"File",NULL,DESK_MENU_HEADING);
    if(t->role==DESK_APP_TEXT) {
        append(items,&count,TOOL_NEW,"New document","Ctrl-N",busy);
        append(items,&count,TOOL_OPEN,"Open…","Ctrl-O",busy);
        append(items,&count,TOOL_SAVE,"Save","Ctrl-S",busy);
        append(items,&count,MENU_SAVE_AS,"Save as…","Ctrl-Shift-S",busy);
    } else {
        append(items,&count,MENU_NEW_WINDOW,"New window",NULL,blocked);
        if(t->role==DESK_APP_FILES) {
            append(items,&count,TOOL_OPEN,"Open selected item","Enter",busy|(t->table.body.selected<0?DESK_MENU_DISABLED:0));
            append(items,&count,MENU_NEW_FOLDER,"New folder…",NULL,busy);
            unsigned selected=busy|(t->table.body.selected<0?DESK_MENU_DISABLED:0);
            append(items,&count,TOOL_RENAME,"Rename…","F2",selected);
            append(items,&count,TOOL_DUPLICATE,"Duplicate…",NULL,selected);
            append(items,&count,TOOL_DELETE,"Delete…","Delete",selected);
        }
    }
    append(items,&count,0,"",NULL,DESK_MENU_SEPARATOR);
    append(items,&count,MENU_CLOSE,"Close window","Ctrl-W",blocked);
    if(t->role==DESK_APP_TEXT || t->role==DESK_APP_FILES || t->role==DESK_APP_CALCULATOR || t->role==DESK_APP_SETTINGS) {
        unsigned id=0;struct desk_edit *e=edit_target(t,&id);struct desk_text_buffer *text=e?e->text:NULL;
        unsigned disabled=blocked|(!e?DESK_MENU_DISABLED:0),selection=disabled|(!text || text->anchor==text->caret?DESK_MENU_DISABLED:0);
        append(items,&count,0,"Edit",NULL,DESK_MENU_HEADING);
        append(items,&count,MENU_EDIT+DESK_EDIT_UNDO,"Undo","Ctrl-Z",disabled|(!desk_text_buffer_can_undo(text,0)?DESK_MENU_DISABLED:0));
        append(items,&count,MENU_EDIT+DESK_EDIT_REDO,"Redo","Ctrl-Shift-Z",disabled|(!desk_text_buffer_can_undo(text,1)?DESK_MENU_DISABLED:0));
        append(items,&count,0,"",NULL,DESK_MENU_SEPARATOR);
        append(items,&count,MENU_EDIT+DESK_EDIT_COMMAND_CUT,"Cut","Ctrl-X",selection|(t->clip.mode?DESK_MENU_DISABLED:0));
        append(items,&count,MENU_EDIT+DESK_EDIT_COMMAND_COPY,"Copy","Ctrl-C",selection|(t->clip.mode?DESK_MENU_DISABLED:0));
        append(items,&count,MENU_EDIT+DESK_EDIT_COMMAND_PASTE,"Paste","Ctrl-V",disabled|(t->clip.mode?DESK_MENU_DISABLED:0));
        append(items,&count,0,"",NULL,DESK_MENU_SEPARATOR);
        append(items,&count,MENU_EDIT+DESK_EDIT_SELECT_ALL,"Select all","Ctrl-A",disabled);
    }
    if(t->role==DESK_APP_TEXT) {
        append(items,&count,0,"Search",NULL,DESK_MENU_HEADING);
        append(items,&count,TOOL_FIND,"Find…","Ctrl-F",busy);
        append(items,&count,TOOL_FIND_NEXT,"Find next","Ctrl-G",busy);
        append(items,&count,TOOL_FIND_PREVIOUS,"Find previous","Ctrl-Shift-G",busy);
        append(items,&count,TOOL_FIND_CLOSE,"Close search","Esc",blocked|(!t->search_open?DESK_MENU_DISABLED:0));
    } else if(t->role==DESK_APP_FILES) {
        append(items,&count,0,"Go",NULL,DESK_MENU_HEADING);
        append(items,&count,TOOL_BACK,"Back","Alt-Left",busy|(t->location_index<=0?DESK_MENU_DISABLED:0));
        append(items,&count,TOOL_FORWARD,"Forward","Alt-Right",busy|(t->location_index+1>=t->location_count?DESK_MENU_DISABLED:0));
        append(items,&count,TOOL_HOME,"Home","Alt-Home",busy);
        append(items,&count,TOOL_VOLUME,"Startup disk",NULL,busy);
        append(items,&count,TOOL_ROOT,"System root",NULL,busy);
        append(items,&count,TOOL_UP,"Parent folder","Alt-Up",busy);
        append(items,&count,MENU_LOCATION,"Location…","Ctrl-L",busy);
        append(items,&count,TOOL_REFRESH,"Refresh","Ctrl-R",busy);
    } else if(t->role==DESK_APP_CALCULATOR) {
        append(items,&count,0,"Calculate",NULL,DESK_MENU_HEADING);
        append(items,&count,1,"Evaluate","Enter",blocked);append(items,&count,2,"Clear",NULL,blocked);
    } else if(t->role==DESK_APP_SYSTEM) {
        append(items,&count,0,"View",NULL,DESK_MENU_HEADING);
        append(items,&count,SYSTEM_PAUSE,t->system_paused?"Resume updates":"Pause updates",NULL,blocked);
        append(items,&count,SYSTEM_REFRESH,"Refresh now","F5",blocked);
        append(items,&count,0,"",NULL,DESK_MENU_SEPARATOR);
        append(items,&count,SYSTEM_PROCESSES,"Processes",NULL,blocked|(desk_tabs_selected(&t->system_tabs)==SYSTEM_PROCESSES?DESK_MENU_CHECKED:0));
        append(items,&count,SYSTEM_NETWORK,"Network",NULL,blocked|(desk_tabs_selected(&t->system_tabs)==SYSTEM_NETWORK?DESK_MENU_CHECKED:0));
    } else if(t->role==DESK_APP_SETTINGS) {
        append(items,&count,0,"View",NULL,DESK_MENU_HEADING);
        append(items,&count,MENU_STANDARD,"Standard text",NULL,blocked|(t->body_size==16?DESK_MENU_CHECKED:0));
        append(items,&count,MENU_LARGER,"Larger text",NULL,blocked|(t->body_size==20?DESK_MENU_CHECKED:0));
    }
    desk_app_menus(&t->window,items,count);
}
void tool_menu_action(struct tool *t,unsigned id) {
    if(t->modal || t->picker_mode || t->prompt_mode) return;
    if(id>=MENU_EDIT && id<=MENU_EDIT+DESK_EDIT_REDO) {
        unsigned target=0;struct desk_edit *e=edit_target(t,&target);
        if(e) tool_dispatch(t,(struct desk_action){target,desk_edit_command(e,id-MENU_EDIT)});
    } else if(id==MENU_CLOSE) {
        if(t->role==DESK_APP_TEXT) tool_text_request(t,TOOL_PENDING_CLOSE);else t->closing=1;
    } else if(id==MENU_SAVE_AS) tool_text_save(t,1);
    else if(id==MENU_NEW_WINDOW) tool_launch(t,t->role,t->role==DESK_APP_FILES?t->saved_path:"");
    else if(id==MENU_LOCATION || id==MENU_NEW_FOLDER) {
        if(t->job.busy) return;
        unsigned target=id==MENU_LOCATION?TOOL_PATH:TOOL_NAME;
        struct desk_text_buffer *text=id==MENU_LOCATION?&t->path_text:&t->name_text;
        desk_ui_focus(&t->ui,target);desk_text_buffer_select(text,0,text->length);t->window.dirty=1;
        if(t->role==DESK_APP_TEXT) tool_status(t,"Enter a file path, then press Enter to open it.");
    } else if(id==MENU_STANDARD || id==MENU_LARGER) tool_dispatch(t,(struct desk_action){id==MENU_STANDARD?1:2,DESK_WIDGET_ACTIVATED});
    else tool_dispatch(t,(struct desk_action){id,DESK_WIDGET_ACTIVATED});
}
