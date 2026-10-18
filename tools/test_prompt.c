#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_prompt.h>
int main(void) {
    struct desk_prompt p;desk_prompt_init(&p);
    assert(!desk_prompt_open(&p,"Rename","Enter a new name.","notes.txt","Rename"));
    desk_prompt_configure(&p,(struct desk_rect){0,0,720,480});
    assert(p.edit.focused && p.text.anchor==0 && p.text.caret==9);
    struct srv_key_event key={.flags=SRV_KEY_DOWN_EVENT,.usage=0x04,.codepoint='a'};
    struct desk_action a=desk_prompt_key(&p,&key);assert(a.id==DESK_PROMPT_FIELD && (a.flags&DESK_WIDGET_CHANGED) && !strcmp(p.text.data,"a"));
    key.usage=SRV_KEY_ENTER;key.codepoint=0;a=desk_prompt_key(&p,&key);assert(a.id==DESK_PROMPT_ACCEPT && (a.flags&DESK_WIDGET_ACTIVATED));
    key.usage=SRV_KEY_ESCAPE;a=desk_prompt_key(&p,&key);assert(a.id==DESK_PROMPT_CANCEL && (a.flags&DESK_WIDGET_ACTIVATED));
    key.usage=SRV_KEY_TAB;desk_prompt_key(&p,&key);assert(p.dialog.buttons[0].focused);
    key.usage=SRV_KEY_ENTER;a=desk_prompt_key(&p,&key);assert(a.id==DESK_PROMPT_CANCEL);
    struct desk_rect r=p.dialog.buttons[1].frame;
    struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=r.x+20,.y=r.y+10,.down=1,.pressed=1};
    desk_prompt_pointer(&p,e,0);assert(p.dialog.buttons[1].armed);desk_prompt_focus(&p,0);desk_prompt_focus(&p,1);
    e.down=e.pressed=0;a=desk_prompt_pointer(&p,e,0);assert(!(a.flags&DESK_WIDGET_ACTIVATED));
    desk_ui_focus(&p.dialog.ui,DESK_PROMPT_FIELD);
    desk_prompt_pointer(&p,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=0,.y=0,.down=1,.pressed=1},0);assert(p.edit.focused);
    key=(struct srv_key_event){.usage=0x06,.flags=SRV_KEY_DOWN_EVENT,.modifiers=SRV_KEY_CTRL};a=desk_prompt_key(&p,&key);assert(a.id==DESK_PROMPT_FIELD && (a.flags&DESK_EDIT_COPY));
    assert(desk_prompt_open(&p,"Bad","Message","line\nbreak","OK")<0 && !strcmp(p.text.data,"a"));
    assert(!desk_prompt_open(&p,p.dialog.title,p.dialog.message,p.text.data,"Rename"));assert(!strcmp(p.text.data,"a"));
    desk_prompt_configure(&p,(struct desk_rect){0,0,280,240});
    uint32_t pixels[280*240]={0};struct desk_canvas c;assert(!desk_canvas_init(&c,pixels,280,240,280));struct desk_rect clip={30,30,200,180};desk_clip(&c,clip);desk_prompt_draw(&c,&p,&desk_theme_quiet);
    assert(!memcmp(&clip,&c.clip,sizeof(clip)));
    for(int y=0;y<240;y++) for(int x=0;x<280;x++) if(x<30 || x>=230 || y<30 || y>=210) assert(!pixels[y*280+x]);
    desk_prompt_configure(&p,(struct desk_rect){0,0,1,1});desk_prompt_draw(&c,&p,&desk_theme_quiet);
    puts("prompt: PASS owned draft, editing/submit/cancel, focus traversal, capture cancellation, outside click, clipboard requests, reopen and clipping");
}
