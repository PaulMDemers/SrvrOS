#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_palette.h>
static struct srv_key_event key(unsigned usage,unsigned codepoint) { return (struct srv_key_event){.usage=usage,.codepoint=codepoint,.flags=SRV_KEY_DOWN_EVENT}; }
int main(void) {
    struct desk_palette p;desk_palette_init(&p);
    struct desk_palette_item items[]={{11,"Terminal","New application"},{5000000024ULL,"Text — Notes","Open window"},{17,"Files","New application"},{19,"System","New application"}};
    assert(!desk_palette_open(&p,"Open",items,4));desk_palette_configure(&p,(struct desk_rect){0,44,1280,680});
    assert(p.query.focused && p.list.count==4 && p.list.selected==0);
    items[0].label[0]='X';assert(!strcmp(p.visible[0].label,"Terminal"));
    struct srv_key_event down=key(SRV_KEY_DOWN,0);desk_palette_key(&p,&down);assert(p.list.selected==1 && p.query.focused);
    struct srv_key_event enter=key(SRV_KEY_ENTER,0);struct desk_palette_result r=desk_palette_key(&p,&enter);assert(r.id==5000000024ULL && (r.flags&DESK_WIDGET_ACTIVATED));
    struct srv_key_event letter=key(0x17,'t');r=desk_palette_key(&p,&letter);assert(p.list.count==4 && (r.flags&DESK_WIDGET_CHANGED));
    desk_text_buffer_set(&p.text,"new term",8);desk_palette_filter(&p);assert(p.list.count==1 && p.visible[0].id==11);
    desk_text_buffer_set(&p.text,"  WINDOW  notes ",16);desk_palette_filter(&p);assert(p.list.count==1 && p.visible[0].id==5000000024ULL);
    desk_text_buffer_set(&p.text,"missing",7);desk_palette_filter(&p);assert(!p.list.count && !desk_palette_key(&p,&enter).id);
    desk_text_buffer_set(&p.text,"",0);desk_palette_filter(&p);desk_list_select(&p.list,2);
    struct desk_palette_item replacement[]={{19,"System","App"},{17,"Files","App"}};
    assert(!desk_palette_items(&p,replacement,2) && p.list.selected==1 && p.visible[1].id==17);
    replacement[1].id=19;assert(desk_palette_items(&p,replacement,2)<0 && p.list.count==2);
    struct srv_key_event escape=key(SRV_KEY_ESCAPE,0);assert(desk_palette_key(&p,&escape).cancel);
    desk_palette_focus(&p,0);assert(!desk_palette_key(&p,&escape).cancel);desk_palette_focus(&p,1);
    r=desk_palette_pointer(&p,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=0,.y=0,.down=1,.pressed=1},1);assert(r.cancel);
    struct desk_rect row=p.list.frame;struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=row.x+40,.y=row.y+12,.down=1,.pressed=1};
    desk_palette_pointer(&p,e,100000);e.down=e.pressed=0;desk_palette_pointer(&p,e,110000);e.down=e.pressed=1;desk_palette_pointer(&p,e,200000);e.down=e.pressed=0;
    r=desk_palette_pointer(&p,e,210000);assert(r.id==19 && (r.flags&DESK_WIDGET_ACTIVATED));
    static uint32_t pixels[640*480];struct desk_canvas c;assert(!desk_canvas_init(&c,pixels,640,480,640));
    desk_palette_configure(&p,(struct desk_rect){0,0,640,480});struct desk_rect clip={30,50,550,350};desk_clip(&c,clip);desk_palette_draw(&c,&p,&desk_theme_quiet);
    assert(!memcmp(&clip,&c.clip,sizeof(clip)));
    for(int y=0;y<480;y++) for(int x=0;x<640;x++) if(x<30 || x>=580 || y<50 || y>=400) assert(!pixels[y*640+x]);
    desk_palette_configure(&p,(struct desk_rect){0,0,1,1});desk_palette_draw(&c,&p,&desk_theme_quiet);
    static struct desk_palette_item large[200],storage[256],more[512];
    static struct desk_list_item visible[256],more_visible[512];
    for(unsigned i=0;i<200;i++) { large[i].id=i+1000;snprintf(large[i].label,sizeof(large[i].label),"Document %03u",i); }
    assert(desk_palette_items(&p,large,200)==-1 && p.count==2);
    assert(!desk_palette_bind_storage(&p,storage,visible,256));
    assert(!desk_palette_open(&p,"All windows",large,200) && p.list.count==200);
    desk_text_buffer_set(&p.text,"Document 199",12);desk_palette_filter(&p);
    assert(p.list.count==1 && desk_palette_key(&p,&enter).id==1199);
    p.list.pressed=0;p.list.scrollbar_capture=1;
    assert(!desk_palette_bind_storage(&p,more,more_visible,512));
    assert(p.list.count==1 && p.list.selected==0 && p.visible[0].id==1199 && !p.list.scrollbar_capture && p.list.pressed==-1);
    assert(!strcmp(p.text.data,"Document 199") && p.items==more && p.visible==more_visible);
    assert(desk_palette_bind_storage(&p,storage,visible,100)==-1 && p.items==more);
    assert(desk_palette_bind_storage(&p,storage,visible,DESK_LIST_MAX_ITEMS+1)==-1);
    large[199].id=large[198].id;
    assert(desk_palette_items(&p,large,200)==-1 && p.items[199].id==1199);
    puts("palette-storage: PASS 200 searchable identities, borrowed storage growth, query/selection preservation and invalid replacement");
    puts("palette: PASS owned items, token search, keyboard selection/activation, empty results, stable replacement, validation, cancel, double click and clipping");
}
