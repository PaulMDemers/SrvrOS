#include <srvros/desk_dialog.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct desk_dialog d;
static struct desk_action key(unsigned usage) {
    struct srv_key_event k={.usage=usage,.flags=SRV_KEY_DOWN_EVENT};return desk_dialog_key(&d,&k);
}
#define W 400
#define H 300
#define S 407
static uint32_t pixels[S*H+16],tiled[S*H+16];
static void held_click_layout(void) {
    struct desk_dialog model;desk_dialog_init(&model);
    const struct desk_dialog_choice choices[]={{1,"Save",1},{2,"Discard",1},{3,"Cancel",1}};
    assert(!desk_dialog_open(&model,"Unsaved","Keep or discard?",choices,3,3,3));
    struct desk_rect bounds={0,0,W,H};desk_dialog_configure(&model,bounds);
    for(unsigned i=0;i<3;i++) {
        struct desk_rect b=model.buttons[i].frame;
        struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=b.x+10,.y=b.y+10,.down=1,.pressed=1};
        desk_dialog_pointer(&model,e,1);
        for(unsigned tick=0;tick<8;tick++)desk_dialog_configure(&model,bounds);
        e.down=e.pressed=0;struct desk_action a=desk_dialog_pointer(&model,e,20);
        assert(a.id==i+1 && (a.flags&DESK_WIDGET_ACTIVATED));
        assert(!(desk_dialog_pointer(&model,e,21).flags&DESK_WIDGET_ACTIVATED));
    }
    struct desk_rect b=model.buttons[1].frame;
    struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=b.x+10,.y=b.y+10,.down=1,.pressed=1};
    desk_dialog_pointer(&model,e,30);model.choices[1].enabled=0;
    desk_dialog_configure(&model,bounds);e.down=e.pressed=0;
    assert(!(desk_dialog_pointer(&model,e,40).flags&DESK_WIDGET_ACTIVATED));
}
int main(void) {
    held_click_layout();
    desk_dialog_init(&d);
    struct desk_dialog_choice choices[]={ {1,"Save",1},{2,"Discard",1},{3,"Cancel",1} };
    assert(!desk_dialog_open(&d,"Save changes?","This document has unsaved changes. Cancel returns to your document.",choices,3,3,3));
    desk_dialog_configure(&d,(struct desk_rect){0,0,W,H});
    assert(key(SRV_KEY_ENTER).id==3 && key(SRV_KEY_ESCAPE).id==3);
    key(SRV_KEY_TAB);assert(key(SRV_KEY_ENTER).id==1);
    struct desk_action a=desk_dialog_pointer(&d,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=0,.y=0,.down=1,.pressed=1},1);
    assert(!a.id && key(SRV_KEY_ENTER).id==1);
    struct desk_rect button=d.buttons[1].frame;
    desk_dialog_pointer(&d,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=button.x+10,.y=button.y+10,.down=1,.pressed=1},2);
    desk_dialog_focus(&d,0);
    a=desk_dialog_pointer(&d,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=button.x+10,.y=button.y+10},3);
    assert(!(a.flags&DESK_WIDGET_ACTIVATED) && !key(SRV_KEY_ESCAPE).id);
    desk_dialog_focus(&d,1);
    desk_dialog_pointer(&d,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=button.x+10,.y=button.y+10,.down=1,.pressed=1},4);
    desk_dialog_configure(&d,(struct desk_rect){0,0,W+1,H});
    a=desk_dialog_pointer(&d,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=button.x+10,.y=button.y+10},5);
    assert(!(a.flags&DESK_WIDGET_ACTIVATED));
    assert(desk_dialog_open(&d,"Invalid","",choices,4,3,3)<0 && !strcmp(d.title,"Save changes?"));
    choices[1].id=1;assert(desk_dialog_open(&d,"Invalid","",choices,3,3,3)<0);choices[1].id=2;
    choices[2].enabled=0;assert(desk_dialog_open(&d,"Invalid","",choices,3,3,3)<0);choices[2].enabled=1;
    assert(!desk_dialog_open(&d,d.title,d.message,d.choices,d.count,3,3)); /* Owned snapshot can be reused. */
    desk_dialog_configure(&d,(struct desk_rect){0,0,W,H});
    assert(desk_text_block_height(&desk_font_16,"one\ntwo",100,10)==2*desk_font_16.line_height);
    assert(desk_text_block_height(&desk_font_16,"word word",desk_text_width(&desk_font_16,"word"),10)==2*desk_font_16.line_height);
    assert(desk_text_block_height(&desk_font_16,"éé",1,10)==2*desk_font_16.line_height);
    assert(desk_text_block_height(&desk_font_16,"a very long sentence",1,2)==2*desk_font_16.line_height);
    for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) pixels[i]=tiled[i]=0xdeadbeef;
    struct desk_canvas full,tile;assert(!desk_canvas_init(&full,pixels,W,H,S));assert(!desk_canvas_init(&tile,tiled,W,H,S));
    desk_dialog_draw(&full,&d,&desk_theme_quiet);
    for(int y=0;y<H;y+=17) for(int x=0;x<W;x+=23) {
        desk_clip(&tile,(struct desk_rect){x,y,23,17});struct desk_rect clip=tile.clip;
        desk_dialog_draw(&tile,&d,&desk_theme_quiet);assert(!memcmp(&clip,&tile.clip,sizeof(clip)));
    }
    assert(!memcmp(pixels,tiled,sizeof(pixels)));
    for(int y=0;y<H;y++) for(int x=W;x<S;x++) assert(pixels[y*S+x]==0xdeadbeef);
    for(unsigned i=S*H;i<sizeof(pixels)/sizeof(*pixels);i++) assert(pixels[i]==0xdeadbeef);
    desk_dialog_configure(&d,(struct desk_rect){0,0,1,1});desk_dialog_draw(&full,&d,&desk_theme_quiet);
    puts("dialog: PASS safe initial focus, escape, modal pointer isolation, capture cancellation, owned snapshots, wrapping and clipped/tiled drawing");
}
