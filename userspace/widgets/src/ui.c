#include <srvros/desk_ui.h>

static int enabled(const struct desk_control *c) {
    switch(c->kind) {
    case DESK_CONTROL_BUTTON: return ((struct desk_button *)c->state)->enabled;
    case DESK_CONTROL_EDIT: return ((struct desk_edit *)c->state)->enabled;
    case DESK_CONTROL_LIST: return ((struct desk_list *)c->state)->enabled;
    case DESK_CONTROL_CHECK: return ((struct desk_check *)c->state)->button.enabled;
    case DESK_CONTROL_TABLE: return ((struct desk_table *)c->state)->body.enabled;
    case DESK_CONTROL_CALENDAR: return ((struct desk_calendar *)c->state)->enabled;
    case DESK_CONTROL_TABS: return ((struct desk_tabs *)c->state)->enabled;
    case DESK_CONTROL_IMAGE: return ((struct desk_image *)c->state)->enabled;
    }
    return 0;
}
static struct desk_rect frame(const struct desk_control *c) {
    switch(c->kind) {
    case DESK_CONTROL_BUTTON: return ((struct desk_button *)c->state)->frame;
    case DESK_CONTROL_EDIT: return ((struct desk_edit *)c->state)->frame;
    case DESK_CONTROL_LIST: return ((struct desk_list *)c->state)->frame;
    case DESK_CONTROL_CHECK: return ((struct desk_check *)c->state)->button.frame;
    case DESK_CONTROL_TABLE: return ((struct desk_table *)c->state)->frame;
    case DESK_CONTROL_CALENDAR: return ((struct desk_calendar *)c->state)->frame;
    case DESK_CONTROL_TABS: return ((struct desk_tabs *)c->state)->frame;
    case DESK_CONTROL_IMAGE: return ((struct desk_image *)c->state)->frame;
    }
    return (struct desk_rect){0};
}
static unsigned event(struct desk_control *c,struct desk_widget_event e,uint64_t time_us) {
    switch(c->kind) {
    case DESK_CONTROL_BUTTON: return desk_button_event(c->state,e);
    case DESK_CONTROL_EDIT: return desk_edit_event(c->state,e);
    case DESK_CONTROL_LIST: return desk_list_event(c->state,e,time_us);
    case DESK_CONTROL_CHECK: return desk_check_event(c->state,e);
    case DESK_CONTROL_TABLE: return desk_table_event(c->state,e,time_us);
    case DESK_CONTROL_CALENDAR: return desk_calendar_event(c->state,e);
    case DESK_CONTROL_TABS: return desk_tabs_event(c->state,e);
    case DESK_CONTROL_IMAGE: return desk_image_event(c->state,e);
    }
    return 0;
}
void desk_ui_init(struct desk_ui *ui) { *ui=(struct desk_ui){.focus=-1,.capture=-1,.active=1}; }
int desk_ui_add(struct desk_ui *ui,unsigned id,enum desk_control_kind kind,void *state,const char *label) {
    if(!id || !state || kind<DESK_CONTROL_BUTTON || kind>DESK_CONTROL_IMAGE || ui->count==DESK_UI_MAX_CONTROLS) return -1;
    for(unsigned i=0;i<ui->count;i++) if(ui->controls[i].id==id || ui->controls[i].state==state) return -1;
    ui->controls[ui->count++]=(struct desk_control){id,kind,state,label?label:""};return 0;
}
static unsigned cancel(struct desk_ui *ui) {
    unsigned result=0;
    if(ui->capture>=0) result=event(&ui->controls[ui->capture],(struct desk_widget_event){.type=DESK_WIDGET_CANCEL},0);
    ui->capture=-1;return result;
}
static unsigned focus_index(struct desk_ui *ui,int index) {
    if(index==ui->focus) return 0;
    unsigned result=cancel(ui);
    if(ui->focus>=0) result|=event(&ui->controls[ui->focus],(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_OUT},0);
    ui->focus=index;
    if(index>=0 && ui->active) result|=event(&ui->controls[index],(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN},0);
    return result;
}
unsigned desk_ui_focus(struct desk_ui *ui,unsigned id) {
    if(!id) return focus_index(ui,-1);
    for(unsigned i=0;i<ui->count;i++) if(ui->controls[i].id==id && enabled(&ui->controls[i])) return focus_index(ui,i);
    return 0;
}
unsigned desk_ui_window_focus(struct desk_ui *ui,int active) {
    active=active!=0;if(active==ui->active) return 0;
    unsigned result=cancel(ui);ui->active=active;
    if(ui->focus>=0) result|=event(&ui->controls[ui->focus],(struct desk_widget_event){.type=active?DESK_WIDGET_FOCUS_IN:DESK_WIDGET_FOCUS_OUT},0);
    return result;
}
static unsigned step(struct desk_ui *ui,int reverse) {
    if(!ui->count) return focus_index(ui,-1);
    int index=ui->focus;
    if(index<0) index=reverse?0:-1;
    for(unsigned n=0;n<ui->count;n++) {
        index=(index+(reverse?(int)ui->count-1:1))%(int)ui->count;
        if(enabled(&ui->controls[index])) return focus_index(ui,index);
    }
    return focus_index(ui,-1);
}
unsigned desk_ui_layout_changed(struct desk_ui *ui) {
    unsigned result=cancel(ui);
    if(ui->focus<0 || !enabled(&ui->controls[ui->focus])) result|=step(ui,0);
    return result|DESK_WIDGET_REDRAW;
}
static int hit(const struct desk_ui *ui,int32_t x,int32_t y) {
    for(int i=(int)ui->count-1;i>=0;i--)
        if(enabled(&ui->controls[i]) && desk_widget_contains(frame(&ui->controls[i]),x,y)) return i;
    return -1;
}
struct desk_action desk_ui_pointer(struct desk_ui *ui,struct desk_widget_event e,uint64_t time_us) {
    struct desk_action result={0};
    if(e.type==DESK_WIDGET_CANCEL) { result.flags=cancel(ui);return result; }
    if(!ui->active || e.type!=DESK_WIDGET_POINTER) return result;
    if(e.pressed) {
        result.flags|=cancel(ui);
        int i=e.down?hit(ui,e.x,e.y):-1;
        result.flags|=focus_index(ui,i);ui->capture=i;
    }
    if(ui->capture>=0) {
        struct desk_control *c=&ui->controls[ui->capture];
        result.id=c->id;result.flags|=event(c,e,time_us);
    }
    if(!e.down) ui->capture=-1;
    return result;
}
struct desk_action desk_ui_key(struct desk_ui *ui,const struct srv_key_event *key) {
    struct desk_action result={0};
    if(key->flags&SRV_KEY_CANCEL) { result.flags=cancel(ui);return result; }
    if(!ui->active) return result;
    if((key->flags&SRV_KEY_DOWN_EVENT) && key->usage==SRV_KEY_TAB &&
       !(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META))) {
        result.flags=cancel(ui);
        result.flags|=step(ui,(key->modifiers&SRV_KEY_SHIFT)!=0);return result;
    }
    if(ui->focus<0 || !enabled(&ui->controls[ui->focus])) result.flags|=desk_ui_layout_changed(ui);
    if(ui->focus<0) return result;
    struct desk_control *c=&ui->controls[ui->focus];result.id=c->id;
    switch(c->kind) {
    case DESK_CONTROL_CHECK:
    case DESK_CONTROL_BUTTON:
        if((key->flags&SRV_KEY_DOWN_EVENT) && !(key->flags&SRV_KEY_REPEAT) &&
           (key->usage==SRV_KEY_ENTER || key->usage==SRV_KEY_SPACE) &&
           !(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META)))
            result.flags|=event(c,(struct desk_widget_event){.type=DESK_WIDGET_ACTIVATE},0);
        break;
    case DESK_CONTROL_EDIT: result.flags|=desk_edit_key(c->state,key);break;
    case DESK_CONTROL_LIST: result.flags|=desk_list_key(c->state,key);break;
    case DESK_CONTROL_TABLE: result.flags|=desk_table_key(c->state,key);break;
    case DESK_CONTROL_CALENDAR: result.flags|=desk_calendar_key(c->state,key);break;
    case DESK_CONTROL_TABS: result.flags|=desk_tabs_key(c->state,key);break;
    case DESK_CONTROL_IMAGE: result.flags|=desk_image_key(c->state,key);break;
    }
    return result;
}
struct desk_action desk_ui_scroll(struct desk_ui *ui,int32_t x,int32_t y,int32_t dx,int32_t dy) {
    struct desk_action result={0};int i=hit(ui,x,y);
    if(i<0 || ui->capture>=0) return result;
    struct desk_control *c=&ui->controls[i];result.id=c->id;
    if(c->kind==DESK_CONTROL_EDIT) result.flags=desk_edit_scroll(c->state,dx,dy);
    if(c->kind==DESK_CONTROL_LIST) result.flags=desk_list_scroll(c->state,dy);
    if(c->kind==DESK_CONTROL_TABLE) result.flags=desk_table_scroll(c->state,dy);
    if(c->kind==DESK_CONTROL_IMAGE) result.flags=desk_image_scroll(c->state,dx,dy);
    return result;
}
void desk_ui_draw(struct desk_canvas *canvas,const struct desk_ui *ui,const struct desk_theme *theme) {
    for(unsigned i=0;i<ui->count;i++) {
        const struct desk_control *c=&ui->controls[i];
        switch(c->kind) {
        case DESK_CONTROL_BUTTON: desk_button_draw(canvas,c->state,c->label,theme);break;
        case DESK_CONTROL_EDIT: desk_edit_draw(canvas,c->state,theme);break;
        case DESK_CONTROL_LIST: desk_list_draw(canvas,c->state,theme);break;
        case DESK_CONTROL_CHECK: desk_check_draw(canvas,c->state,c->label,theme);break;
        case DESK_CONTROL_TABLE: desk_table_draw(canvas,c->state,theme);break;
        case DESK_CONTROL_CALENDAR: desk_calendar_draw(canvas,c->state,theme);break;
    case DESK_CONTROL_TABS: desk_tabs_draw(canvas,c->state,theme);break;
    case DESK_CONTROL_IMAGE: desk_image_draw(canvas,c->state,theme);break;
        }
    }
}
