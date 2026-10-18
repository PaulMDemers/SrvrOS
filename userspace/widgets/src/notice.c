#include <srvros/desk_notice.h>
#include <srvros/desk_flow.h>
#include <srvros/desk_text.h>
#include <string.h>
static int valid_text(const char *text,size_t capacity,int multiline) {
    if(!text) return 0;
    size_t length=0;
    while(length<capacity && text[length]) {
        unsigned c=(unsigned char)text[length];if((c<32 && !(multiline && c=='\n')) || c==127) return 0;length++;
    }
    return length<capacity && desk_text_utf8_valid(text,length);
}
static int valid_frame(struct desk_rect r) { return r.x>=-16384 && r.x<=16384 && r.y>=-16384 && r.y<=16384 && r.w>=0 && r.w<=16384 && r.h>=0 && r.h<=16384; }
void desk_notice_init(struct desk_notice *n) { *n=(struct desk_notice){.body=&desk_font_16}; }
int desk_notice_set(struct desk_notice *n,const char *heading,const char *message,enum desk_notice_tone tone) {
    if(tone<DESK_NOTICE_INFO || tone>DESK_NOTICE_WARNING || !valid_text(heading,sizeof(n->heading),0) || !valid_text(message,sizeof(n->message),1)) return -1;
    /* Inputs may refer to the current snapshot. Copy both before publication. */
    char h[DESK_NOTICE_HEADING],m[DESK_NOTICE_MESSAGE];strcpy(h,heading);strcpy(m,message);
    strcpy(n->heading,h);strcpy(n->message,m);n->tone=tone;return 0;
}
void desk_notice_configure(struct desk_notice *n,struct desk_rect frame,const struct desk_font *body) {
    if(!valid_frame(frame)) return;
    n->frame=frame;n->body=body && body->line_height && body->line_height<=128?body:&desk_font_16;
}
void desk_notice_draw(struct desk_canvas *c,const struct desk_notice *n,const struct desk_theme *t) {
    struct desk_rect r=n->frame,old=c->clip;if(!r.w || !r.h) return;
    desk_clip(c,desk_intersect(old,r));desk_fill(c,r,t->border);desk_fill(c,desk_widget_inset(r,1),t->surface);
    uint32_t accent=n->tone==DESK_NOTICE_WARNING?0x8b6d36:n->tone==DESK_NOTICE_SUCCESS?t->accent:t->muted;
    desk_fill(c,(struct desk_rect){r.x+1,r.y+1,r.w>4?3:r.w-1,r.h>2?r.h-2:0},accent);
    desk_label_ellipsis(c,(struct desk_rect){r.x+14,r.y+10,r.w>28 && n->heading_trailing<(unsigned)(r.w-28)?r.w-28-(int)n->heading_trailing:0,26},t->heading,n->heading,t->text);
    desk_text_block_draw(c,(struct desk_rect){r.x+14,r.y+42,r.w>28?r.w-28:0,r.h>52 && n->message_bottom<(unsigned)(r.h-52)?r.h-52-(int)n->message_bottom:0},n->body,n->message,t->muted);
    c->clip=old;
}
void desk_progress_draw(struct desk_canvas *c,struct desk_rect frame,uint32_t value,uint32_t maximum,const struct desk_theme *t) {
    if(!valid_frame(frame) || !frame.w || !frame.h) return;
    struct desk_rect old=c->clip;desk_clip(c,desk_intersect(old,frame));desk_fill(c,frame,t->border);
    struct desk_rect inner=desk_widget_inset(frame,1);desk_fill(c,inner,t->track);
    if(value>maximum) value=maximum;
    if(maximum) { inner.w=(uint64_t)inner.w*value/maximum;desk_fill(c,inner,t->accent); }
    c->clip=old;
}
