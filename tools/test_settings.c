#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/desktools/settings.c"
static int brightness_send_fail;static uint64_t ticks=10;static struct desk_packet brightness_sent;
long srv_ticks(void) { return ticks; }
struct desk_packet desk_packet_new(uint32_t type) { return (struct desk_packet){.type=type}; }
int desk_send(uint64_t target,const struct desk_packet *p) { (void)target;brightness_sent=*p;return brightness_send_fail?-1:0; }
void srv_puts(const char *text) { (void)text; }
static unsigned mask,sends;static struct desk_preferences requested;static int fail;
int desk_app_preferences(struct desk_app *app,const struct desk_preferences *p,unsigned fields) {
    (void)app;if(fail) return -1;requested=*p;mask=fields;sends++;return 0;
}
void tool_status(struct tool *t,const char *text) { snprintf(t->status,sizeof(t->status),"%s",text);t->window.dirty=1; }
void tool_layout(struct tool *t) { tool_settings_layout(t);desk_ui_layout_changed(&t->ui);t->window.dirty=1; }
static struct tool t;
int main(void) {
    t.window.width=560;t.window.height=480;t.body_size=16;
    desk_ui_init(&t.ui);assert(!desk_text_buffer_init(&t.name_text,t.name_store[0],t.name_store[1],128));
    desk_edit_init(&t.name,&t.name_text,0);tool_settings_init(&t);tool_layout(&t);
    assert(t.checks[0].checked && !t.checks[1].checked && t.checks[2].checked && t.checks[3].checked);
    assert(!strcmp(t.name_text.data,"+00:00") && t.settings_scroll.content==798);
    tool_settings_action(&t,(struct desk_action){2,DESK_WIDGET_ACTIVATED});
    assert(sends==1 && mask==DESK_PREF_TEXT && requested.text_size==20 && t.body_size==20 && t.checks[1].checked && !t.checks[0].checked);
    t.checks[2].checked=0;tool_settings_action(&t,(struct desk_action){3,DESK_WIDGET_ACTIVATED});
    assert(mask==DESK_PREF_FORMAT && !(requested.clock_flags&DESK_CLOCK_24H));
    desk_text_buffer_set(&t.name_text,"-04:00",6);tool_settings_action(&t,(struct desk_action){TOOL_NAME,DESK_EDIT_SUBMIT});
    assert(mask==DESK_PREF_OFFSET && requested.utc_offset_minutes==-240);
    unsigned before=sends;desk_text_buffer_set(&t.name_text,"+14:15",6);tool_settings_action(&t,(struct desk_action){8,DESK_WIDGET_ACTIVATED});
    assert(sends==before && strstr(t.status,"15-minute"));
    desk_text_buffer_set(&t.name_text,"04:00",5);tool_settings_action(&t,(struct desk_action){8,DESK_WIDGET_ACTIVATED});assert(sends==before);
    struct desk_packet p={.value=DESK_PREF_SAVED};memcpy(p.text,&t.preferences,sizeof(t.preferences));
    tool_settings_receive(&t,&p);assert(!strcmp(t.name_text.data,"04:00")); /* Save acknowledgement preserves a draft. */
    struct desk_rect date=t.checks[3].button.frame;
    struct desk_widget_event press={.type=DESK_WIDGET_POINTER,.x=date.x+30,.y=date.y+10,.down=1,.pressed=1};
    desk_ui_pointer(&t.ui,press,0);assert(t.checks[3].button.armed);
    tool_settings_receive(&t,&p);assert(t.checks[3].button.armed && t.ui.capture>=0);
    press.down=press.pressed=0;assert(desk_ui_pointer(&t.ui,press,0).flags&DESK_WIDGET_ACTIVATED);

    struct desk_preferences changed=t.preferences;changed.utc_offset_minutes=330;memcpy(p.text,&changed,sizeof(changed));
    tool_settings_receive(&t,&p);assert(!strcmp(t.name_text.data,"+05:30"));
    fail=1;tool_settings_action(&t,(struct desk_action){1,DESK_WIDGET_ACTIVATED});assert(sends==before && t.preferences.text_size==20);fail=0;
    tool_settings_scroll(&t,68);assert(t.settings_scroll.offset==68 && t.checks[6].button.enabled);
    t.window.width=280;t.window.height=240;tool_layout(&t);assert(t.settings_scroll.content==870);
    tool_settings_scroll(&t,312);assert(t.checks[4].button.enabled && t.checks[5].button.enabled && t.checks[6].button.enabled);
    for(int i=4;i<7;i++) assert(t.checks[i].button.frame.x==24 && t.checks[i].button.frame.w==92);
    struct desk_rect thumb=desk_scrollbar_geometry(&t.settings_scroll).thumb;
    struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=thumb.x+2,.y=thumb.y+2,.down=1,.pressed=1};
    assert(tool_settings_pointer(&t,e) && t.settings_capture && t.settings_scroll.dragging);
    tool_settings_pointer(&t,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});assert(!t.settings_capture && !t.settings_scroll.dragging);
    static uint32_t pixels[280*240];struct desk_canvas c;assert(!desk_canvas_init(&c,pixels,280,240,280));
    struct desk_rect clip={20,70,180,90};desk_clip(&c,clip);tool_settings_draw(&t,&c);assert(!memcmp(&clip,&c.clip,sizeof(clip)));
    for(int y=0;y<240;y++) for(int x=0;x<280;x++) if(x<20 || x>=200 || y<70 || y>=160) assert(!pixels[y*280+x]);
    tool_settings_scroll(&t,532-t.settings_scroll.offset);assert(t.pointer_speed.enabled && t.checks[7].button.enabled);
    t.pointer_speed.value=5;t.pointer_speed.dragging=1;
    tool_settings_action(&t,(struct desk_action){SETTINGS_POINTER_SPEED,DESK_WIDGET_CHANGED});
    assert(requested.pointer_speed==5 && mask==DESK_PREF_POINTER_SPEED && t.pointer_speed.dragging);
    t.checks[7].checked=0;tool_settings_action(&t,(struct desk_action){SETTINGS_POINTER_ACCELERATION,DESK_WIDGET_ACTIVATED});
    assert(!requested.pointer_acceleration && mask==DESK_PREF_POINTER_ACCELERATION);
    t.window.server=42;tool_settings_tick(&t);
    assert(t.brightness_pending && brightness_sent.value==0);
    struct desk_packet bp=brightness_sent;bp.value=14;tool_settings_brightness(&t,&bp);
    assert(t.brightness_percent==14 && !t.brightness_pending);
    t.brightness.value=70;t.brightness.dragging=1;
    tool_settings_action(&t,(struct desk_action){SETTINGS_BRIGHTNESS,DESK_WIDGET_CHANGED});
    assert(brightness_sent.value==70 && t.brightness_pending);
    bp.value=10;tool_settings_brightness(&t,&bp);assert(t.brightness_percent==14); /* Stale response. */
    bp=brightness_sent;bp.value=70;tool_settings_brightness(&t,&bp);assert(t.brightness_percent==70 && t.brightness.dragging);
    t.brightness.value=20;tool_settings_action(&t,(struct desk_action){SETTINGS_BRIGHTNESS,DESK_WIDGET_CHANGED});
    ticks+=501;tool_settings_tick(&t);assert(t.brightness_percent==-1 && !t.brightness_pending);
    t.brightness_refresh=1;tool_settings_tick(&t);bp=brightness_sent;bp.value=40;tool_settings_brightness(&t,&bp);
    assert(t.brightness_percent==40);brightness_send_fail=1;t.brightness.value=50;
    tool_settings_action(&t,(struct desk_action){SETTINGS_BRIGHTNESS,DESK_WIDGET_CHANGED});
    assert(!t.brightness_pending && t.brightness_percent==-1);brightness_send_fail=0;
    puts("settings: PASS grouped choices, partial field requests, offset validation, draft retention, send failure, narrow scrolling and capture cancellation");
}
