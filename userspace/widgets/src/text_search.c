#include <srvros/desk_text.h>
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
