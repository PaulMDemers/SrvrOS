#include <srvros/desk_bitmap.h>
#include <string.h>
size_t desk_bitmap_storage_bytes(uint32_t w,uint32_t h,unsigned history) {
    if(!w || !h || w>4096 || h>4096 || (uint64_t)w*h>DESK_BITMAP_MAX_PIXELS || history<2 || history>DESK_BITMAP_HISTORY) return 0;
    return (size_t)w*h*4*(history+1);
}
static size_t bytes(const struct desk_bitmap *b) { return (size_t)b->width*b->height*4; }
static uint32_t flatten(uint32_t value) {
    unsigned a=value>>24,r=(((value>>16)&255)*a+255*(255-a)+127)/255;
    unsigned g=(((value>>8)&255)*a+255*(255-a)+127)/255,blue=((value&255)*a+255*(255-a)+127)/255;
    return 0xff000000u|(r<<16)|(g<<8)|blue;
}
int desk_bitmap_init(struct desk_bitmap *b,void *storage,size_t size,uint32_t w,uint32_t h,unsigned history,const uint32_t *source) {
    size_t needed=desk_bitmap_storage_bytes(w,h,history);
    if(!needed || !storage || size<needed || (uintptr_t)storage%_Alignof(uint32_t)) return -1;
    if(source && source!=storage) {
        uintptr_t a=(uintptr_t)storage,s=(uintptr_t)source;size_t image=(size_t)w*h*4;
        if(a>UINTPTR_MAX-needed || s>UINTPTR_MAX-image || (s<a+needed && a<s+image)) return -1;
    }
    struct desk_bitmap next={.pixels=storage,.width=w,.height=h,.capacity=history,.count=1,.next_revision=1,.saved_revision=1};
    for(unsigned i=0;i<history;i++) next.frames[i]=next.pixels+(size_t)w*h*(i+1);
    for(size_t i=0;i<(size_t)w*h;i++) next.pixels[i]=source?flatten(source[i]):0xffffffffu;
    memcpy(next.frames[0],next.pixels,bytes(&next));next.revisions[0]=1;*b=next;return 0;
}
uint64_t desk_bitmap_revision(const struct desk_bitmap *b) { return b->count?b->revisions[b->index]:0; }
int desk_bitmap_dirty(const struct desk_bitmap *b) { return (b->active && b->changed) || desk_bitmap_revision(b)!=b->saved_revision; }
int desk_bitmap_begin(struct desk_bitmap *b) {
    if(!b->pixels || b->active || b->next_revision==UINT64_MAX) return -1;
    b->active=1;b->changed=0;return 0;
}
void desk_bitmap_restore(struct desk_bitmap *b) {
    if(b->active) { memcpy(b->pixels,b->frames[b->index],bytes(b));b->changed=0; }
}
void desk_bitmap_cancel(struct desk_bitmap *b) { desk_bitmap_restore(b);b->active=b->changed=0; }
int desk_bitmap_commit(struct desk_bitmap *b) {
    if(!b->active) return 0;
    b->active=b->changed=0;if(!memcmp(b->pixels,b->frames[b->index],bytes(b))) return 0;
    if(b->index+1==b->capacity) {
        uint32_t *oldest=b->frames[0];
        memmove(b->frames,b->frames+1,(b->capacity-1)*sizeof(b->frames[0]));b->frames[b->capacity-1]=oldest;
        memmove(b->revisions,b->revisions+1,(b->capacity-1)*sizeof(b->revisions[0]));b->index--;
    }
    b->index++;b->count=b->index+1;memcpy(b->frames[b->index],b->pixels,bytes(b));b->revisions[b->index]=++b->next_revision;return 1;
}
int desk_bitmap_undo(struct desk_bitmap *b) {
    if(b->active || !b->index) return 0;
    b->index--;memcpy(b->pixels,b->frames[b->index],bytes(b));return 1;
}
int desk_bitmap_redo(struct desk_bitmap *b) {
    if(b->active || b->index+1>=b->count) return 0;
    b->index++;memcpy(b->pixels,b->frames[b->index],bytes(b));return 1;
}
static int clamp(int n,unsigned maximum) { return n<0?0:(unsigned)n>=maximum?(int)maximum-1:n; }
static void brush(struct desk_bitmap *b,int x,int y,uint32_t rgb,unsigned size) {
    int left=(int)size/2,right=((int)size-1)/2,shift=size%2?0:1;uint32_t color=rgb|0xff000000u;
    for(int dy=-left;dy<=right;dy++) {
        int py=y+dy;if(py<0 || (unsigned)py>=b->height) continue;
        int yy=2*dy+shift;
        for(int dx=-left;dx<=right;dx++) {
            int px=x+dx,xx=2*dx+shift;if(px<0 || (unsigned)px>=b->width || (unsigned)(xx*xx+yy*yy)>size*size) continue;
            uint32_t *pixel=b->pixels+(size_t)py*b->width+px;if(*pixel!=color) { *pixel=color;b->changed=1; }
        }
    }
}
void desk_bitmap_line(struct desk_bitmap *b,int x0,int y0,int x1,int y1,uint32_t rgb,unsigned size) {
    if(!b->active || !size || size>32) return;
    x0=clamp(x0,b->width);x1=clamp(x1,b->width);y0=clamp(y0,b->height);y1=clamp(y1,b->height);
    int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1,dy=y1>y0?y0-y1:y1-y0,sy=y0<y1?1:-1,error=dx+dy;
    for(;;) {
        brush(b,x0,y0,rgb,size);if(x0==x1 && y0==y1) break;
        int twice=2*error;if(twice>=dy) { error+=dy;x0+=sx; }if(twice<=dx) { error+=dx;y0+=sy; }
    }
}
void desk_bitmap_box(struct desk_bitmap *b,int x0,int y0,int x1,int y1,uint32_t rgb,unsigned size) {
    desk_bitmap_line(b,x0,y0,x1,y0,rgb,size);desk_bitmap_line(b,x1,y0,x1,y1,rgb,size);
    desk_bitmap_line(b,x1,y1,x0,y1,rgb,size);desk_bitmap_line(b,x0,y1,x0,y0,rgb,size);
}
int desk_bitmap_clear(struct desk_bitmap *b,uint32_t rgb) {
    if(desk_bitmap_begin(b)) return 0;
    for(size_t i=0;i<(size_t)b->width*b->height;i++) b->pixels[i]=rgb|0xff000000u;
    return desk_bitmap_commit(b);
}
