#define main help_main
#include "../userspace/deskhelp/deskhelp.c"
#undef main
#include <assert.h>
#include <srvros/desktop_apps.h>
long srv_ticks(void) { return 1; }
void srv_puts(const char *text) { (void)text; }
static unsigned launched;
int desk_app_launch(struct desk_app *window,unsigned application,const char *path) { (void)window;assert(!*path);launched=application;return 0; }
static void press(unsigned usage,unsigned modifiers) { struct srv_key_event k={.usage=usage,.modifiers=modifiers,.flags=SRV_KEY_DOWN_EVENT};key(&k); }
static void choose(unsigned id) { chooser_action((struct desk_palette_result){.id=id,.flags=DESK_WIDGET_ACTIVATED}); }
int main(int argc,char **argv) {
    if(argc==3 && !strcmp(argv[1],"--dump")) {
        struct desk_document document;desk_document_init(&document);const struct help_topic *topic=help_topic_alias(argv[2]);
        assert(!desk_document_set(&document,topic->blocks,topic->count));char text[DESK_DOCUMENT_BYTES+DESK_DOCUMENT_BLOCKS*2];
        long length=desk_document_text(&document,text,sizeof(text));assert(length>=0);fwrite(text,1,length,stdout);return 0;
    }
    assert(help_topic_count<=DESK_PALETTE_ITEMS && help_topic_alias(NULL)->id==1 && help_topic_alias("unknown")->id==1);
    assert(desk_dock_count()==6 && !desk_application_info(DESK_APP_HELP)->dock);
    initialize();app.window.width=760;app.window.height=540;app.window.focused=1;app.window.server=42;
    for(unsigned i=0;i<help_topic_count;i++) {
        const struct help_topic *p=&help_topics[i];assert(help_topic(p->id)==p && help_topic_alias(p->alias)==p);
        assert(p->application<=DESK_APP_COUNT);assert(!desk_document_set(&app.document,p->blocks,p->count));
        for(unsigned j=0;j<i;j++) assert(p->id!=help_topics[j].id && strcmp(p->alias,help_topics[j].alias));
        char text[DESK_DOCUMENT_BYTES+DESK_DOCUMENT_BLOCKS*2];long length=desk_document_text(&app.document,text,sizeof(text));
        assert(length>0 && (size_t)length==strlen(text) && strstr(text,p->blocks[0].text));
    }
    visit(1);assert(app.count==1 && app.index==0 && !app.buttons[0].enabled && !app.buttons[1].enabled && !app.buttons[3].enabled);
    desk_document_scroll(&app.document,123);int previous=app.document.scroll.offset;assert(previous>0);
    visit(3);desk_document_scroll(&app.document,92);int next=app.document.scroll.offset;
    press(SRV_KEY_LEFT,SRV_KEY_ALT);assert(app.page->id==1 && app.document.scroll.offset==previous && app.buttons[1].enabled);
    press(SRV_KEY_RIGHT,SRV_KEY_ALT);assert(app.page->id==3 && app.document.scroll.offset==next);
    action((struct desk_action){HELP_OPEN,DESK_WIDGET_ACTIVATED});assert(launched==DESK_APP_FILES);
    history(-1);visit(4);assert(app.count==2 && app.index==1 && !app.buttons[1].enabled);
    visit(4);assert(app.count==2 && app.document.scroll.offset==0);
    for(unsigned i=0;i<50;i++) visit(i%help_topic_count+1);
    assert(app.count==HELP_HISTORY && app.index==HELP_HISTORY-1);
    for(unsigned i=0;i<20;i++) history(-1);
    assert(!app.index);
    for(unsigned i=0;i<20;i++) history(1);
    assert(app.index==HELP_HISTORY-1);
    unsigned current=app.page->id;visit(999);assert(app.page->id==current);
    press(15,SRV_KEY_CTRL);assert(app.chooser && !app.ui.active);
    assert(desk_text_buffer_set(&app.topics.text,"image zoom",10)>=0);desk_palette_filter(&app.topics);assert(app.topics.list.count==1 && app.topics.visible[0].id==7);
    choose(7);assert(!app.chooser && app.page->id==7 && app.document.focused);
    press(6,SRV_KEY_CTRL);assert(app.clipboard.mode==1 && app.clip_action==HELP_COPY_PAGE && !app.buttons[4].enabled);
    char copied[DESK_CLIPBOARD_BYTES+1];strcpy(copied,(char *)app.clipboard_data);assert(strstr(copied,"Preview\n\n") && strstr(copied,"4096"));
    visit(8);assert(!strcmp(copied,(char *)app.clipboard_data));app.clipboard.mode=0;clipboard_finish(1);assert(!strcmp(app.status,"Page copied.") && app.buttons[4].enabled);
    chooser_open();desk_text_buffer_set(&app.topics.text,"text",4);query_clipboard(DESK_EDIT_PASTE);assert(app.clip_action==DESK_EDIT_PASTE);
    strcpy((char *)app.clipboard_data,"preview");app.clipboard.length=7;app.clipboard.mode=0;
    desk_text_buffer_insert(&app.topics.text,"x",1);clipboard_finish(1);assert(!strstr(app.topics.text.data,"preview") && strstr(app.topics.notice,"canceled"));
    desk_text_buffer_set(&app.topics.text,"",0);query_clipboard(DESK_EDIT_PASTE);
    strcpy((char *)app.clipboard_data,"preview");app.clipboard.length=7;app.clipboard.mode=0;clipboard_finish(1);
    assert(!strcmp(app.topics.text.data,"preview") && app.topics.list.count==1);
    choose(7);chooser_open();query_clipboard(DESK_EDIT_PASTE);chooser_close();
    app.clipboard.mode=0;clipboard_finish(1);assert(!app.chooser && app.page->id==7 && app.buttons[4].enabled);
    static uint32_t full[980*660],tiled[980*660];
    const int sizes[][2]={{760,540},{280,240},{460,320},{960,640}};
    for(unsigned font=0;font<2;font++) {
        app.body_size=font?20:16;
        for(unsigned mode=0;mode<4;mode++) for(unsigned modal=0;modal<2;modal++) {
            app.window.width=sizes[mode][0];app.window.height=sizes[mode][1];visit(10);layout();
            if(modal) chooser_open();else chooser_close();
            for(unsigned i=0;i<5;i++) assert(app.buttons[i].frame.x>=12 && app.buttons[i].frame.x+app.buttons[i].frame.w<=sizes[mode][0]-12);
            memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));struct desk_canvas a,b;
            desk_canvas_init(&a,full,sizes[mode][0],sizes[mode][1],980);desk_canvas_init(&b,tiled,sizes[mode][0],sizes[mode][1],980);draw(&a);
            for(int y=0;y<sizes[mode][1];y+=71) for(int x=0;x<sizes[mode][0];x+=83) { desk_clip(&b,(struct desk_rect){x,y,83,71});draw(&b); }
            assert(!memcmp(full,tiled,sizeof(full)));
            for(unsigned y=0;y<660;y++) for(unsigned x=0;x<980;x++) if(x>=(unsigned)sizes[mode][0] || y>=(unsigned)sizes[mode][1]) assert(!full[y*980+x]);
        }
    }
    puts("help-app: PASS content validation, bounded history/branching/scroll restore, chooser, clipboard snapshots/stale reply, app routing, compact layouts and tiled rendering");
}
