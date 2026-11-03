#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/apps/desktools/settings.c"
static int brightness_send_fail;static uint64_t ticks=10;static struct desk_packet brightness_sent;
long srv_ticks(void) { return ticks; }
struct desk_packet desk_packet_new(uint32_t type) { return (struct desk_packet){.window=DESK_DEFAULT_WINDOW,.type=type}; }
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
    /* Large document text must not make a 28px toolbar label disappear. */
    struct desk_app themed={0};themed.preferences_valid=1;
    themed.preferences=desk_preferences_default();themed.preferences.text_size=20;themed.preferences.theme_accent=2;
    struct desk_theme appearance_theme=desk_app_theme(&themed);
    assert(appearance_theme.body==&desk_font_16 && appearance_theme.accent==0x16776e);
    uint32_t with_label[140*28]={0},without_label[140*28]={0};struct desk_canvas labeled,empty;
    assert(!desk_canvas_init(&labeled,with_label,140,28,140));assert(!desk_canvas_init(&empty,without_label,140,28,140));
    struct desk_button toolbar={0};desk_button_configure(&toolbar,(struct desk_rect){0,0,140,28},1);
    desk_button_draw(&labeled,&toolbar,"Text size",&appearance_theme);
    desk_button_draw(&empty,&toolbar,"",&appearance_theme);
    int ink=0;for(int i=0;i<140*28;i++) if(with_label[i]!=without_label[i]) ink++;
    assert(ink>20);
    t.window.width=560;t.window.height=480;t.body_size=16;
    desk_ui_init(&t.ui);assert(!desk_text_buffer_init(&t.name_text,t.name_store[0],t.name_store[1],128));
    desk_edit_init(&t.name,&t.name_text,0);tool_settings_init(&t);tool_layout(&t);
    assert(t.checks[0].checked && !t.checks[1].checked && t.checks[2].checked && t.checks[3].checked);
    assert(!strcmp(t.name_text.data,"+00:00") && t.settings_scroll.content==1880);
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
    tool_settings_scroll(&t,440);
    struct desk_rect date=t.checks[3].button.frame;
    struct desk_widget_event press={.type=DESK_WIDGET_POINTER,.x=date.x+30,.y=date.y+10,.down=1,.pressed=1};
    desk_ui_pointer(&t.ui,press,0);assert(t.checks[3].button.armed);
    tool_settings_receive(&t,&p);assert(t.checks[3].button.armed && t.ui.capture>=0);
    press.down=press.pressed=0;assert(desk_ui_pointer(&t.ui,press,0).flags&DESK_WIDGET_ACTIVATED);

    struct desk_preferences changed=t.preferences;changed.utc_offset_minutes=330;memcpy(p.text,&changed,sizeof(changed));
    tool_settings_receive(&t,&p);assert(!strcmp(t.name_text.data,"+05:30"));
    fail=1;tool_settings_action(&t,(struct desk_action){1,DESK_WIDGET_ACTIVATED});assert(sends==before && t.preferences.text_size==20);fail=0;
    tool_settings_action(&t,(struct desk_action){SETTINGS_ACCENT_TEAL,DESK_WIDGET_ACTIVATED});
    assert(mask==DESK_PREF_ACCENT && requested.theme_accent==2 && t.appearance_pending && t.preferences.theme_accent==0);
    struct desk_preferences appearance=t.preferences;appearance.theme_accent=2;memcpy(p.text,&appearance,sizeof(appearance));
    tool_settings_receive(&t,&p);assert(!t.appearance_pending && t.preferences.theme_accent==2 && t.checks[15].checked);
    tool_settings_action(&t,(struct desk_action){SETTINGS_GLASS_CLEAR,DESK_WIDGET_ACTIVATED});
    assert(t.appearance_pending && t.preferences.glass_level==1);
    ticks+=501;tool_settings_tick(&t);assert(!t.appearance_pending && strstr(t.status,"not confirmed"));
    tool_settings_action(&t,(struct desk_action){SETTINGS_STYLE_AERO,DESK_WIDGET_ACTIVATED});
    assert(mask==DESK_PREF_STYLE && requested.theme_style==DESK_THEME_AERO && t.appearance_pending && t.preferences.theme_style==DESK_THEME_SILVER);
    appearance=t.preferences;appearance.theme_style=DESK_THEME_AQUA;memcpy(p.text,&appearance,sizeof(appearance));
    tool_settings_receive(&t,&p);assert(t.appearance_pending && t.checks[21].checked);
    appearance.theme_style=DESK_THEME_AERO;memcpy(p.text,&appearance,sizeof(appearance));
    tool_settings_receive(&t,&p);assert(!t.appearance_pending && t.checks[20].checked);
    tool_settings_scroll(&t,68);assert(t.settings_scroll.offset==508 && t.checks[6].button.enabled);
    t.window.width=280;t.window.height=240;tool_layout(&t);assert(t.settings_scroll.content==1952);
    tool_settings_scroll(&t,-t.settings_scroll.offset);
    assert(desk_text_width(&desk_font_16,"Silver")<=(280-64)/3-22);
    assert(t.settings_scroll.offset==0 && t.checks[19].button.frame.y==156);
    assert(t.checks[19].button.enabled && t.checks[20].button.enabled && t.checks[21].button.enabled);
    static uint32_t top_pixels[280*240];struct desk_canvas top_canvas;
    assert(!desk_canvas_init(&top_canvas,top_pixels,280,240,280));tool_settings_draw(&t,&top_canvas);
    int preview_ink=0;for(int i=100*280;i<152*280;i++) if(top_pixels[i]) preview_ink++;
    assert(preview_ink>100);
    struct desk_rect aqua=t.checks[21].button.frame;
    struct desk_widget_event style_press={.type=DESK_WIDGET_POINTER,.x=aqua.x+10,.y=aqua.y+10,.down=1,.pressed=1};
    desk_ui_pointer(&t.ui,style_press,0);style_press.down=style_press.pressed=0;
    struct desk_action chosen=desk_ui_pointer(&t.ui,style_press,0);
    assert(chosen.id==SETTINGS_STYLE_AQUA && (chosen.flags&DESK_WIDGET_ACTIVATED));
    tool_settings_action(&t,chosen);assert(mask==DESK_PREF_STYLE && requested.theme_style==DESK_THEME_AQUA);
    desk_ui_focus(&t.ui,SETTINGS_STYLE_SILVER);
    struct srv_key_event space={.usage=SRV_KEY_SPACE,.flags=SRV_KEY_DOWN_EVENT};
    chosen=desk_ui_key(&t.ui,&space);assert(chosen.id==SETTINGS_STYLE_SILVER && (chosen.flags&DESK_WIDGET_ACTIVATED));
    tool_settings_action(&t,chosen);assert(requested.theme_style==DESK_THEME_SILVER);
    tool_settings_scroll(&t,508);
    tool_settings_scroll(&t,312);assert(t.checks[4].button.enabled && t.checks[5].button.enabled && t.checks[6].button.enabled);
    for(int i=4;i<7;i++) assert(t.checks[i].button.frame.x==24 && t.checks[i].button.frame.w==92);
    struct desk_rect thumb=desk_scrollbar_geometry(&t.settings_scroll).thumb;
    struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=thumb.x+2,.y=thumb.y+2,.down=1,.pressed=1};
    assert(tool_settings_pointer(&t,e) && t.settings_capture && t.settings_scroll.dragging);
    tool_settings_pointer(&t,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});assert(!t.settings_capture && !t.settings_scroll.dragging);
    static uint32_t pixels[280*240];struct desk_canvas c;assert(!desk_canvas_init(&c,pixels,280,240,280));
    struct desk_rect clip={20,70,180,90};desk_clip(&c,clip);tool_settings_draw(&t,&c);assert(!memcmp(&clip,&c.clip,sizeof(clip)));
    for(int y=0;y<240;y++) for(int x=0;x<280;x++) if(x<20 || x>=200 || y<70 || y>=160) assert(!pixels[y*280+x]);
    tool_settings_scroll(&t,972-t.settings_scroll.offset);assert(t.pointer_speed.enabled && t.checks[7].button.enabled);
    t.pointer_speed.value=5;t.pointer_speed.dragging=1;
    tool_settings_action(&t,(struct desk_action){SETTINGS_POINTER_SPEED,DESK_WIDGET_CHANGED});
    assert(requested.pointer_speed==5 && mask==DESK_PREF_POINTER_SPEED && t.pointer_speed.dragging);
    t.checks[7].checked=0;tool_settings_action(&t,(struct desk_action){SETTINGS_POINTER_ACCELERATION,DESK_WIDGET_ACTIVATED});
    assert(!requested.pointer_acceleration && mask==DESK_PREF_POINTER_ACCELERATION);
    assert(!t.preferences.disable_while_typing && !t.checks[8].checked);
    t.checks[8].checked=1;tool_settings_action(&t,(struct desk_action){SETTINGS_DISABLE_WHILE_TYPING,DESK_WIDGET_ACTIVATED});
    assert(requested.disable_while_typing==1 && mask==DESK_PREF_DISABLE_WHILE_TYPING);
    t.checks[8].checked=0;tool_settings_action(&t,(struct desk_action){SETTINGS_DISABLE_WHILE_TYPING,DESK_WIDGET_ACTIVATED});
    assert(!requested.disable_while_typing && !t.preferences.disable_while_typing);
    assert(t.preferences.tap_to_click && t.preferences.two_finger_tap);
    t.checks[9].checked=0;tool_settings_action(&t,(struct desk_action){SETTINGS_TAP_CLICK,DESK_WIDGET_ACTIVATED});
    assert(!requested.tap_to_click && requested.two_finger_tap && mask==DESK_PREF_TAP_CLICK);
    t.checks[10].checked=0;tool_settings_action(&t,(struct desk_action){SETTINGS_TWO_FINGER_TAP,DESK_WIDGET_ACTIVATED});
    assert(!requested.two_finger_tap && mask==DESK_PREF_TWO_FINGER_TAP);
    t.window.width=560;t.window.height=480;tool_layout(&t);
    t.window.server=42;t.display_serial=1;t.display_pending=1;t.display_operation=DESK_DISPLAY_QUERY;
    struct desk_display_state ds={.width=1920,.height=1080,.scale=1,.capabilities=DESK_DISPLAY_CAP_SCALE};
    struct desk_packet dp={.serial=1,.value=DESK_DISPLAY_OK};memcpy(dp.text,&ds,sizeof(ds));
    tool_settings_display(&t,&dp);assert(t.display_ready && t.display_width==1920 && t.display_scale==1);
    tool_settings_scroll(&t,1800-t.settings_scroll.offset);assert(!t.buttons[18].enabled && t.buttons[19].enabled);
    tool_settings_action(&t,(struct desk_action){SETTINGS_DISPLAY_SCALE,DESK_WIDGET_ACTIVATED});
    assert(t.display_scale==2 && t.display_selection_dirty && t.buttons[20].enabled);
    tool_settings_action(&t,(struct desk_action){SETTINGS_DISPLAY_APPLY,DESK_WIDGET_ACTIVATED});
    assert(brightness_sent.type==DESK_DISPLAY && brightness_sent.value==DESK_DISPLAY_APPLY && brightness_sent.x==2);
    dp.serial=brightness_sent.serial;dp.generation=7;ds.scale=2;ds.phase=DESK_DISPLAY_TRIAL;ds.remaining_seconds=15;memcpy(dp.text,&ds,sizeof(ds));
    tool_settings_display(&t,&dp);assert(t.display_token==7 && t.buttons[21].enabled);
    tool_settings_action(&t,(struct desk_action){SETTINGS_DISPLAY_REVERT,DESK_WIDGET_ACTIVATED});
    assert(brightness_sent.value==DESK_DISPLAY_REVERT && brightness_sent.generation==7);
    dp.serial=brightness_sent.serial;dp.generation=0;ds.scale=1;ds.phase=0;memcpy(dp.text,&ds,sizeof(ds));tool_settings_display(&t,&dp);
    assert(t.display.scale==1 && !t.display_selection_dirty);
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
    /* Audio replies are serial-checked and do not cancel an active slider drag. */
    t.audio_pending=0;t.audio_queued_op=0;t.brightness_pending=0;t.brightness_serial=99;
    t.brightness_refresh=0;t.audio_serial=0;t.window.width=560;t.window.height=600;
    tool_settings_tick(&t);assert(t.audio_pending && brightness_sent.type==DESK_AUDIO);
    struct desk_packet ap=brightness_sent;struct srv_audio_status audio={.version=SRV_AUDIO_VERSION,.state=SRV_AUDIO_READY,.volume=25};
    memcpy(ap.text,&audio,sizeof(audio));tool_settings_audio(&t,&ap);assert(!t.audio_pending && t.audio.volume==25);
    tool_settings_scroll(&t,1090-t.settings_scroll.offset);assert(t.audio_volume.enabled && !t.checks[12].button.enabled);
    t.audio_volume.value=60;t.audio_volume.dragging=1;
    tool_settings_action(&t,(struct desk_action){SETTINGS_AUDIO_VOLUME,DESK_WIDGET_CHANGED});tool_settings_tick(&t);
    assert(t.audio_pending && brightness_sent.value==SRV_AUDIO_VOLUME && brightness_sent.width==60);
    ap.serial--;tool_settings_audio(&t,&ap);assert(t.audio_pending);
    ap=brightness_sent;memcpy(ap.text,&audio,sizeof(audio));ap.value=0;tool_settings_audio(&t,&ap);
    assert(!t.audio_pending && t.audio_volume.dragging && t.audio_volume.value==60);
    t.audio_pending=1;tool_settings_action(&t,(struct desk_action){SETTINGS_AUDIO_STOP,DESK_WIDGET_ACTIVATED});
    assert(t.audio_queued_op==SRV_AUDIO_STOP);t.audio_pending=0;tool_settings_tick(&t);
    assert(brightness_sent.value==SRV_AUDIO_STOP);
    ap=brightness_sent;audio.capabilities=SRV_AUDIO_CAP_PCM_BUSY;memcpy(ap.text,&audio,sizeof(audio));
    tool_settings_audio(&t,&ap);tool_settings_layout(&t);
    assert(!t.audio_volume.enabled && !t.checks[11].button.enabled && !t.buttons[16].enabled && t.buttons[17].enabled);
    t.audio_pending=1;ap.serial=t.audio_serial;audio.capabilities=0;memcpy(ap.text,&audio,sizeof(audio));tool_settings_audio(&t,&ap);tool_settings_layout(&t);
    assert(t.audio_volume.enabled && t.buttons[16].enabled && t.buttons[17].enabled);
    puts("settings: PASS grouped choices, partial field requests, offset validation, draft retention, send failure, narrow scrolling and capture cancellation");
}
