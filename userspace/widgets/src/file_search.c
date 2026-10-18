#include <srvros/desk_file_search.h>
#include <srvros/desk_text.h>
#include <string.h>
int desk_file_search_path_valid(const char *path) {
    if(!path || path[0]!='/') return 0;
    size_t n=strlen(path);if(n>=DESK_SEARCH_PATH || !desk_text_utf8_valid(path,n) || (n>1 && path[n-1]=='/')) return 0;
    const char *component=path+1;
    for(const char *p=path+1;;p++) {
        unsigned c=(unsigned char)*p;
        if(!c || c=='/') {
            size_t length=p-component;
            if((!length && n!=1) || (length==1 && component[0]=='.') || (length==2 && component[0]=='.' && component[1]=='.')) return 0;
            if(!c) break;
            component=p+1;
        } else if(c<32 || c==127) return 0;
    }
    return 1;
}
int desk_file_search_query_valid(const char *query) {
    if(!query) return 0;
    size_t n=strlen(query);
    if(!n || n>=DESK_SEARCH_QUERY || !desk_text_utf8_valid(query,n)) return 0;
    for(const unsigned char *p=(const unsigned char *)query;*p;p++) if(*p<32 || *p==127 || *p=='/') return 0;
    return 1;
}
int desk_file_search_under(const char *path,const char *root) {
    size_t n=strlen(root);return !strcmp(root,"/")?path[0]=='/':!strncmp(path,root,n) && (!path[n] || path[n]=='/');
}
static unsigned folded(unsigned c) { return c>='A' && c<='Z'?c+32:c; }
int desk_file_search_matches(const char *path,const char *query) {
    const char *name=strrchr(path,'/');name=name?name+1:path;
    size_t n=strlen(query),length=strlen(name);if(!n || n>length) return 0;
    for(size_t i=0;i<=length-n;i++) {
        size_t j=0;for(;j<n && folded((unsigned char)name[i+j])==folded((unsigned char)query[j]);j++);
        if(j==n) return 1;
    }
    return 0;
}
int desk_file_search_decode(struct desk_search_header *header,struct desk_search_entry *entries,
    const void *data,size_t length,const char *root,const char *query,unsigned kind) {
    if(!header || !entries || !data || length<sizeof(*header) || !desk_file_search_path_valid(root) || !desk_file_search_query_valid(query) || kind>2) return -1;
    struct desk_search_header h;memcpy(&h,data,sizeof(h));
    if(h.magic!=DESK_SEARCH_MAGIC || h.version!=DESK_SEARCH_VERSION || h.count>DESK_SEARCH_LIMIT || (h.flags&~DESK_SEARCH_TRUNCATED) ||
       h.matched<h.count || h.scanned<h.matched || h.skipped>h.scanned-h.matched || (h.matched>h.count)!=(!!(h.flags&DESK_SEARCH_TRUNCATED)) ||
       length!=sizeof(h)+h.count*sizeof(*entries)) return -1;
    const unsigned char *bytes=(const unsigned char *)data+sizeof(h);
    for(unsigned i=0;i<h.count;i++) {
        struct desk_search_entry e;memcpy(&e,bytes+i*sizeof(e),sizeof(e));
        if(!memchr(e.path,0,sizeof(e.path)) || !desk_file_search_path_valid(e.path) || !desk_file_search_under(e.path,root) ||
           !desk_file_search_matches(e.path,query) || e.reserved || !e.inode || e.directory>1 ||
           (kind==1 && e.directory) || (kind==2 && !e.directory)) return -1;
        for(unsigned j=0;j<i;j++) { struct desk_search_entry previous;memcpy(&previous,bytes+j*sizeof(e),sizeof(e));if(!strcmp(e.path,previous.path)) return -1; }
    }
    memmove(entries,bytes,h.count*sizeof(*entries));*header=h;return (int)h.count;
}
