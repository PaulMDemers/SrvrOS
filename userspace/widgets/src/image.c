#include <srvros/desk_image.h>
#include <string.h>
#define ONE 65536u
static int clamp(int64_t n,int limit) { return n<0?0:n>limit?limit:(int)n; }
static int scaled(uint32_t size,uint32_t scale) { return (int)(((uint64_t)size*scale+ONE-1)/ONE); }
static void limits(struct desk_image *v,int *x,int *y) {
    struct desk_rect r=desk_widget_inset(v->frame,1);
    *x=scaled(v->width,v->scale)-r.w;if(*x<0) *x=0;
    *y=scaled(v->height,v->scale)-r.h;if(*y<0) *y=0;
}
static void layout(struct desk_image *v) {
    struct desk_rect r=desk_widget_inset(v->frame,1);
    if(v->fit && v->width && v->height && r.w && r.h) {
        uint32_t x=(uint32_t)((uint64_t)r.w*ONE/v->width),y=(uint32_t)((uint64_t)r.h*ONE/v->height);
        v->scale=x<y?x:y;if(v->scale>ONE) v->scale=ONE;if(!v->scale) v->scale=1;
    }
    int x,y;limits(v,&x,&y);v->offset_x=clamp(v->offset_x,x);v->offset_y=clamp(v->offset_y,y);
}
void desk_image_init(struct desk_image *v) { *v=(struct desk_image){.fit=1,.scale=ONE}; }
int desk_image_set(struct desk_image *v,const uint32_t *pixels,uint32_t width,uint32_t height) {
    if((!pixels && (width || height)) || (pixels && (!width || !height)) || width>SRV_IMAGE_MAX_DIMENSION || height>SRV_IMAGE_MAX_DIMENSION ||
       (uint64_t)width*height>SRV_IMAGE_MAX_PIXELS) return -1;
    v->pixels=pixels;v->width=width;v->height=height;v->offset_x=v->offset_y=v->dragging=0;v->fit=1;layout(v);return 0;
}
void desk_image_configure(struct desk_image *v,struct desk_rect r,int active) {
    if(r.x<-16384 || r.y<-16384 || r.x>16384 || r.y>16384 || r.w<0 || r.h<0 || r.w>16384 || r.h>16384) return;
    if(memcmp(&v->frame,&r,sizeof(r)) || !active) v->dragging=0;
    v->frame=r;v->enabled=active && r.w>2 && r.h>2;if(!v->enabled) v->focused=0;layout(v);
}
unsigned desk_image_fit(struct desk_image *v) {
    v->fit=1;v->offset_x=v->offset_y=v->dragging=0;layout(v);return DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW;
}
static unsigned zoom_to(struct desk_image *v,uint32_t scale) {
    if(!v->pixels || !scale) return 0;
    struct desk_rect r=desk_widget_inset(v->frame,1);
    /* Preserve the image point at the center; centered margins count as zero
     * source offset so zooming a small image starts from its actual center. */
    int old_w=scaled(v->width,v->scale),old_h=scaled(v->height,v->scale);
    int64_t center_x=old_w<r.w?old_w/2:(int64_t)v->offset_x+r.w/2;
    int64_t center_y=old_h<r.h?old_h/2:(int64_t)v->offset_y+r.h/2;
    int64_t x=center_x*scale/v->scale-r.w/2,y=center_y*scale/v->scale-r.h/2;
    v->scale=scale;v->fit=0;v->dragging=0;int lx,ly;limits(v,&lx,&ly);
    v->offset_x=clamp(x,lx);v->offset_y=clamp(y,ly);return DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW;
}
unsigned desk_image_actual(struct desk_image *v) { return zoom_to(v,ONE); }
unsigned desk_image_zoom(struct desk_image *v,int direction) {
    static const uint32_t steps[]={ONE/16,ONE/8,ONE/4,ONE/2,ONE,ONE*2,ONE*4};
    if(direction>0) { for(unsigned i=0;i<sizeof(steps)/sizeof(*steps);i++) if(steps[i]>v->scale) return zoom_to(v,steps[i]); }
    else if(direction<0) { for(int i=(int)(sizeof(steps)/sizeof(*steps))-1;i>=0;i--) if(steps[i]<v->scale) return zoom_to(v,steps[i]); }
    return 0;
}
unsigned desk_image_scroll(struct desk_image *v,int32_t dx,int32_t dy) {
    if(!v->enabled || !v->pixels) return 0;
    int x,y;limits(v,&x,&y);x=clamp((int64_t)v->offset_x+dx,x);y=clamp((int64_t)v->offset_y+dy,y);
    if(x==v->offset_x && y==v->offset_y) return 0;
    v->offset_x=x;v->offset_y=y;return DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW;
}
unsigned desk_image_event(struct desk_image *v,struct desk_widget_event e) {
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        v->dragging=0;if(e.type==DESK_WIDGET_FOCUS_OUT) { v->focused=0;return DESK_WIDGET_REDRAW; }return 0;
    }
    if(!v->enabled) return 0;
    if(e.type==DESK_WIDGET_FOCUS_IN) { v->focused=1;return DESK_WIDGET_REDRAW; }
    if(e.type!=DESK_WIDGET_POINTER || !v->pixels) return 0;
    if(e.pressed) {
        v->dragging=e.down && desk_widget_contains(desk_widget_inset(v->frame,1),e.x,e.y);
        v->grab_x=e.x;v->grab_y=e.y;v->grab_offset_x=v->offset_x;v->grab_offset_y=v->offset_y;return 0;
    }
    if(!v->dragging) return 0;
    int x,y;limits(v,&x,&y);x=clamp((int64_t)v->grab_offset_x+v->grab_x-e.x,x);y=clamp((int64_t)v->grab_offset_y+v->grab_y-e.y,y);
    unsigned changed=x!=v->offset_x || y!=v->offset_y;v->offset_x=x;v->offset_y=y;if(!e.down) v->dragging=0;
    return changed?DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW:0;
}
unsigned desk_image_key(struct desk_image *v,const struct srv_key_event *k) {
    if(k->flags&SRV_KEY_CANCEL) return desk_image_event(v,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    if(!v->enabled || !v->focused || !(k->flags&SRV_KEY_DOWN_EVENT) || (k->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META))) return 0;
    v->dragging=0;
    if(k->codepoint=='+' || k->codepoint=='=') return desk_image_zoom(v,1);
    if(k->codepoint=='-') return desk_image_zoom(v,-1);
    if(k->codepoint=='0') return desk_image_fit(v);
    if(k->codepoint=='1') return desk_image_actual(v);
    int step=k->modifiers&SRV_KEY_SHIFT?128:32;
    switch(k->usage) {
    case SRV_KEY_LEFT: return desk_image_scroll(v,-step,0);
    case SRV_KEY_RIGHT: return desk_image_scroll(v,step,0);
    case SRV_KEY_UP: return desk_image_scroll(v,0,-step);
    case SRV_KEY_DOWN: return desk_image_scroll(v,0,step);
    case SRV_KEY_PAGEUP: return desk_image_scroll(v,0,-v->frame.h);
    case SRV_KEY_PAGEDOWN: return desk_image_scroll(v,0,v->frame.h);
    case SRV_KEY_HOME: return desk_image_scroll(v,-16384,-16384);
    case SRV_KEY_END: return desk_image_scroll(v,16384,16384);
    default: return 0;
    }
}
static uint32_t blend(uint32_t pixel,uint32_t background) {
    unsigned a=pixel>>24;if(a==255) return pixel&0xffffff;if(!a) return background;
    unsigned r=(((pixel>>16)&255)*a+((background>>16)&255)*(255-a)+127)/255;
    unsigned g=(((pixel>>8)&255)*a+((background>>8)&255)*(255-a)+127)/255;
    unsigned b=((pixel&255)*a+(background&255)*(255-a)+127)/255;return (r<<16)|(g<<8)|b;
}
void desk_image_draw(struct desk_canvas *c,const struct desk_image *v,const struct desk_theme *theme) {
    struct desk_rect old=c->clip;desk_clip(c,desk_intersect(old,v->frame));desk_fill(c,v->frame,v->focused?theme->accent:theme->border);
    struct desk_rect r=desk_widget_inset(v->frame,1);desk_fill(c,r,theme->track);
    if(v->pixels && r.w && r.h) {
        int w=scaled(v->width,v->scale),h=scaled(v->height,v->scale);
        int x=r.x+(w<r.w?(r.w-w)/2:-v->offset_x),y=r.y+(h<r.h?(r.h-h)/2:-v->offset_y);
        struct desk_rect visible=desk_intersect(c->clip,desk_intersect(r,(struct desk_rect){x,y,w,h}));
        if(!visible.w || !visible.h) { c->clip=old;return; }
        /* Exact rational stepping removes a divide from every output pixel.
         * Start from the clipped left edge, retaining the division remainder,
         * so tiled and full draws sample precisely the same source pixels. */
        uint64_t numerator=(uint64_t)(visible.x-x)*ONE;
        uint32_t first=numerator/v->scale,first_remainder=numerator%v->scale;
        uint32_t step=ONE/v->scale,remainder_step=ONE%v->scale;
        for(int py=visible.y;py<visible.y+visible.h;py++) {
            uint32_t sy=(uint32_t)((uint64_t)(py-y)*ONE/v->scale);if(sy>=v->height) sy=v->height-1;
            const uint32_t *source=v->pixels+(size_t)sy*v->width;
            uint32_t *target=c->pixels+(size_t)py*c->stride+visible.x;
            uint32_t sx=first,remainder=first_remainder;
            for(int px=visible.x;px<visible.x+visible.w;px++) {
                uint32_t bg=(((px-x)/8+(py-y)/8)&1)?0xd7ddd5:0xf1f0eb;
                *target++=blend(source[sx<v->width?sx:v->width-1],bg);
                sx+=step;remainder+=remainder_step;
                if(remainder>=v->scale) { remainder-=v->scale;sx++; }
            }
        }
        c->pixel_writes+=(uint64_t)visible.w*visible.h;
    }
    c->clip=old;
}
