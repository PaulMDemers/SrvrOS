#include <srvros/desk_properties.h>
#include <srvros/desk_ui.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
int main(void) {
    struct desk_properties p;desk_properties_init(&p);
    char name[48]="Name",value[384]="Résumé — example.txt";
    struct desk_property fields[]={{1,name,value},{2,"Size","18446744073709551615 bytes"},{3,"Path","/fat/home/a long path with words/and-a-very-long-filename-that-wraps-in-compact-views.txt"},{4,"Modified UTC","2026-10-18 12:34:56"},{5,"Multiline","first line\nsecond\tpart"},{6,"Empty",""}};
    assert(!desk_properties_set(&p,fields,6));name[0]='x';value[0]='x';assert(!strcmp(p.fields[0].label,"Name") && !strcmp(p.fields[0].value,"Résumé — example.txt"));
    char output[4096];long length=desk_properties_text(&p,NULL,0);memset(output,'X',sizeof(output));assert(desk_properties_text(&p,output,length)==-1 && output[0]=='X');
    assert(desk_properties_text(&p,output,sizeof(output))==length && strstr(output,"Empty: \n") && strstr(output,"Size: 18446744073709551615 bytes\n"));
    struct desk_properties before=p;struct desk_property invalid[]={{1,"One","a"},{1,"Two","b"}};
    assert(desk_properties_set(&p,invalid,2)<0 && !memcmp(&before,&p,sizeof(p)));
    invalid[1].id=2;invalid[0].value="bad\xff";assert(desk_properties_set(&p,invalid,2)<0);
    invalid[0].value="bad\r";assert(desk_properties_set(&p,invalid,2)<0);
    assert(desk_properties_set(&p,fields,17)<0);
    struct desk_property borrowed[]={{7,p.fields[0].value,p.fields[0].label},{8,p.fields[2].label,p.fields[2].value}};
    assert(!desk_properties_set(&p,borrowed,2) && !strcmp(p.fields[0].value,"Name"));
    name[0]='N';strcpy(value,"Résumé — example.txt");assert(!desk_properties_set(&p,fields,6));
    desk_properties_configure(&p,(struct desk_rect){10,20,280,200},&desk_font_20,1);assert(p.stacked && p.scroll.content>p.scroll.viewport);
    desk_properties_event(&p,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});struct srv_key_event key={.usage=SRV_KEY_END,.flags=SRV_KEY_DOWN_EVENT};assert(desk_properties_key(&p,&key)&DESK_WIDGET_REDRAW);assert(p.scroll.offset==desk_scrollbar_limit(&p.scroll));
    key.usage=SRV_KEY_HOME;desk_properties_key(&p,&key);assert(!p.scroll.offset);
    struct desk_scrollbar_geometry geometry=desk_scrollbar_geometry(&p.scroll);
    desk_properties_event(&p,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=geometry.thumb.x+1,.y=geometry.thumb.y+1,.down=1,.pressed=1});assert(p.scroll.dragging);
    desk_properties_configure(&p,(struct desk_rect){10,20,560,300},&desk_font_16,1);assert(!p.stacked && !p.scroll.dragging);
    struct desk_ui ui;desk_ui_init(&ui);assert(!desk_ui_add(&ui,1,DESK_CONTROL_PROPERTIES,&p,NULL));desk_ui_focus(&ui,1);assert(p.focused);
    static uint32_t full[650*480],tiled[650*480];
    for(unsigned f=0;f<2;f++) for(int width=40;width<=600;width+=70) for(unsigned scroll=0;scroll<2;scroll++) {
        desk_properties_configure(&p,(struct desk_rect){-5,10,width,300},f?&desk_font_20:&desk_font_16,1);desk_properties_scroll(&p,scroll?INT_MAX:INT_MIN);
        memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));struct desk_canvas a,b;desk_canvas_init(&a,full,620,450,650);desk_canvas_init(&b,tiled,620,450,650);desk_properties_draw(&a,&p,&desk_theme_quiet);
        for(int y=0;y<450;y+=47) for(int x=0;x<620;x+=53) { desk_clip(&b,(struct desk_rect){x,y,53,47});desk_properties_draw(&b,&p,&desk_theme_quiet); }
        assert(!memcmp(full,tiled,sizeof(full)));
        for(unsigned y=0;y<480;y++) for(unsigned x=0;x<650;x++) if(x>=620 || y>=450) assert(!full[y*650+x]);
    }
    char maximum[DESK_PROPERTY_VALUE];memset(maximum,'W',sizeof(maximum)-1);maximum[sizeof(maximum)-1]=0;struct desk_property many[16];
    for(unsigned i=0;i<16;i++) many[i]=(struct desk_property){i+1,"Longest",maximum};
    assert(!desk_properties_set(&p,many,16));desk_properties_configure(&p,(struct desk_rect){0,0,64,32},&desk_font_20,1);assert(p.scroll.content>10000);
    before=p;maximum[sizeof(maximum)-1]='W';assert(desk_properties_set(&p,many,16)<0 && !memcmp(&before,&p,sizeof(p)));
    assert(!desk_properties_set(&p,NULL,0));assert(desk_properties_text(&p,output,sizeof(output))==0 && !output[0]);
    puts("properties: PASS owned/borrowed snapshots, atomic limits/UTF-8/control validation, exact export, responsive columns/wrapping, scrollbar/keys/capture, UI routing and tiled/stride rendering");
}
