#include <srvros/desk_flow.h>
#include <string.h>

static const char *line_end(const struct desk_font *font,const char *start,int width,const char **next) {
    const char *p=start,*space=NULL,*after_space=NULL;int64_t used=0;
    while(*p) {
        if(*p=='\n') { *next=p+1;return p; }
        const char *q=p;desk_utf8_next(&q);char glyph[5]={0};size_t bytes=q-p;
        if(bytes>4) bytes=4;
        if(*p=='\t') memcpy(glyph,"    ",4);else memcpy(glyph,p,bytes);
        int advance=desk_text_width(font,glyph);
        if(used+advance>width && p>start) {
            if(*p==' ' || *p=='\t') { *next=q;while(**next==' ' || **next=='\t') (*next)++;return p; }
            if(space && space>start) { *next=after_space;while(**next==' ' || **next=='\t') (*next)++;return space; }
            *next=p;return p;
        }
        if(*p==' ' || *p=='\t') { space=p;after_space=q; }
        used+=advance;p=q;
    }
    *next=p;return p;
}
int desk_text_block_height(const struct desk_font *font,const char *text,int width,int max_lines) {
    if(!font || !text || width<=0 || max_lines<=0) return 0;
    int lines=0;const char *p=text;
    while(*p && lines<max_lines) { const char *next;line_end(font,p,width,&next);p=next;lines++; }
    int64_t height=(int64_t)lines*font->line_height;return height>INT32_MAX?INT32_MAX:(int)height;
}
void desk_text_block_draw(struct desk_canvas *c,struct desk_rect frame,const struct desk_font *font,const char *text,uint32_t color) {
    if(!font || !text || frame.w<=0 || frame.h<=0 || !font->line_height) return;
    struct desk_rect clip=c->clip;desk_clip(c,desk_intersect(clip,frame));
    const char *p=text;int64_t y=frame.y;
    while(*p && y<(int64_t)frame.y+frame.h) {
        const char *next,*end=line_end(font,p,frame.w,&next);int64_t x=frame.x;
        while(p<end) {
            const char *q=p;desk_utf8_next(&q);char glyph[5]={0};size_t bytes=q-p;
            if(bytes>4) bytes=4;
            if(*p=='\t') memcpy(glyph,"    ",4);else memcpy(glyph,p,bytes);
            if(x>=INT32_MIN && x<=INT32_MAX && y>=INT32_MIN && y<=INT32_MAX) desk_text(c,font,x,y,glyph,color);
            x+=desk_text_width(font,glyph);p=q;
        }
        p=next;y+=font->line_height;
    }
    c->clip=clip;
}
