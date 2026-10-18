#include <srvros/desk_file_picker.h>
#include <srvros/desk_path.h>
#include <string.h>
static int valid(const char *text,size_t capacity) {
    if(!text) return 0;
    size_t n=0;
    while(n<capacity && text[n]) { if((unsigned char)text[n]<32 || (unsigned char)text[n]==127) return 0;n++; }
    return n<capacity && desk_text_utf8_valid(text,n);
}
static int directory_valid(const char *path) { char normalized[DESK_PICKER_PATH];return !desk_path_normalize(normalized,sizeof(normalized),path); }
static int name_valid(const char *name) {
    return valid(name,DESK_PICKER_NAME) && desk_path_name_valid(name);
}
static void directory_copy(char *out,const char *path) {
    (void)desk_path_normalize(out,DESK_PICKER_PATH,path);
}
static void layout(struct desk_file_picker *p) {
    struct desk_rect r=p->frame;int w=r.w,h=r.h,x=r.x,y=r.y;
    int enabled=!p->busy && w>=240 && h>=180;
    desk_button_configure(&p->buttons[0],desk_intersect(r,(struct desk_rect){x+w-172,y+10,60,26}),enabled);
    desk_button_configure(&p->buttons[1],desk_intersect(r,(struct desk_rect){x+w-104,y+10,88,26}),enabled);
    desk_button_configure(&p->buttons[2],desk_intersect(r,(struct desk_rect){x+w-100,y+h-36,84,28}),enabled);
    desk_button_configure(&p->buttons[3],desk_intersect(r,(struct desk_rect){x+w-192,y+h-36,84,28}),1);
    desk_edit_configure(&p->location,desk_intersect(r,(struct desk_rect){x+16,y+40,w>32?w-32:0,30}),&desk_font_16,enabled);
    desk_list_configure(&p->list,desk_intersect(r,(struct desk_rect){x+16,y+78,w>32?w-32:0,h>184?h-184:0}),enabled);
    desk_edit_configure(&p->name,desk_intersect(r,(struct desk_rect){x+64,y+h-74,w>80?w-80:0,30}),&desk_font_16,enabled);
    desk_ui_layout_changed(&p->ui);
}
void desk_picker_init(struct desk_file_picker *p) {
    *p=(struct desk_file_picker){0};desk_ui_init(&p->ui);desk_list_init(&p->list);
    desk_text_buffer_init(&p->location_text,p->location_data[0],p->location_data[1],DESK_PICKER_PATH);
    desk_text_buffer_init(&p->name_text,p->name_data[0],p->name_data[1],DESK_PICKER_NAME);
    desk_edit_init(&p->location,&p->location_text,0);desk_edit_init(&p->name,&p->name_text,0);
}
int desk_picker_open(struct desk_file_picker *p,enum desk_picker_mode mode,const char *directory,const char *name) {
    if((mode!=DESK_PICKER_OPEN && mode!=DESK_PICKER_SAVE) || !directory_valid(directory) ||
        !valid(name,DESK_PICKER_NAME) || (name[0] && !name_valid(name))) return -1;
    char folder[DESK_PICKER_PATH],filename[DESK_PICKER_NAME];directory_copy(folder,directory);strcpy(filename,name);
    struct desk_rect frame=p->frame;desk_picker_init(p);p->frame=frame;p->mode=mode;
    strcpy(p->directory,folder);strcpy(p->target,folder);
    desk_text_buffer_set(&p->location_text,folder,strlen(folder));desk_text_buffer_set(&p->name_text,filename,strlen(filename));
    desk_ui_add(&p->ui,DESK_PICKER_UP,DESK_CONTROL_BUTTON,&p->buttons[0],"Up");
    desk_ui_add(&p->ui,DESK_PICKER_REFRESH,DESK_CONTROL_BUTTON,&p->buttons[1],"Refresh");
    desk_ui_add(&p->ui,DESK_PICKER_LOCATION,DESK_CONTROL_EDIT,&p->location,NULL);
    desk_ui_add(&p->ui,DESK_PICKER_LIST,DESK_CONTROL_LIST,&p->list,NULL);
    desk_ui_add(&p->ui,DESK_PICKER_FILENAME,DESK_CONTROL_EDIT,&p->name,NULL);
    desk_ui_add(&p->ui,DESK_PICKER_DISMISS,DESK_CONTROL_BUTTON,&p->buttons[3],"Cancel");
    desk_ui_add(&p->ui,DESK_PICKER_OK,DESK_CONTROL_BUTTON,&p->buttons[2],mode==DESK_PICKER_SAVE?"Save":"Open");
    strcpy(p->status,mode==DESK_PICKER_SAVE?"Choose a folder and a filename.":"Choose a file to open.");layout(p);
    desk_ui_focus(&p->ui,mode==DESK_PICKER_SAVE?DESK_PICKER_FILENAME:DESK_PICKER_LIST);return 0;
}
void desk_picker_configure(struct desk_file_picker *p,struct desk_rect bounds) {
    if(bounds.w<0 || bounds.h<0 || bounds.w>16384 || bounds.h>16384 || bounds.x<0 || bounds.y<0 || bounds.x>16384 || bounds.y>16384) return;
    p->frame=desk_widget_inset(bounds,8);layout(p);
}
void desk_picker_loading(struct desk_file_picker *p) { p->busy=1;strcpy(p->status,"Opening folder…");layout(p); }
int desk_picker_listing(struct desk_file_picker *p,const char *directory,const struct desk_list_item *items,int count) {
    if(!directory_valid(directory) || count<0 || count>DESK_LIST_MAX_ITEMS || (count && !items)) return -1;
    for(int i=0;i<count;i++) if(!name_valid(items[i].label) || (items[i].folder!=0 && items[i].folder!=1)) return -1;
    directory_copy(p->directory,directory);desk_text_buffer_set(&p->location_text,p->directory,strlen(p->directory));
    desk_list_set_items(&p->list,items,count);p->busy=0;
    if(p->mode==DESK_PICKER_OPEN) desk_text_buffer_set(&p->name_text,"",0);
    strcpy(p->status,count?"Double-click a folder to browse it.":"This folder is empty.");layout(p);
    desk_ui_focus(&p->ui,p->mode==DESK_PICKER_SAVE?DESK_PICKER_FILENAME:DESK_PICKER_LIST);
    if(p->mode==DESK_PICKER_SAVE) desk_text_buffer_select(&p->name_text,0,p->name_text.length);
    return 0;
}
void desk_picker_message(struct desk_file_picker *p,const char *message) {
    if(valid(message,sizeof(p->status))) strcpy(p->status,message);
}
void desk_picker_error(struct desk_file_picker *p,const char *message) {
    p->busy=0;desk_text_buffer_set(&p->location_text,p->directory,strlen(p->directory));
    if(valid(message,sizeof(p->status))) strcpy(p->status,message);else strcpy(p->status,"This folder could not be opened.");layout(p);
}
unsigned desk_picker_focus(struct desk_file_picker *p,int active) { return desk_ui_window_focus(&p->ui,active); }
static int join(struct desk_file_picker *p,const char *name) {
    return name_valid(name)?desk_path_join(p->target,sizeof(p->target),p->directory,name):-1;
}
static struct desk_picker_result browse(struct desk_file_picker *p,const char *directory) {
    if(!directory_valid(directory)) { strcpy(p->status,"Enter an absolute folder path.");return (struct desk_picker_result){.flags=DESK_WIDGET_REDRAW}; }
    directory_copy(p->target,directory);return (struct desk_picker_result){DESK_PICKER_BROWSE,DESK_WIDGET_REDRAW,0};
}
static struct desk_picker_result accept(struct desk_file_picker *p) {
    if(join(p,p->name_text.data)) { strcpy(p->status,"Choose a filename without a slash, within the path limit.");return (struct desk_picker_result){.flags=DESK_WIDGET_REDRAW}; }
    int selected=p->list.selected;
    if(p->mode==DESK_PICKER_OPEN && selected>=0 && p->list.items[selected].folder && !strcmp(p->list.items[selected].label,p->name_text.data))
        return (struct desk_picker_result){DESK_PICKER_BROWSE,DESK_WIDGET_REDRAW,0};
    return (struct desk_picker_result){DESK_PICKER_ACCEPT,DESK_WIDGET_REDRAW,0};
}
static struct desk_picker_result action(struct desk_file_picker *p,struct desk_action a) {
    struct desk_picker_result result={.flags=a.flags,.field=a.id};
    if(a.id==DESK_PICKER_DISMISS && (a.flags&DESK_WIDGET_ACTIVATED)) { result.operation=DESK_PICKER_CANCEL;return result; }
    if(p->busy) return result;
    if(a.id==DESK_PICKER_LIST && (a.flags&(DESK_WIDGET_CHANGED|DESK_WIDGET_ACTIVATED)) && p->list.selected>=0) {
        const struct desk_list_item *item=&p->list.items[p->list.selected];
        if(!item->folder || p->mode==DESK_PICKER_OPEN) desk_text_buffer_set(&p->name_text,item->label,strlen(item->label));
        if(a.flags&DESK_WIDGET_ACTIVATED) {
            if(join(p,item->label)) { strcpy(p->status,"This path is too long.");return result; }
            result.operation=item->folder?DESK_PICKER_BROWSE:DESK_PICKER_ACCEPT;
        }
        return result;
    }
    if((a.id==DESK_PICKER_OK && (a.flags&DESK_WIDGET_ACTIVATED)) || (a.id==DESK_PICKER_FILENAME && (a.flags&DESK_EDIT_SUBMIT))) return accept(p);
    if(a.id==DESK_PICKER_LOCATION && (a.flags&DESK_EDIT_SUBMIT)) return browse(p,p->location_text.data);
    if(a.flags&DESK_WIDGET_ACTIVATED) {
        if(a.id==DESK_PICKER_REFRESH) return browse(p,p->directory);
        if(a.id==DESK_PICKER_UP) {
            char parent[DESK_PICKER_PATH];
            if(!desk_path_parent(parent,sizeof(parent),p->directory)) return browse(p,parent);
        }
    }
    return result;
}
struct desk_picker_result desk_picker_key(struct desk_file_picker *p,const struct srv_key_event *key) {
    if(p->ui.active && (key->flags&SRV_KEY_DOWN_EVENT)) {
        if(key->usage==SRV_KEY_ESCAPE) return (struct desk_picker_result){DESK_PICKER_CANCEL,0,0};
        if(p->location.enabled && (key->modifiers&(SRV_KEY_CTRL|SRV_KEY_META)) && key->usage==0x0f) {
            desk_ui_focus(&p->ui,DESK_PICKER_LOCATION);desk_text_buffer_select(&p->location_text,0,p->location_text.length);
            return (struct desk_picker_result){.flags=DESK_WIDGET_REDRAW};
        }
        if(p->location.enabled && (key->modifiers&SRV_KEY_ALT) && key->usage==SRV_KEY_UP) return action(p,(struct desk_action){DESK_PICKER_UP,DESK_WIDGET_ACTIVATED});
        if(p->location.enabled && key->usage==SRV_KEY_F5) return action(p,(struct desk_action){DESK_PICKER_REFRESH,DESK_WIDGET_ACTIVATED});
    }
    return action(p,desk_ui_key(&p->ui,key));
}
struct desk_picker_result desk_picker_pointer(struct desk_file_picker *p,struct desk_widget_event event,uint64_t time_us) {
    return action(p,desk_ui_pointer(&p->ui,event,time_us));
}
struct desk_picker_result desk_picker_scroll(struct desk_file_picker *p,int x,int y,int delta) {
    return action(p,desk_ui_scroll(&p->ui,x,y,0,delta));
}
void desk_picker_draw(struct desk_canvas *c,const struct desk_file_picker *p,const struct desk_theme *t) {
    struct desk_rect r=p->frame,clip=c->clip;
    desk_fill(c,(struct desk_rect){r.x+3,r.y+4,r.w,r.h},0xb9c2b4);desk_fill(c,r,t->border);
    desk_fill(c,desk_widget_inset(r,1),t->surface);desk_clip(c,desk_intersect(clip,r));
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+10,r.w>200?r.w-200:0,28},t->heading,p->mode==DESK_PICKER_SAVE?(r.w<400?"Save":"Save document"):(r.w<400?"Open":"Open document"),t->text);
    desk_ui_draw(c,&p->ui,t);
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+r.h-100,r.w>32?r.w-32:0,22},t->body,p->status,t->muted);
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+r.h-68,48,24},t->body,"Name",t->text);
    c->clip=clip;
}
