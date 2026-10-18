#include "app.h"
#include <stdio.h>
#include <string.h>
static struct desk_rect viewport(struct tool *t) { return (struct desk_rect){16,64,t->window.width-32,t->window.height-108}; }
static int visible(struct tool *t,int y,int h) {
    struct desk_rect r=viewport(t);return y>=r.y && y+h<=r.y+r.h;
}
static void offset_text(struct tool *t) {
    int value=t->preferences.utc_offset_minutes,abs=value<0?-value:value;char text[16];
    snprintf(text,sizeof(text),"%c%02d:%02d",value<0?'-':'+',abs/60,abs%60);
    desk_text_buffer_set(&t->name_text,text,strlen(text));
}
void tool_settings_init(struct tool *t) {
    t->preferences=desk_preferences_default();t->preferences_status=DESK_PREF_LOADING;offset_text(t);
    static const char *labels[]={"Standard","Larger","24-hour clock","Show date","Sage","Sand","Slate"};
    for(unsigned i=0;i<7;i++) desk_ui_add(&t->ui,i+1,DESK_CONTROL_CHECK,&t->checks[i],labels[i]);
    desk_ui_add(&t->ui,TOOL_NAME,DESK_CONTROL_EDIT,&t->name,NULL);
    desk_ui_add(&t->ui,8,DESK_CONTROL_BUTTON,&t->buttons[0],"Apply");
    desk_ui_add(&t->ui,9,DESK_CONTROL_BUTTON,&t->buttons[1],"UTC");
    tool_status(t,"Loading saved preferences…");
}
void tool_settings_layout(struct tool *t) {
    struct desk_rect view=viewport(t);
    desk_scrollbar_configure(&t->settings_scroll,(struct desk_rect){t->window.width-28,view.y,12,view.h},t->window.width>=372?440:512,view.h);
    int y=64-t->settings_scroll.offset,w=t->window.width;
    desk_check_configure(&t->checks[0],(struct desk_rect){24,y+52,112,32},visible(t,y+52,32),t->preferences.text_size==16,1);
    desk_check_configure(&t->checks[1],(struct desk_rect){148,y+52,96,32},visible(t,y+52,32),t->preferences.text_size==20,1);
    desk_check_configure(&t->checks[2],(struct desk_rect){24,y+172,w-64,30},visible(t,y+172,30),(t->preferences.clock_flags&DESK_CLOCK_24H)!=0,0);
    desk_check_configure(&t->checks[3],(struct desk_rect){24,y+208,w-64,30},visible(t,y+208,30),(t->preferences.clock_flags&DESK_CLOCK_DATE)!=0,0);
    desk_edit_configure(&t->name,(struct desk_rect){24,y+284,112,34},&desk_font_16,visible(t,y+284,34));
    desk_button_configure(&t->buttons[0],(struct desk_rect){148,y+285,w>=372?112:w-184,32},visible(t,y+285,32));
    desk_button_configure(&t->buttons[1],(struct desk_rect){272,y+285,64,32},visible(t,y+285,32) && w>=372);
    for(int i=0;i<3;i++) {
        int row=y+394+(w>=372?0:i*36);
        desk_check_configure(&t->checks[i+4],(struct desk_rect){24+(w>=372?i*100:0),row,92,32},
            visible(t,row,32),t->preferences.wallpaper==(unsigned)i,1);
    }
}
void tool_settings_draw(struct tool *t,struct desk_canvas *c) {
    const struct desk_theme *theme=&desk_theme_quiet;struct desk_rect old=c->clip,view=viewport(t);
    desk_text(c,theme->heading,24,20,"Desktop settings",theme->text);
    desk_clip(c,desk_intersect(old,(struct desk_rect){view.x,view.y,view.w-16,view.h}));
    int y=64-t->settings_scroll.offset;
    desk_label_draw(c,(struct desk_rect){24,y+4,c->width-64,28},theme->body,"Document text size",theme->text);
    desk_label_draw(c,(struct desk_rect){24,y+98,c->width-64,32},t->body_size==20?theme->heading:theme->body,
        "The quick brown fox jumps over the lazy dog.",theme->text);
    desk_label_draw(c,(struct desk_rect){24,y+140,c->width-64,28},theme->body,"Menu-bar clock",theme->text);
    desk_label_draw(c,(struct desk_rect){24,y+250,c->width-64,28},theme->body,"Fixed UTC offset (±HH:MM)",theme->text);
    desk_label_draw(c,(struct desk_rect){24,y+326,c->width-64,28},theme->body,"Daylight-saving changes are manual.",theme->muted);
    desk_label_draw(c,(struct desk_rect){24,y+360,c->width-64,28},theme->body,"Desktop color",theme->text);
    desk_ui_draw(c,&t->ui,theme);c->clip=old;
    if(desk_scrollbar_limit(&t->settings_scroll)>0) desk_scrollbar_draw(c,&t->settings_scroll,theme);
}
static void request(struct tool *t,const struct desk_preferences *value,unsigned mask) {
    if(desk_app_preferences(&t->window,value,mask)) {
        tool_status(t,"The setting could not be applied. Try again.");tool_layout(t);return;
    }
    desk_preferences_merge(&t->preferences,value,mask);t->body_size=t->preferences.text_size;
    tool_layout(t);tool_status(t,"Applying preference…");
}
void tool_settings_action(struct tool *t,struct desk_action a) {
    int activated=(a.flags&DESK_WIDGET_ACTIVATED)!=0;
    struct desk_preferences value=t->preferences;
    if(activated && (a.id==1 || a.id==2)) { value.text_size=a.id==1?16:20;request(t,&value,DESK_PREF_TEXT); }
    else if(activated && (a.id==3 || a.id==4)) {
        unsigned bit=a.id==3?DESK_CLOCK_24H:DESK_CLOCK_DATE;
        value.clock_flags=(value.clock_flags&~bit)|(t->checks[a.id-1].checked?bit:0);
        request(t,&value,a.id==3?DESK_PREF_FORMAT:DESK_PREF_DATE);
    } else if(activated && a.id>=5 && a.id<=7) { value.wallpaper=a.id-5;request(t,&value,DESK_PREF_WALLPAPER); }
    else if((activated && a.id==8) || (a.id==TOOL_NAME && (a.flags&DESK_EDIT_SUBMIT))) {
        const char *text=t->name_text.data;
        if(t->name_text.length!=6 || (text[0]!='+' && text[0]!='-') || text[3]!=':' ||
            text[1]<'0' || text[1]>'9' || text[2]<'0' || text[2]>'9' ||
            text[4]<'0' || text[4]>'5' || text[5]<'0' || text[5]>'9') {
            tool_status(t,"Use a signed offset such as -04:00 or +05:30.");return;
        }
        int minutes=((text[1]-'0')*10+text[2]-'0')*60+(text[4]-'0')*10+text[5]-'0';
        value.utc_offset_minutes=text[0]=='-'?-minutes:minutes;
        if(!desk_preferences_valid(&value)) { tool_status(t,"Offset must be -12:00 to +14:00, in 15-minute steps.");return; }
        request(t,&value,DESK_PREF_OFFSET);
    } else if(activated && a.id==9) { value.utc_offset_minutes=0;request(t,&value,DESK_PREF_OFFSET);offset_text(t); }
}
void tool_settings_receive(struct tool *t,const struct desk_packet *packet) {
    struct desk_preferences value;memcpy(&value,packet->text,sizeof(value));
    if(!desk_preferences_valid(&value) || packet->value>DESK_PREF_CONFLICT) return;
    int changed=memcmp(&value,&t->preferences,sizeof(value))!=0;
    int offset_changed=value.utc_offset_minutes!=t->preferences.utc_offset_minutes;
    t->preferences=value;t->body_size=value.text_size;t->preferences_status=packet->value;
    if(offset_changed) offset_text(t);
    static const char *status[]={"Loading saved preferences…","Preferences ready · changes are saved automatically.",
        "Applied · saving preferences…","Applied for this session · preferences could not be saved.",
        "Applied for this session · the settings file changed elsewhere."};
    tool_status(t,status[packet->value]);
    /* A save acknowledgement changes status, not geometry or interaction.
     * Relayout here would disarm a click held while the worker finishes. */
    if(changed) tool_layout(t);
}
int tool_settings_pointer(struct tool *t,struct desk_widget_event e) {
    if(e.type==DESK_WIDGET_CANCEL) {
        t->settings_capture=0;desk_scrollbar_event(&t->settings_scroll,e);return 0;
    }
    if(e.pressed && e.down && desk_widget_contains(t->settings_scroll.track,e.x,e.y)) {
        t->settings_capture=1;desk_ui_pointer(&t->ui,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL},0);
    }
    if(!t->settings_capture) return 0;
    unsigned flags=desk_scrollbar_event(&t->settings_scroll,e);
    if(flags&DESK_WIDGET_CHANGED) { tool_settings_layout(t);desk_ui_layout_changed(&t->ui); }
    if(flags&DESK_WIDGET_REDRAW) t->window.dirty=1;
    if(!e.down) t->settings_capture=0;
    return 1;
}
void tool_settings_scroll(struct tool *t,int delta) {
    if(t->settings_capture) return;
    if(desk_scrollbar_scroll(&t->settings_scroll,delta)&DESK_WIDGET_CHANGED) {
        tool_settings_layout(t);desk_ui_layout_changed(&t->ui);t->window.dirty=1;
    }
}
