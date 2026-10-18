#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_searchbar.h>
static struct srv_key_event key(unsigned usage,unsigned mods) { return (struct srv_key_event){.usage=usage,.modifiers=mods,.flags=SRV_KEY_DOWN_EVENT}; }
static unsigned lower(unsigned c) { return c>='A' && c<='Z'?c+32:c; }
static unsigned random_state=713;
static unsigned random_value(void) { random_state=random_state*1664525+1013904223;return random_state; }
int main(void) {
    struct desk_text_buffer text;char data[128],history[128];assert(!desk_text_buffer_init(&text,data,history,sizeof(data)));
    const char *document="one ONE café one";desk_text_buffer_set(&text,document,strlen(document));
    struct desk_text_match match={0};
    assert(desk_text_find(&text,"one",3,3,0,0,&match)==1 && match.start==4 && match.end==7 && !match.wrapped);
    assert(desk_text_find(&text,"one",3,4,1,0,&match)==1 && match.start==0 && !match.wrapped);
    assert(desk_text_find(&text,"one",3,0,1,0,&match)==1 && match.start==14 && match.wrapped);
    assert(desk_text_find(&text,"one",3,3,0,1,&match)==1 && match.start==14);
    assert(desk_text_find(&text,"é",2,0,0,1,&match)==1 && match.start==11 && match.end==13);
    assert(desk_text_find(&text,"É",2,0,0,0,&match)==0);
    assert(desk_text_find(&text,"",0,0,0,0,&match)==0);
    assert(desk_text_find(&text,"\xc0",1,0,0,0,&match)==-1);
    /* Compare circular direction/case rules with a separate small reference. */
    for(int trial=0;trial<20000;trial++) {
        char source[65],query[6];size_t n=random_value()%64,q=random_value()%5+1;
        for(size_t i=0;i<n;i++) { source[i]="abABxy"[random_value()%6]; }
        source[n]=0;
        for(size_t i=0;i<q;i++) { query[i]="abABxy"[random_value()%6]; }
        query[q]=0;
        desk_text_buffer_set(&text,source,n);size_t from=random_value()%(n+1);int back=random_value()%2,sensitive=random_value()%2;
        size_t expected=SIZE_MAX;int wrapped=0;
        for(int pass=0;pass<2 && expected==SIZE_MAX;pass++) {
            for(size_t step=0;step<=n;step++) {
                size_t at=back?n-step:step;if(at+q>n || ((back?at<from:at>=from)!=(pass==0))) continue;
                size_t i=0;while(i<q && (sensitive?(unsigned)source[at+i]:lower(source[at+i]))==(sensitive?(unsigned)query[i]:lower(query[i]))) i++;
                if(i==q) { expected=at;wrapped=pass;break; }
            }
        }
        int result=desk_text_find(&text,query,q,from,back,sensitive,&match);
        assert(result==(expected==SIZE_MAX?0:1));if(result) assert(match.start==expected && match.end==expected+q && match.wrapped==wrapped);
    }
    struct desk_searchbar bar;desk_searchbar_init(&bar);desk_searchbar_configure(&bar,(struct desk_rect){16,20,480,72},1);
    assert(!desk_searchbar_open(&bar,"needle") && bar.query.focused && bar.text.anchor==0 && bar.text.caret==6);
    struct srv_key_event enter=key(SRV_KEY_ENTER,0);struct desk_action action=desk_searchbar_key(&bar,&enter);assert(action.id==DESK_SEARCH_NEXT && (action.flags&DESK_WIDGET_ACTIVATED));
    enter.modifiers=SRV_KEY_SHIFT;assert(desk_searchbar_key(&bar,&enter).id==DESK_SEARCH_PREVIOUS);
    struct srv_key_event tab=key(SRV_KEY_TAB,0);desk_searchbar_key(&bar,&tab);desk_searchbar_key(&bar,&tab);desk_searchbar_key(&bar,&tab);
    struct srv_key_event space=key(SRV_KEY_SPACE,0);action=desk_searchbar_key(&bar,&space);assert(action.id==DESK_SEARCH_CASE && bar.match_case.checked);
    struct srv_key_event escape=key(SRV_KEY_ESCAPE,0);assert(desk_searchbar_key(&bar,&escape).id==DESK_SEARCH_CLOSE);
    desk_searchbar_focus(&bar,0);assert(!desk_searchbar_key(&bar,&escape).id);assert(desk_searchbar_open(&bar,"bad\nline")<0 && !strcmp(bar.text.data,"needle"));
    desk_searchbar_open(&bar,NULL);assert(bar.query.focused);
    static uint32_t pixels[640*240];struct desk_canvas canvas;desk_canvas_init(&canvas,pixels,640,240,640);
    struct desk_rect clip={30,30,200,60};desk_clip(&canvas,clip);desk_searchbar_draw(&canvas,&bar,&desk_theme_quiet);assert(!memcmp(&canvas.clip,&clip,sizeof(clip)));
    for(int y=0;y<240;y++) for(int x=0;x<640;x++) if(x<30 || x>=230 || y<30 || y>=90) assert(!pixels[y*640+x]);
    desk_searchbar_configure(&bar,(struct desk_rect){0,0,1,1},0);desk_searchbar_draw(&canvas,&bar,&desk_theme_quiet);
    desk_searchbar_replacing(&bar,1);desk_searchbar_configure(&bar,(struct desk_rect){8,8,248,144},1);desk_searchbar_open(&bar,"abc");
    assert(bar.query.frame.x==60 && bar.with.enabled && bar.replace.enabled && bar.replace_all.enabled);
    desk_searchbar_key(&bar,&tab);assert(bar.with.focused);
    enter.modifiers=0;action=desk_searchbar_key(&bar,&enter);assert(action.id==DESK_SEARCH_REPLACE && (action.flags&DESK_WIDGET_ACTIVATED));
    enter.modifiers=SRV_KEY_CTRL;assert(desk_searchbar_key(&bar,&enter).id==DESK_SEARCH_REPLACE_ALL);
    struct srv_key_event paste=key(25,SRV_KEY_CTRL);action=desk_searchbar_key(&bar,&paste);assert(action.id==DESK_SEARCH_REPLACEMENT_FIELD && (action.flags&DESK_EDIT_PASTE));
    static uint32_t full[280*180],tiled[284*180];
    desk_canvas_init(&canvas,full,280,180,280);desk_searchbar_draw(&canvas,&bar,&desk_theme_quiet);
    desk_canvas_init(&canvas,tiled,280,180,284);
    for(int y=0;y<180;y+=23) for(int x=0;x<280;x+=37) { desk_clip(&canvas,(struct desk_rect){x,y,37,23});desk_searchbar_draw(&canvas,&bar,&desk_theme_quiet); }
    for(int y=0;y<180;y++) { assert(!memcmp(full+y*280,tiled+y*284,280*4));for(int x=280;x<284;x++) assert(!tiled[y*284+x]); }
    desk_searchbar_replacing(&bar,0);desk_searchbar_configure(&bar,(struct desk_rect){8,8,248,72},1);
    assert(!bar.with.enabled && !bar.replace.enabled && !bar.replace_all.enabled && !bar.with.frame.w);
    puts("search: PASS 20,000 circular-search comparisons, UTF-8 boundaries/case, query validation, direction, focus, controls and clipped/tiny drawing");
}
