#include "notification_view.h"
#include <srvros/desktop_apps.h>
#include <stdio.h>
#include <string.h>
static const struct desktop_notice *selected(const struct desktop_notice_view *v) {
    return v->list.selected>=0 && v->list.selected<v->list.count?desktop_notification_find(&v->store,v->items[v->list.selected].id):NULL;
}
static void card(struct desk_notice *card,const struct desktop_notice *record) {
    const struct desk_application_info *info=desk_application_info(record->application);
    char heading[DESK_NOTICE_HEADING];snprintf(heading,sizeof(heading),"%s · %s",info?info->title:"Application",record->text.title);
    desk_notice_set(card,heading,record->text.message,(enum desk_notice_tone)record->tone);
}
void desktop_notice_view_init(struct desktop_notice_view *v) {
    *v=(struct desktop_notice_view){0};desk_ui_init(&v->ui);desk_ui_init(&v->toast_ui);desk_list_init(&v->list);desk_notice_init(&v->detail);desk_notice_init(&v->toast);
    desk_ui_add(&v->ui,NOTICE_LIST,DESK_CONTROL_LIST,&v->list,NULL);
    desk_ui_add(&v->ui,NOTICE_OPEN,DESK_CONTROL_BUTTON,&v->open,"Open app");desk_ui_add(&v->ui,NOTICE_REMOVE,DESK_CONTROL_BUTTON,&v->remove,"Clear");
    desk_ui_add(&v->ui,NOTICE_CLEAR,DESK_CONTROL_BUTTON,&v->clear,"Clear all");desk_ui_add(&v->ui,NOTICE_CLOSE,DESK_CONTROL_BUTTON,&v->close,"Close");
    desk_ui_add(&v->toast_ui,NOTICE_TOAST_OPEN,DESK_CONTROL_BUTTON,&v->toast_open,NULL);desk_ui_add(&v->toast_ui,NOTICE_TOAST_CLOSE,DESK_CONTROL_BUTTON,&v->toast_close,"×");
    desk_button_centered(&v->open,1);desk_button_centered(&v->remove,1);desk_button_centered(&v->clear,1);desk_button_centered(&v->close,1);desk_button_centered(&v->toast_close,1);
    desk_ui_window_focus(&v->ui,0);desk_ui_window_focus(&v->toast_ui,1);
}
void desktop_notice_view_refresh(struct desktop_notice_view *v) {
    uint64_t id=v->list.selected>=0 && v->list.selected<v->list.count?v->items[v->list.selected].id:0;int index=-1;
    for(unsigned i=0;i<v->store.count;i++) {
        const struct desktop_notice *record=&v->store.records[v->store.count-1-i];const struct desk_application_info *info=desk_application_info(record->application);
        v->items[i]=(struct desk_list_item){.id=record->id,.label=record->text.title,.detail=info?info->title:"Application"};if(record->id==id) index=i;
    }
    desk_list_set_items(&v->list,v->items,v->store.count);desk_list_select(&v->list,index>=0?index:v->store.count?0:-1);
    const struct desktop_notice *record=selected(v);
    if(record) card(&v->detail,record);else desk_notice_set(&v->detail,"No notifications","Completed timers and other application notices appear here.",DESK_NOTICE_INFO);
    desk_button_configure(&v->open,v->open.frame,record!=NULL);desk_button_configure(&v->remove,v->remove.frame,record!=NULL);desk_button_configure(&v->clear,v->clear.frame,v->store.count!=0);
    desk_ui_layout_changed(&v->ui);
}
void desktop_notice_view_configure(struct desktop_notice_view *v,int width,int height) {
    if(width<640 || height<480 || width>8192 || height>8192) return;
    int h=height-64;if(h>500) h=500;struct desk_rect r={width-572,52,560,h};v->frame=r;
    desk_list_configure(&v->list,(struct desk_rect){r.x+16,r.y+52,r.w-32,r.h-244},1);
    desk_notice_configure(&v->detail,(struct desk_rect){r.x+16,r.y+r.h-180,r.w-32,116},&desk_font_16);
    desk_button_configure(&v->open,(struct desk_rect){r.x+16,r.y+r.h-44,96,28},1);
    desk_button_configure(&v->remove,(struct desk_rect){r.x+120,r.y+r.h-44,80,28},1);
    desk_button_configure(&v->clear,(struct desk_rect){r.x+208,r.y+r.h-44,88,28},1);
    desk_button_configure(&v->close,(struct desk_rect){r.x+r.w-84,r.y+r.h-44,68,28},1);
    v->toast_frame=(struct desk_rect){width-376,56,360,144};desk_notice_configure(&v->toast,v->toast_frame,&desk_font_16);v->toast.heading_trailing=32;v->toast.message_bottom=26;
    desk_button_configure(&v->toast_open,v->toast_frame,1);desk_button_configure(&v->toast_close,(struct desk_rect){width-50,64,28,28},1);
    desk_ui_layout_changed(&v->toast_ui);desktop_notice_view_refresh(v);
}
void desktop_notice_view_open(struct desktop_notice_view *v) {
    v->opened=1;v->toast_id=v->pending_toast=0;desktop_notifications_read(&v->store);desktop_notice_view_refresh(v);
    desk_ui_window_focus(&v->toast_ui,0);desk_ui_window_focus(&v->ui,1);desk_ui_focus(&v->ui,NOTICE_LIST);
}
void desktop_notice_view_close(struct desktop_notice_view *v) {
    v->opened=0;desk_ui_window_focus(&v->ui,0);desk_ui_window_focus(&v->toast_ui,1);
}
static void show_toast(struct desktop_notice_view *v,uint64_t id,uint64_t now) {
    const struct desktop_notice *record=desktop_notification_find(&v->store,id);v->toast_id=record?id:0;v->toast_started=now;v->pending_toast=0;
    if(record) card(&v->toast,record);
    desk_ui_layout_changed(&v->toast_ui);desk_ui_focus(&v->toast_ui,0);
}
void desktop_notice_view_arrived(struct desktop_notice_view *v,uint64_t id,uint64_t now) {
    if(v->opened) { desktop_notifications_read(&v->store);desktop_notice_view_refresh(v);return; }
    if(v->toast_ui.capture>=0) v->pending_toast=id;else show_toast(v,id,now);
}
int desktop_notice_view_tick(struct desktop_notice_view *v,uint64_t now) {
    if(v->opened || v->toast_ui.capture>=0) return 0;
    if(v->pending_toast) { show_toast(v,v->pending_toast,now);return 1; }
    if(v->toast_id && (!desktop_notification_find(&v->store,v->toast_id) || (now>=v->toast_started && now-v->toast_started>=8000000))) {
        v->toast_id=0;return 1;
    }
    return 0;
}
struct desktop_notice_action desktop_notice_view_action(struct desktop_notice_view *v,struct desk_action a) {
    struct desktop_notice_action result={0};const struct desktop_notice *record=selected(v);
    if(a.id==NOTICE_LIST && (a.flags&DESK_WIDGET_CHANGED)) { if(record) card(&v->detail,record); }
    if(!(a.flags&DESK_WIDGET_ACTIVATED)) return result;
    if((a.id==NOTICE_OPEN || a.id==NOTICE_LIST) && record) { result.owner=record->owner;result.application=record->application;result.close=1; }
    else if(a.id==NOTICE_REMOVE && record) { desktop_notification_remove(&v->store,record->id);desktop_notice_view_refresh(v); }
    else if(a.id==NOTICE_CLEAR) { v->store.count=0;memset(v->store.records,0,sizeof(v->store.records));desktop_notice_view_refresh(v); }
    else if(a.id==NOTICE_CLOSE) result.close=1;
    return result;
}
struct desktop_notice_action desktop_notice_toast_action(struct desktop_notice_view *v,struct desk_action a) {
    struct desktop_notice_action result={0};if(!(a.flags&DESK_WIDGET_ACTIVATED)) return result;
    const struct desktop_notice *record=desktop_notification_find(&v->store,v->toast_id);
    if(a.id==NOTICE_TOAST_OPEN && record) { result.owner=record->owner;result.application=record->application; }
    if(a.id==NOTICE_TOAST_OPEN || a.id==NOTICE_TOAST_CLOSE) {
        for(unsigned i=0;i<v->store.count;i++) if(v->store.records[i].id==v->toast_id) v->store.records[i].unread=0;
        v->toast_id=0;
    }
    return result;
}
void desktop_notice_view_draw(struct desk_canvas *c,const struct desktop_notice_view *v,const struct desk_theme *theme) {
    struct desk_rect old=c->clip;
    if(v->opened) {
        struct desk_rect r=v->frame;desk_fill(c,(struct desk_rect){r.x+4,r.y+5,r.w,r.h},0xb9c2b4);desk_fill(c,r,theme->border);desk_fill(c,desk_widget_inset(r,1),theme->surface);
        desk_clip(c,desk_intersect(old,r));desk_label_draw(c,(struct desk_rect){r.x+16,r.y+12,r.w-32,28},theme->heading,"Notifications",theme->text);
        desk_ui_draw(c,&v->ui,theme);desk_notice_draw(c,&v->detail,theme);
    } else if(v->toast_id && desktop_notification_find(&v->store,v->toast_id)) {
        struct desk_rect r=v->toast_frame;desk_fill(c,(struct desk_rect){r.x+4,r.y+5,r.w,r.h},0xb9c2b4);desk_notice_draw(c,&v->toast,theme);
        desk_button_draw(c,&v->toast_close,"×",theme);
        desk_label_draw(c,(struct desk_rect){r.x+14,r.y+r.h-26,r.w-28,20},theme->body,"Click to open · Kept in Notifications",theme->muted);
    }
    c->clip=old;
}
