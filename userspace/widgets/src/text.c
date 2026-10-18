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
int desk_text_buffer_history(struct desk_text_buffer *t,struct desk_text_history *j,char *data,size_t capacity) {
    if(!t || !j || !data || !t->capacity || t->capacity-1>SIZE_MAX/2 || capacity<2*(t->capacity-1) ||
       overlap(j,sizeof(*j),t,sizeof(*t)) || overlap(j,sizeof(*j),t->data,t->capacity) ||
       overlap(j,sizeof(*j),t->history_data,t->capacity) || overlap(j,sizeof(*j),data,capacity) ||
       overlap(data,capacity,t,sizeof(*t)) || overlap(data,capacity,t->data,t->capacity) ||
       overlap(data,capacity,t->history_data,t->capacity)) return -1;
    *j=(struct desk_text_history){.data=data,.capacity=capacity};t->journal=j;t->history=0;return 0;
}
int desk_text_buffer_can_undo(const struct desk_text_buffer *t,int redo) {
    return t && (t->history&(redo?2u:1u))!=0;
}
static void journal_flags(struct desk_text_buffer *t) {
    const struct desk_text_history *j=t->journal;
    t->history=(j->applied?1u:0u)|(j->applied<j->count?2u:0u);
}
static void journal_record(struct desk_text_buffer *t,size_t lo,size_t hi,const char *data,size_t length) {
    struct desk_text_history *j=t->journal;size_t bytes=hi-lo+length;
    /* A new branch discards only unapplied records. Eviction removes a prefix,
     * keeping every retained record relative to the current document lineage. */
    if(j->applied<j->count) { j->used=j->changes[j->applied].offset;j->count=j->applied; }
    unsigned drop=0;size_t removed=0;
    while(j->count-drop>=DESK_TEXT_HISTORY_LIMIT || bytes>j->capacity-(j->used-removed)) {
        const struct desk_text_change *c=&j->changes[drop++];removed=c->offset+c->removed+c->inserted;
    }
    if(drop) {
        memmove(j->data,j->data+removed,j->used-removed);j->used-=removed;
        j->count-=drop;j->applied-=drop;
        memmove(j->changes,j->changes+drop,j->count*sizeof(*j->changes));
        for(unsigned i=0;i<j->count;i++) j->changes[i].offset-=removed;
    }
    struct desk_text_change *c=&j->changes[j->count++];
    *c=(struct desk_text_change){j->used,lo,hi-lo,length,t->caret,t->anchor};
    if(hi>lo) memcpy(j->data+j->used,t->data+lo,hi-lo);
    if(length) memcpy(j->data+j->used+hi-lo,data,length);
    j->used+=bytes;j->applied=j->count;journal_flags(t);
}
int desk_text_buffer_set(struct desk_text_buffer *t,const char *data,size_t length) {
    if(length>=t->capacity || !desk_text_utf8_valid(data,length)) return -1;
    if(length) memmove(t->data,data,length);
    t->data[length]=0; t->length=length;
    t->caret=t->anchor=0; t->history=0; t->revision++;
    if(t->journal) t->journal->used=t->journal->count=t->journal->applied=0;
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
    if(t->journal && (overlap(data,length,t->journal->data,t->journal->capacity) ||
       overlap(data,length,t->journal,sizeof(*t->journal)))) return -1;
    if(t->journal) journal_record(t,lo,hi,data,length);else snapshot(t);
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
    if(r>0) {
        if(t->journal) {
            struct desk_text_change *c=&t->journal->changes[t->journal->applied-1];c->anchor=anchor;c->caret=caret;
        } else { t->history_anchor=anchor; t->history_caret=caret; }
    }
    return r;
}
int desk_text_buffer_undo(struct desk_text_buffer *t,int redo) {
    if(!desk_text_buffer_can_undo(t,redo)) return 0;
    if(t->journal) {
        struct desk_text_history *j=t->journal;
        const struct desk_text_change *c=&j->changes[redo?j->applied:j->applied-1];
        size_t remove=redo?c->removed:c->inserted,insert=redo?c->inserted:c->removed;
        const char *source=j->data+c->offset+(redo?c->removed:0);
        memmove(t->data+c->position+insert,t->data+c->position+remove,t->length-c->position-remove+1);
        if(insert) memcpy(t->data+c->position,source,insert);
        t->length=t->length-remove+insert;
        if(redo) { j->applied++;t->anchor=t->caret=c->position+insert; }
        else { j->applied--;t->anchor=c->anchor;t->caret=c->caret; }
        t->revision++;journal_flags(t);return 1;
    }
    char *data=t->data; t->data=t->history_data; t->history_data=data;
    size_t n=t->length; t->length=t->history_length; t->history_length=n;
    n=t->caret; t->caret=t->history_caret; t->history_caret=n;
    n=t->anchor; t->anchor=t->history_anchor; t->history_anchor=n;
    t->history=redo?1:2; t->revision++; return 1;
}
