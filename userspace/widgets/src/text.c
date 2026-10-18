#include <srvros/desk_text.h>
#include <string.h>

int desk_text_utf8_valid(const char *s,size_t length) {
    if (!s && length) return 0;
    for (size_t i=0;i<length;) {
        unsigned char c=s[i++];
        if (!c) return 0;
        if (c<128) continue;
        unsigned n; uint32_t cp,min;
        if(c>=0xc2 && c<=0xdf) { n=1; cp=c&31; min=0x80; }
        else if(c>=0xe0 && c<=0xef) { n=2; cp=c&15; min=0x800; }
        else if(c>=0xf0 && c<=0xf4) { n=3; cp=c&7; min=0x10000; }
        else return 0;
        if(n>length-i) return 0;
        for(unsigned j=0;j<n;j++) {
            c=s[i++]; if((c&0xc0)!=0x80) return 0;
            cp=(cp<<6)|(c&63);
        }
        if(cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return 0;
    }
    return 1;
}
static size_t boundary(const struct desk_text_buffer *t,size_t p) {
    if(p>t->length) p=t->length;
    while(p && ((unsigned char)t->data[p]&0xc0)==0x80) p--;
    return p;
}
static int overlap(const void *a,size_t an,const void *b,size_t bn) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return an && bn && (x<=y?y-x<an:x-y<bn);
}
int desk_text_buffer_init(struct desk_text_buffer *t,char *buffer,char *history,size_t capacity) {
    if(!t || !buffer || !history || !capacity || overlap(buffer,capacity,history,capacity)) return -1;
    *t=(struct desk_text_buffer){.data=buffer,.history_data=history,.capacity=capacity};
    buffer[0]=history[0]=0; return 0;
}
int desk_text_buffer_set(struct desk_text_buffer *t,const char *data,size_t length) {
    if(length>=t->capacity || !desk_text_utf8_valid(data,length)) return -1;
    if(length) memmove(t->data,data,length);
    t->data[length]=0; t->length=length;
    t->caret=t->anchor=0; t->history=0; t->revision++;
    return 1;
}
static void snapshot(struct desk_text_buffer *t) {
    memcpy(t->history_data,t->data,t->length+1);
    t->history_length=t->length; t->history_caret=t->caret; t->history_anchor=t->anchor;
    t->history=1;
}
int desk_text_buffer_insert(struct desk_text_buffer *t,const char *data,size_t length) {
    size_t lo=t->caret<t->anchor?t->caret:t->anchor, hi=t->caret>t->anchor?t->caret:t->anchor;
    if(length>t->capacity-1-(t->length-(hi-lo)) || !desk_text_utf8_valid(data,length)) return -1;
    if(!length && lo==hi) return 0;
    /* Insertion sources must be external: a pointer into either owned buffer
     * would be invalidated by snapshot/memmove. Reject rather than corrupt. */
    if(overlap(data,length,t->data,t->capacity) || overlap(data,length,t->history_data,t->capacity)) return -1;
    snapshot(t);
    memmove(t->data+lo+length,t->data+hi,t->length-hi+1);
    if(length) memcpy(t->data+lo,data,length);
    t->length=t->length-(hi-lo)+length; t->caret=t->anchor=lo+length; t->revision++;
    return 1;
}
void desk_text_buffer_select(struct desk_text_buffer *t,size_t anchor,size_t caret) {
    t->anchor=boundary(t,anchor); t->caret=boundary(t,caret);
}
size_t desk_text_prev(const struct desk_text_buffer *t,size_t position) {
    size_t p=boundary(t,position); return p?boundary(t,p-1):0;
}
size_t desk_text_next(const struct desk_text_buffer *t,size_t position) {
    size_t p=boundary(t,position); if(p==t->length) return p;
    do { p++; } while(p<t->length && ((unsigned char)t->data[p]&0xc0)==0x80);
    return p;
}
size_t desk_text_line_start(const struct desk_text_buffer *t,size_t position) {
    size_t p=boundary(t,position);
    while(p && t->data[p-1]!='\n') p--;
    return p;
}
size_t desk_text_line_end(const struct desk_text_buffer *t,size_t position) {
    size_t p=boundary(t,position);
    while(p<t->length && t->data[p]!='\n') p++;
    return p;
}
size_t desk_text_line_count(const struct desk_text_buffer *t) {
    size_t lines=1; for(size_t i=0;i<t->length;i++) lines+=t->data[i]=='\n'; return lines;
}
int desk_text_buffer_erase(struct desk_text_buffer *t,int backwards) {
    size_t anchor=t->anchor,caret=t->caret;
    if(anchor==caret) t->anchor=backwards?desk_text_prev(t,caret):desk_text_next(t,caret);
    int r=desk_text_buffer_insert(t,"",0);
    if(r>0) { t->history_anchor=anchor; t->history_caret=caret; }
    return r;
}
int desk_text_buffer_undo(struct desk_text_buffer *t,int redo) {
    if(t->history!=(redo?2u:1u)) return 0;
    char *data=t->data; t->data=t->history_data; t->history_data=data;
    size_t n=t->length; t->length=t->history_length; t->history_length=n;
    n=t->caret; t->caret=t->history_caret; t->history_caret=n;
    n=t->anchor; t->anchor=t->history_anchor; t->history_anchor=n;
    t->history=redo?1:2; t->revision++; return 1;
}
