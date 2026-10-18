#include <srvros/desk_text.h>
#include <string.h>
static unsigned folded(unsigned c) { return c>='A' && c<='Z'?c+32:c; }
int desk_text_find(const struct desk_text_buffer *t,const char *query,size_t length,
    size_t from,int backwards,int match_case,struct desk_text_match *match) {
    if(!t || !match || !desk_text_utf8_valid(query,length)) return -1;
    if(!length || length>t->length) return 0;
    if(from>t->length) from=t->length;
    size_t first=SIZE_MAX,wrapped=SIZE_MAX;
    for(size_t at=0;at<=t->length-length;at=desk_text_next(t,at)) {
        size_t i=0;
        for(;i<length;i++) {
            unsigned a=(unsigned char)t->data[at+i],b=(unsigned char)query[i];
            if((match_case?a:folded(a))!=(match_case?b:folded(b))) break;
        }
        if(i!=length) continue;
        int primary=backwards?at<from:at>=from;
        size_t *slot=primary?&first:&wrapped;
        if(backwards || *slot==SIZE_MAX) *slot=at;
    }
    if(first==SIZE_MAX && wrapped==SIZE_MAX) return 0;
    match->wrapped=first==SIZE_MAX;match->start=match->wrapped?wrapped:first;match->end=match->start+length;return 1;
}

static int overlaps(const void *a,size_t an,const void *b,size_t bn) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return an && bn && (x<=y?y-x<an:x-y<bn);
}
static int matches_at(const struct desk_text_buffer *t,size_t at,const char *query,size_t length,int sensitive) {
    if(length>t->length-at) return 0;
    for(size_t i=0;i<length;i++) {
        unsigned a=(unsigned char)t->data[at+i],b=(unsigned char)query[i];
        if((sensitive?a:folded(a))!=(sensitive?b:folded(b))) return 0;
    }
    return 1;
}
int desk_text_replace(struct desk_text_buffer *t,const char *query,size_t qlen,
    const char *replacement,size_t rlen,int sensitive,int all,char *scratch,size_t capacity,size_t *matches) {
    if(matches) *matches=0;
    if(!t || !t->data || !t->capacity || !scratch || !capacity || !matches ||
       !desk_text_utf8_valid(query,qlen) || !desk_text_utf8_valid(replacement,rlen) ||
       overlaps(scratch,capacity,t,sizeof(*t)) || overlaps(scratch,capacity,t->data,t->capacity) ||
       overlaps(scratch,capacity,t->history_data,t->capacity) || overlaps(scratch,capacity,query,qlen) ||
       overlaps(scratch,capacity,replacement,rlen) || (t->journal &&
       (overlaps(scratch,capacity,t->journal,sizeof(*t->journal)) || overlaps(scratch,capacity,t->journal->data,t->journal->capacity)))) return -1;
    if(!qlen || qlen>t->length) return 0;
    size_t lo=t->anchor<t->caret?t->anchor:t->caret,hi=t->anchor>t->caret?t->anchor:t->caret;
    if(!all) {
        if(hi-lo!=qlen || !matches_at(t,lo,query,qlen,sensitive)) return 0;
        if(rlen>=capacity) return -1;
        if(rlen) memcpy(scratch,replacement,rlen);
        scratch[rlen]=0;
        int result=desk_text_buffer_replace_range(t,lo,hi,scratch,rlen);
        if(result>=0) *matches=1;
        return result;
    }
    size_t used=0,count=0;
    for(size_t at=0;at<t->length;) {
        int match=matches_at(t,at,query,qlen,sensitive);
        size_t end=match?at+qlen:desk_text_next(t,at),bytes=match?rlen:end-at;
        if(bytes>capacity-1-used || bytes>t->capacity-1-used) return -1;
        if(bytes) memcpy(scratch+used,match?replacement:t->data+at,bytes);
        used+=bytes;at=end;count+=match;
    }
    scratch[used]=0;
    if(!count) return 0;
    int result=desk_text_buffer_replace_range(t,0,t->length,scratch,used);
    if(result>=0) *matches=count;
    return result;
}
