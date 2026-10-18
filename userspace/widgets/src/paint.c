#include <srvros/desk_paint.h>
#include <string.h>
void desk_paint_init(struct desk_paint *p) { *p=(struct desk_paint){.color=0x292e2b,.size=3};desk_image_init(&p->view); }
static unsigned cancel(struct desk_paint *p) {
    if(!p->drawing) return 0;
    desk_bitmap_cancel(p->bitmap);p->drawing=0;return DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW;
}
int desk_paint_bind(struct desk_paint *p,struct desk_bitmap *b) {
    if(!b || !b->pixels || !b->count || b->active) return -1;
    cancel(p);p->bitmap=b;return desk_image_set(&p->view,b->pixels,b->width,b->height);
}
void desk_paint_configure(struct desk_paint *p,struct desk_rect r,int enabled) {
    if(memcmp(&p->view.frame,&r,sizeof(r)) || !enabled) cancel(p);
    desk_image_configure(&p->view,r,enabled);
}
void desk_paint_tool(struct desk_paint *p,enum desk_paint_tool tool,uint32_t rgb,unsigned size) {
    if(tool<DESK_PAINT_PEN || tool>DESK_PAINT_PAN || !size || size>32) return;
    if(p->tool!=tool || p->color!=(rgb&0xffffff) || p->size!=size) {
        cancel(p);desk_image_event(&p->view,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    }
    p->tool=tool;p->color=rgb&0xffffff;p->size=size;
}
static unsigned draw_to(struct desk_paint *p,int x,int y) {
    if(p->tool==DESK_PAINT_PEN) desk_bitmap_line(p->bitmap,p->last_x,p->last_y,x,y,p->color,p->size);
    else {
        desk_bitmap_restore(p->bitmap);
        if(p->tool==DESK_PAINT_LINE) desk_bitmap_line(p->bitmap,p->start_x,p->start_y,x,y,p->color,p->size);
        else desk_bitmap_box(p->bitmap,p->start_x,p->start_y,x,y,p->color,p->size);
    }
    p->last_x=x;p->last_y=y;return DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW;
}
unsigned desk_paint_event(struct desk_paint *p,struct desk_widget_event e) {
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) return cancel(p)|desk_image_event(&p->view,e);
    if(e.type!=DESK_WIDGET_POINTER || p->tool==DESK_PAINT_PAN) return desk_image_event(&p->view,e);
    if(!p->view.enabled || !p->bitmap) return 0;
    int32_t x,y;
    if(e.pressed) {
        if(!e.down || !desk_image_point(&p->view,e.x,e.y,0,&x,&y) || desk_bitmap_begin(p->bitmap)) return 0;
        p->drawing=1;p->start_x=p->last_x=x;p->start_y=p->last_y=y;return draw_to(p,x,y);
    }
    if(!p->drawing || !desk_image_point(&p->view,e.x,e.y,1,&x,&y)) return 0;
    unsigned result=draw_to(p,x,y);
    if(!e.down) { desk_bitmap_commit(p->bitmap);p->drawing=0;result|=DESK_WIDGET_ACTIVATED; }return result;
}
unsigned desk_paint_key(struct desk_paint *p,const struct srv_key_event *key) {
    if(key->flags&SRV_KEY_CANCEL) return desk_paint_event(p,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    if(!(key->flags&SRV_KEY_DOWN_EVENT) || !p->view.focused) return 0;
    if(key->usage==SRV_KEY_ESCAPE && p->drawing) return cancel(p);
    if(p->drawing) return 0;
    return desk_image_key(&p->view,key);
}
unsigned desk_paint_scroll(struct desk_paint *p,int32_t dx,int32_t dy) { return p->drawing?0:desk_image_scroll(&p->view,dx,dy); }
void desk_paint_draw(struct desk_canvas *c,const struct desk_paint *p,const struct desk_theme *t) { desk_image_draw(c,&p->view,t); }
