#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/desktools/calculator_app.c"
static struct tool t;
static int clipboard_fail;
long srv_ticks(void) { return 1; }
void srv_puts(const char *text) { (void)text; }
void tool_status(struct tool *tool,const char *text) { snprintf(tool->status,sizeof(tool->status),"%s",text);tool->window.dirty=1; }
int desk_clipboard_copy(struct desk_clipboard_client *clip,uint64_t server,const void *data,size_t length,uint64_t tick) {
    (void)server;(void)tick;if(clipboard_fail) return -1;memcpy(t.clipboard,data,length);t.clipboard[length]=0;clip->mode=1;return 0;
}
static void action(unsigned id) { tool_calculator_action(&t,(struct desk_action){id,DESK_WIDGET_ACTIVATED}); }
static void expression(const char *s) { assert(desk_text_buffer_set(&t.body_text,s,strlen(s))==1); }
static void paint(int w,int h) {
    t.window.width=w;t.window.height=h;tool_calculator_layout(&t);
    static uint32_t full[640*520],tiles[640*520];memset(full,0,sizeof(full));memset(tiles,0,sizeof(tiles));
    struct desk_canvas a,b;desk_canvas_init(&a,full,w,h,640);desk_canvas_init(&b,tiles,w,h,640);
    desk_ui_draw(&a,&t.ui,&desk_theme_quiet);tool_calculator_draw(&t,&a);
    for(int y=0;y<h;y+=31) for(int x=0;x<w;x+=29) { desk_clip(&b,(struct desk_rect){x,y,29,31});struct desk_rect clip=b.clip;desk_ui_draw(&b,&t.ui,&desk_theme_quiet);tool_calculator_draw(&t,&b);assert(!memcmp(&clip,&b.clip,sizeof(clip))); }
    assert(!memcmp(full,tiles,sizeof(full)));
    for(int y=0;y<520;y++) for(int x=0;x<640;x++) if(x>=w || y>=h) assert(!full[y*640+x]);
}
int main(void) {
    t.role=DESK_APP_CALCULATOR;t.window.width=640;t.window.height=520;
    desk_ui_init(&t.ui);assert(!desk_text_buffer_init(&t.body_text,t.body_store[0],t.body_store[1],CALC_EXPRESSION_BYTES));
    desk_edit_init(&t.body,&t.body_text,0);tool_calculator_init(&t);tool_calculator_layout(&t);desk_ui_focus(&t.ui,TOOL_BODY);
    assert(t.ui.count==26 && t.calc_history.body.enabled && t.calc_keys.h>0);
    expression("2 + 3 * 4");action(CALC_EVALUATE);assert(t.calc_valid && t.calc_just_evaluated && !strcmp(t.calc_result,"14") && t.calc_count==1);
    assert(t.body_text.anchor==0 && t.body_text.caret==t.body_text.length);
    action(CALC_KEY_FIRST+11);assert(!strcmp(t.body_text.data,"(14)*"));action(CALC_KEY_FIRST+13);action(CALC_EVALUATE);assert(!strcmp(t.calc_result,"28") && t.calc_count==2);
    action(CALC_EVALUATE);assert(t.calc_count==2);
    action(CALC_COPY);assert(!strcmp(t.clipboard,"28") && t.clip_action==DESK_EDIT_COPY && t.clip_target==CALC_COPY);
    t.clip.mode=0;action(CALC_USE);assert(!strcmp(t.body_text.data,"28") && !t.calc_just_evaluated);
    action(CALC_KEY_FIRST+18);assert(!strcmp(t.body_text.data,"-(28)"));action(CALC_EVALUATE);assert(!strcmp(t.calc_result,"-28"));
    action(CALC_KEY_FIRST+6);assert(!strcmp(t.body_text.data,"9"));action(CALC_EVALUATE);assert(!strcmp(t.calc_result,"9"));
    struct srv_key_event key={.flags=SRV_KEY_DOWN_EVENT,.codepoint='+',.usage=0x2e,.modifiers=SRV_KEY_SHIFT};assert(tool_calculator_key(&t,&key));assert(!strcmp(t.body_text.data,"(9)+"));
    action(CALC_KEY_FIRST+3);assert(!strcmp(t.body_text.data,"(9)+9"));action(CALC_EVALUATE);assert(!strcmp(t.calc_result,"18"));
    action(CALC_KEY_FIRST+2);assert(!t.body_text.length);
    expression("1/0");action(CALC_EVALUATE);assert(strstr(t.status,"zero") && !strcmp(t.calc_result,"18"));
    expression("1e99999999999");action(CALC_EVALUATE);assert(strstr(t.status,"range"));
    for(unsigned i=0;i<30;i++) { char text[32];snprintf(text,sizeof(text),"%u+1",i);expression(text);action(CALC_EVALUATE); }
    assert(t.calc_count==CALC_HISTORY_LIMIT && !strcmp(t.calc_expressions[0],"29+1") && !strcmp(t.calc_expressions[15],"14+1"));
    desk_list_select(&t.calc_history.body,5);action(CALC_HISTORY);assert(!strcmp(t.body_text.data,"24+1") && !strcmp(t.calc_result,"25"));
    char full[256];memset(full,'1',255);full[255]=0;expression(full);size_t length=t.body_text.length;action(CALC_KEY_FIRST+18);assert(t.body_text.length==length && !strcmp(t.body_text.data,full));
    clipboard_fail=1;action(CALC_COPY);assert(strstr(t.status,"unavailable"));clipboard_fail=0;
    paint(640,520);paint(280,240);assert(!t.calc_keys.h && !t.calc_history.body.enabled && t.buttons[0].enabled && t.buttons[1].enabled && !t.buttons[2].enabled && t.buttons[3].enabled);
    action(CALC_CLEAR);assert(!t.calc_valid && !t.body_text.length && t.calc_count==CALC_HISTORY_LIMIT && !t.buttons[3].enabled);
    puts("calculator-app: PASS keypad/expression/result chaining, history recall/eviction, clipboard snapshots, errors, limits, focus and compact/tiled drawing");
}
