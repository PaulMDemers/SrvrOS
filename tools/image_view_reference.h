/* Deliberately direct reference: one rational coordinate division per pixel.
 * Keep independent of the optimized sampler and use only in host tests. */
#include <srvros/desk_image.h>
static void image_reference(struct desk_canvas *c,const struct desk_image *v,const struct desk_theme *theme) {
    struct desk_rect old=c->clip;desk_clip(c,desk_intersect(old,v->frame));desk_fill(c,v->frame,v->focused?theme->accent:theme->border);
    struct desk_rect r=desk_widget_inset(v->frame,1);desk_fill(c,r,theme->track);
    if(v->pixels && r.w && r.h) {
        int w=((uint64_t)v->width*v->scale+65535)/65536,h=((uint64_t)v->height*v->scale+65535)/65536;
        int x=r.x+(w<r.w?(r.w-w)/2:-v->offset_x),y=r.y+(h<r.h?(r.h-h)/2:-v->offset_y);
        struct desk_rect visible=desk_intersect(c->clip,desk_intersect(r,(struct desk_rect){x,y,w,h}));
        for(int py=visible.y;py<visible.y+visible.h;py++) for(int px=visible.x;px<visible.x+visible.w;px++) {
            unsigned sy=(uint64_t)(py-y)*65536/v->scale,sx=(uint64_t)(px-x)*65536/v->scale;
            if(sx>=v->width) sx=v->width-1;
            if(sy>=v->height) sy=v->height-1;
            uint32_t input=v->pixels[(size_t)sy*v->width+sx],alpha=input>>24;
            uint32_t background=(((px-x)/8+(py-y)/8)&1)?0xd7ddd5:0xf1f0eb,result=0;
            for(unsigned shift=0;shift<24;shift+=8) result|=((((input>>shift)&255)*alpha+((background>>shift)&255)*(255-alpha)+127)/255)<<shift;
            c->pixels[(size_t)py*c->stride+px]=result;c->pixel_writes++;
        }
    }
    c->clip=old;
}
