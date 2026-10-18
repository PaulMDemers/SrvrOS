#include <srvros/desk_color_panel.h>
#include <stdio.h>
static const uint32_t colors[DESK_COLOR_SWATCHES]={0x292e2b,0xffffff,0x89948b,0xb44848,0xd0783b,0xd7b44a,0x38644f,0x79a674,0x3e6d96,0x82b7c5,0x796198,0xc07899};
static void update(struct desk_color_panel *p,uint32_t color) {
    p->color=color&0xffffff;snprintf(p->hex,sizeof(p->hex),"#%06X",p->color);
    static const char names[]="RGB";
    for(unsigned i=0;i<3;i++) {
        unsigned value=(p->color>>(16-i*8))&255;desk_slider_set(&p->channels[i],value);
        snprintf(p->labels[i],sizeof(p->labels[i]),"%c %u",names[i],value);
    }
}
void desk_color_panel_init(struct desk_color_panel *p) {
    *p=(struct desk_color_panel){0};desk_ui_init(&p->ui);
    for(unsigned i=0;i<3;i++) { desk_slider_init(&p->channels[i]);desk_slider_range(&p->channels[i],0,255,1);desk_ui_add(&p->ui,DESK_COLOR_RED+i,DESK_CONTROL_SLIDER,&p->channels[i],NULL); }
    for(unsigned i=0;i<DESK_COLOR_SWATCHES;i++) desk_ui_add(&p->ui,DESK_COLOR_SWATCH+i,DESK_CONTROL_BUTTON,&p->swatches[i],NULL);
    desk_ui_add(&p->ui,DESK_COLOR_COPY,DESK_CONTROL_BUTTON,&p->copy,"Copy hex");
    desk_ui_add(&p->ui,DESK_COLOR_CANCEL,DESK_CONTROL_BUTTON,&p->cancel,"Cancel");
    desk_ui_add(&p->ui,DESK_COLOR_ACCEPT,DESK_CONTROL_BUTTON,&p->accept,"Use color");
    desk_button_centered(&p->copy,1);desk_button_centered(&p->cancel,1);desk_button_centered(&p->accept,1);update(p,0);
}
void desk_color_panel_open(struct desk_color_panel *p,uint32_t rgb) {
    desk_ui_window_focus(&p->ui,0);update(p,rgb);desk_ui_window_focus(&p->ui,1);
    desk_ui_focus(&p->ui,DESK_COLOR_RED);
}
void desk_color_panel_configure(struct desk_color_panel *p,struct desk_rect bounds) {
    if(bounds.x<0 || bounds.y<0 || bounds.x>16384 || bounds.y>16384 || bounds.w<0 || bounds.h<0 || bounds.w>16384 || bounds.h>16384) return;
    int w=bounds.w>16?bounds.w-16:bounds.w,h=bounds.h>16?bounds.h-16:bounds.h;
    if(w>400) w=400;
    if(h>224) h=224;
    struct desk_rect r={bounds.x+(bounds.w-w)/2,bounds.y+(bounds.h-h)/2,w,h};p->frame=r;
    int enabled=w>=248 && h>=224;
    for(unsigned i=0;i<3;i++) desk_slider_configure(&p->channels[i],desk_intersect(r,(struct desk_rect){r.x+68,r.y+44+(int)i*28,w>80?w-80:0,24}),enabled);
    int available=w>24?w-24:0;
    for(unsigned i=0;i<DESK_COLOR_SWATCHES;i++) {
        int left=available*i/DESK_COLOR_SWATCHES,right=available*(i+1)/DESK_COLOR_SWATCHES;
        desk_button_configure(&p->swatches[i],desk_intersect(r,(struct desk_rect){r.x+12+left,r.y+136,right-left>2?right-left-2:0,24}),enabled);
    }
    p->ui.controls[p->ui.count-3].label=w<340?"Copy":"Copy hex";
    p->ui.controls[p->ui.count-1].label=w<340?"Use":"Use color";
    int bw=w>40?(w-40)/3:0;
    desk_button_configure(&p->copy,desk_intersect(r,(struct desk_rect){r.x+12,r.y+h-44,bw,32}),enabled);
    desk_button_configure(&p->cancel,desk_intersect(r,(struct desk_rect){r.x+20+bw,r.y+h-44,bw,32}),enabled);
    desk_button_configure(&p->accept,desk_intersect(r,(struct desk_rect){r.x+28+bw*2,r.y+h-44,bw,32}),enabled);
    desk_ui_layout_changed(&p->ui);
}
unsigned desk_color_panel_focus(struct desk_color_panel *p,int active) { return desk_ui_window_focus(&p->ui,active); }
static struct desk_action action(struct desk_color_panel *p,struct desk_action a) {
    if(a.id>=DESK_COLOR_RED && a.id<=DESK_COLOR_BLUE && (a.flags&DESK_WIDGET_CHANGED)) {
        update(p,((uint32_t)p->channels[0].value<<16)|((uint32_t)p->channels[1].value<<8)|(uint32_t)p->channels[2].value);
    } else if(a.id>=DESK_COLOR_SWATCH && a.id<DESK_COLOR_SWATCH+DESK_COLOR_SWATCHES && (a.flags&DESK_WIDGET_ACTIVATED)) {
        uint32_t color=colors[a.id-DESK_COLOR_SWATCH];if(color!=p->color) { update(p,color);a.flags|=DESK_WIDGET_CHANGED; }
    }
    return a;
}
struct desk_action desk_color_panel_key(struct desk_color_panel *p,const struct srv_key_event *key) {
    if(p->ui.active && (key->flags&SRV_KEY_DOWN_EVENT) && !(key->flags&SRV_KEY_REPEAT) && !(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META))) {
        if(key->usage==SRV_KEY_ESCAPE) return (struct desk_action){DESK_COLOR_CANCEL,DESK_WIDGET_ACTIVATED};
        if(key->usage==SRV_KEY_ENTER && p->ui.focus>=0 && p->ui.controls[p->ui.focus].kind==DESK_CONTROL_SLIDER) return (struct desk_action){DESK_COLOR_ACCEPT,DESK_WIDGET_ACTIVATED};
    }
    return action(p,desk_ui_key(&p->ui,key));
}
struct desk_action desk_color_panel_pointer(struct desk_color_panel *p,struct desk_widget_event e,uint64_t time_us) { return action(p,desk_ui_pointer(&p->ui,e,time_us)); }
void desk_color_panel_draw(struct desk_canvas *c,const struct desk_color_panel *p,const struct desk_theme *t) {
    struct desk_rect r=p->frame,old=c->clip;if(!r.w || !r.h) return;
    desk_fill(c,(struct desk_rect){r.x+4,r.y+5,r.w,r.h},0xb9c2b4);desk_clip(c,desk_intersect(old,r));
    desk_fill(c,r,t->border);desk_fill(c,desk_widget_inset(r,1),t->surface);
    desk_label_ellipsis(c,(struct desk_rect){r.x+12,r.y+12,r.w>152?r.w-152:0,24},t->body,"Color",t->text);
    desk_label_draw(c,(struct desk_rect){r.x+r.w-140,r.y+12,82,24},&desk_font_16,p->hex,t->text);
    desk_fill(c,(struct desk_rect){r.x+r.w-52,r.y+10,40,26},t->border);desk_fill(c,(struct desk_rect){r.x+r.w-50,r.y+12,36,22},p->color);
    for(unsigned i=0;i<3;i++) desk_label_draw(c,(struct desk_rect){r.x+12,r.y+44+(int)i*28,52,24},&desk_font_16,p->labels[i],t->text);
    desk_ui_draw(c,&p->ui,t);
    for(unsigned i=0;i<DESK_COLOR_SWATCHES;i++) {
        const struct desk_button *b=&p->swatches[i];struct desk_rect cell=desk_widget_inset(b->frame,4);
        desk_fill(c,cell,colors[i]);
        if(p->color==colors[i] && cell.w>4 && cell.h>4) {
            uint32_t mark=colors[i]==0xffffff?0x292e2b:0xffffff;
            desk_fill(c,(struct desk_rect){cell.x+cell.w/2-1,cell.y+cell.h/2-1,3,3},mark);
        }
    }
    c->clip=old;
}
