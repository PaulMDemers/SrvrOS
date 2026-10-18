#include <srvros/desk_prompt.h>
#include <string.h>
void desk_prompt_init(struct desk_prompt *p) {
    *p=(struct desk_prompt){0};desk_dialog_init(&p->dialog);
    desk_text_buffer_init(&p->text,p->storage[0],p->storage[1],DESK_PROMPT_BYTES);desk_edit_init(&p->edit,&p->text,0);
}
int desk_prompt_open(struct desk_prompt *p,const char *title,const char *message,const char *initial,const char *accept) {
    if(!initial || !accept) return -1;
    size_t n=0,label=0;while(n<DESK_PROMPT_BYTES && initial[n]) n++;while(label<24 && accept[label]) label++;
    if(n>=DESK_PROMPT_BYTES || !label || label>=24 || !desk_text_utf8_valid(initial,n)) return -1;
    for(size_t i=0;i<n;i++) if((unsigned char)initial[i]<32 || (unsigned char)initial[i]==127) return -1;
    char copy[DESK_PROMPT_BYTES];memcpy(copy,initial,n+1);
    struct desk_dialog_choice choices[]={{DESK_PROMPT_CANCEL,"Cancel",1},{DESK_PROMPT_ACCEPT,"",1}};memcpy(choices[1].label,accept,label+1);
    if(desk_dialog_open(&p->dialog,title,message,choices,2,DESK_PROMPT_CANCEL,DESK_PROMPT_CANCEL)) return -1;
    desk_text_buffer_set(&p->text,copy,n);desk_text_buffer_select(&p->text,0,n);desk_edit_init(&p->edit,&p->text,0);p->edit.enabled=1;
    desk_ui_add(&p->dialog.ui,DESK_PROMPT_FIELD,DESK_CONTROL_EDIT,&p->edit,NULL);desk_ui_focus(&p->dialog.ui,DESK_PROMPT_FIELD);
    return 0;
}
void desk_prompt_configure(struct desk_prompt *p,struct desk_rect bounds) {
    if(bounds.w<0 || bounds.h<0 || bounds.w>16384 || bounds.h>16384 || bounds.x<0 || bounds.y<0 || bounds.x>16384 || bounds.y>16384) return;
    int w=bounds.w>32?bounds.w-32:bounds.w;if(w>480) w=480;
    int h=bounds.h>32?bounds.h-32:bounds.h;if(h>256) h=256;
    struct desk_rect r={bounds.x+(bounds.w-w)/2,bounds.y+(bounds.h-h)/2,w,h};p->dialog.frame=r;
    int bw=w>48?(w-48)/2:0;if(bw>112) bw=112;
    for(int i=0;i<2;i++) desk_button_configure(&p->dialog.buttons[i],desk_intersect(r,(struct desk_rect){r.x+w-16-(2-i)*bw-(1-i)*8,r.y+h-48,bw,32}),1);
    desk_edit_configure(&p->edit,desk_intersect(r,(struct desk_rect){r.x+16,r.y+h-100,w>32?w-32:0,34}),&desk_font_16,1);
    desk_ui_layout_changed(&p->dialog.ui);
}
unsigned desk_prompt_focus(struct desk_prompt *p,int active) { return desk_dialog_focus(&p->dialog,active); }
struct desk_action desk_prompt_key(struct desk_prompt *p,const struct srv_key_event *key) {
    struct desk_action a=desk_dialog_key(&p->dialog,key);
    if(a.id==DESK_PROMPT_FIELD && (a.flags&DESK_EDIT_SUBMIT)) return (struct desk_action){DESK_PROMPT_ACCEPT,DESK_WIDGET_ACTIVATED|DESK_WIDGET_REDRAW};
    return a;
}
struct desk_action desk_prompt_pointer(struct desk_prompt *p,struct desk_widget_event event,uint64_t time_us) {
    if(event.type==DESK_WIDGET_POINTER && event.pressed && !desk_widget_contains(p->edit.frame,event.x,event.y) &&
        !desk_widget_contains(p->dialog.buttons[0].frame,event.x,event.y) && !desk_widget_contains(p->dialog.buttons[1].frame,event.x,event.y))
        return desk_ui_pointer(&p->dialog.ui,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL},time_us);
    return desk_ui_pointer(&p->dialog.ui,event,time_us);
}
void desk_prompt_draw(struct desk_canvas *c,const struct desk_prompt *p,const struct desk_theme *t) {
    struct desk_rect r=p->dialog.frame;if(r.w<=0 || r.h<=0) return;
    desk_fill(c,(struct desk_rect){r.x+4,r.y+5,r.w,r.h},0xb9c2b4);desk_fill(c,r,t->border);desk_fill(c,desk_widget_inset(r,1),t->surface);
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+16,r.w>32?r.w-32:0,30},t->heading,p->dialog.title,t->text);
    desk_text_block_draw(c,(struct desk_rect){r.x+16,r.y+56,r.w>32?r.w-32:0,r.h>168?r.h-168:0},t->body,p->dialog.message,t->muted);
    desk_ui_draw(c,&p->dialog.ui,t);
}
