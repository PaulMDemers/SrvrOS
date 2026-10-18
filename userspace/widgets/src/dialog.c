#include <srvros/desk_dialog.h>
#include <string.h>

static int string_valid(const char *text,size_t capacity) {
    if(!text) return 0;
    size_t n=0;while(n<capacity && text[n]) n++;
    return n<capacity && desk_text_utf8_valid(text,n);
}
void desk_dialog_init(struct desk_dialog *d) { *d=(struct desk_dialog){0};desk_ui_init(&d->ui); }
int desk_dialog_open(struct desk_dialog *d,const char *title,const char *message,
    const struct desk_dialog_choice *choices,unsigned count,unsigned cancel,unsigned focus) {
    if(!count || count>DESK_DIALOG_CHOICES || !choices || !string_valid(title,sizeof(d->title)) ||
       !string_valid(message,sizeof(d->message))) return -1;
    int have_cancel=!cancel,have_focus=!focus;
    for(unsigned i=0;i<count;i++) {
        if(!choices[i].id || !string_valid(choices[i].label,sizeof(choices[i].label)) || !choices[i].label[0]) return -1;
        for(unsigned j=0;j<i;j++) if(choices[j].id==choices[i].id) return -1;
        if(choices[i].enabled && choices[i].id==cancel) have_cancel=1;
        if(choices[i].enabled && choices[i].id==focus) have_focus=1;
    }
    if(!have_cancel || !have_focus) return -1;
    char title_copy[96],message_copy[384];struct desk_dialog_choice choice_copy[DESK_DIALOG_CHOICES];
    strcpy(title_copy,title);strcpy(message_copy,message);memcpy(choice_copy,choices,count*sizeof(*choices));
    choices=choice_copy;title=title_copy;message=message_copy;
    struct desk_rect old=d->frame;desk_dialog_init(d);d->frame=old;d->count=count;d->cancel=cancel;
    strcpy(d->title,title);strcpy(d->message,message);memcpy(d->choices,choices,count*sizeof(*choices));
    for(unsigned i=0;i<count;i++) {
        d->buttons[i].enabled=choices[i].enabled!=0;
        desk_ui_add(&d->ui,choices[i].id,DESK_CONTROL_BUTTON,&d->buttons[i],d->choices[i].label);
    }
    desk_ui_focus(&d->ui,focus?focus:choices[0].id);return 0;
}
void desk_dialog_configure(struct desk_dialog *d,struct desk_rect bounds) {
    if(bounds.w<0 || bounds.h<0 || bounds.w>16384 || bounds.h>16384 || bounds.x<0 || bounds.y<0 || bounds.x>16384 || bounds.y>16384) return;
    int w=bounds.w>32?bounds.w-32:bounds.w;if(w>480) w=480;
    int h=bounds.h>32?bounds.h-32:bounds.h;if(h>208) h=208;
    d->frame=(struct desk_rect){bounds.x+(bounds.w-w)/2,bounds.y+(bounds.h-h)/2,w,h};
    int available=w>32?w-32:0,gaps=d->count>1?(d->count-1)*8:0;
    int bw=d->count && available>gaps?(available-gaps)/(int)d->count:0;if(bw>112) bw=112;
    int x=d->frame.x+w-16-(bw*(int)d->count+gaps),y=d->frame.y+h-48;
    for(unsigned i=0;i<d->count;i++) desk_button_configure(&d->buttons[i],
        desk_intersect(d->frame,(struct desk_rect){x+(int)i*(bw+8),y,bw,32}),d->choices[i].enabled);
    desk_ui_layout_changed(&d->ui);
}
unsigned desk_dialog_focus(struct desk_dialog *d,int active) { return desk_ui_window_focus(&d->ui,active); }
struct desk_action desk_dialog_key(struct desk_dialog *d,const struct srv_key_event *key) {
    if(d->ui.active && (key->flags&SRV_KEY_DOWN_EVENT) && key->usage==SRV_KEY_ESCAPE && d->cancel)
        return (struct desk_action){d->cancel,DESK_WIDGET_ACTIVATED};
    return desk_ui_key(&d->ui,key);
}
struct desk_action desk_dialog_pointer(struct desk_dialog *d,struct desk_widget_event event,uint64_t time_us) {
    if(event.type==DESK_WIDGET_POINTER && event.pressed) {
        int hit=0;for(unsigned i=0;i<d->count;i++) if(d->buttons[i].enabled && desk_widget_contains(d->buttons[i].frame,event.x,event.y)) hit=1;
        if(!hit) return desk_ui_pointer(&d->ui,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL},time_us);
    }
    return desk_ui_pointer(&d->ui,event,time_us);
}
void desk_dialog_draw(struct desk_canvas *c,const struct desk_dialog *d,const struct desk_theme *t) {
    struct desk_rect r=d->frame;if(r.w<=0 || r.h<=0) return;
    desk_fill(c,(struct desk_rect){r.x+4,r.y+5,r.w,r.h},0xb9c2b4);desk_fill(c,r,t->border);
    desk_fill(c,desk_widget_inset(r,1),t->surface);
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+16,r.w>32?r.w-32:0,30},t->heading,d->title,t->text);
    desk_text_block_draw(c,(struct desk_rect){r.x+16,r.y+56,r.w>32?r.w-32:0,r.h>120?r.h-120:0},t->body,d->message,t->muted);
    desk_ui_draw(c,&d->ui,t);
}
