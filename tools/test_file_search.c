#include <srvros/desk_file_search.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct { struct desk_search_header header;struct desk_search_entry entries[DESK_SEARCH_LIMIT]; } payload;
static struct desk_search_header header;
static struct desk_search_entry entries[DESK_SEARCH_LIMIT],before[DESK_SEARCH_LIMIT];
static size_t length(void) { return sizeof(payload.header)+payload.header.count*sizeof(*payload.entries); }
static void valid(void) {
    memset(&payload,0,sizeof(payload));payload.header=(struct desk_search_header){DESK_SEARCH_MAGIC,DESK_SEARCH_VERSION,2,0,10,2,0};
    payload.entries[0]=(struct desk_search_entry){.path="/fat/home/Report.txt",.inode=1,.size=42};
    payload.entries[1]=(struct desk_search_entry){.path="/fat/home/Reports",.inode=2,.directory=1};
}
static void rejects(void) { struct desk_search_header old=header;memcpy(before,entries,sizeof(before));assert(desk_file_search_decode(&header,entries,&payload,length(),"/fat/home","report",0)<0);assert(!memcmp(&old,&header,sizeof(old)) && !memcmp(before,entries,sizeof(before))); }
static void content_valid(void) {
    valid();payload.header.version=DESK_SEARCH_CONTENT_VERSION;payload.header.count=payload.header.matched=1;
    strcpy(payload.entries[0].path,"/fat/home/unrelated.txt");
}
static int content_decode(unsigned kind,unsigned mode,const char *query) {
    return desk_file_search_decode_mode(&header,entries,&payload,length(),"/fat/home",query,kind,mode);
}
static void content_rejects(unsigned kind,unsigned mode,const char *query) {
    struct desk_search_header old=header;memcpy(before,entries,sizeof(before));
    assert(content_decode(kind,mode,query)<0);
    assert(!memcmp(&old,&header,sizeof(old)) && !memcmp(before,entries,sizeof(before)));
}
int main(void) {
    const char *good[]={"/","/fat/home","/fat/café/x.txt","/fat/with space"};
    const char *bad[]={"","fat","//","/fat/","/fat//x","/fat/../x","/fat/./x","/fat/\nname","/fat/\xff"};
    for(unsigned i=0;i<sizeof(good)/sizeof(*good);i++) assert(desk_file_search_path_valid(good[i]));
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) assert(!desk_file_search_path_valid(bad[i]));
    assert(desk_file_search_under("/fat/home/file","/fat/home") && !desk_file_search_under("/fat/homework/file","/fat/home"));
    assert(desk_file_search_under("/fat/home","/") && desk_file_search_under("/fat/home","/fat/home"));
    assert(desk_file_search_matches("/a/Café Report.TXT","REPORT") && desk_file_search_matches("/a/Café Report.TXT","café"));
    assert(!desk_file_search_matches("/Report/no.txt","report") && !desk_file_search_matches("/a/report.txt","*") && !desk_file_search_matches("/a/Café","CAFÉ"));
    assert(!desk_file_search_query_valid("") && !desk_file_search_query_valid("a/b") && !desk_file_search_query_valid("x\t") && desk_file_search_query_valid("[*].é"));
    valid();assert(desk_file_search_decode(&header,entries,&payload,length(),"/fat/home","report",0)==2 && entries[0].size==42);
    valid();payload.header.magic++;rejects();valid();payload.header.flags=2;rejects();valid();payload.header.matched=1;rejects();valid();payload.header.matched=3;rejects();
    valid();payload.header.skipped=9;rejects();valid();payload.entries[1]=payload.entries[0];rejects();valid();strcpy(payload.entries[1].path,"/fat/homework/Reports");rejects();
    valid();strcpy(payload.entries[1].path,"/fat/home/no.txt");rejects();valid();payload.entries[1].reserved=1;rejects();valid();payload.entries[1].directory=2;rejects();valid();payload.entries[1].inode=0;rejects();
    valid();memset(payload.entries[1].path,'x',sizeof(payload.entries[1].path));rejects();
    valid();assert(desk_file_search_decode(&header,entries,&payload,length(),"/fat/home","report",1)<0);assert(desk_file_search_decode(&header,entries,&payload,length(),"/fat/home","report",2)<0);
    valid();payload.header.count=1;payload.header.matched=100;payload.header.scanned=200;payload.header.flags=DESK_SEARCH_TRUNCATED;payload.header.skipped=2;
    assert(desk_file_search_decode(&header,entries,&payload,length(),"/fat/home","report",1)==1 && header.flags==1);
    valid();payload.header.count=payload.header.matched=0;assert(!desk_file_search_decode(&header,entries,&payload,length(),"/fat/home","report",0));
    assert(desk_file_search_query_valid_mode("a/b",DESK_SEARCH_CONTENTS));
    assert(!desk_file_search_query_valid_mode("a/b",DESK_SEARCH_NAME));
    assert(!desk_file_search_query_valid_mode("x\n",DESK_SEARCH_CONTENTS));
    assert(!desk_file_search_query_valid_mode("\xff",DESK_SEARCH_CONTENTS));
    assert(!desk_file_search_query_valid_mode("ok",99));
    char long_query[DESK_SEARCH_QUERY+1];memset(long_query,'a',sizeof(long_query));long_query[DESK_SEARCH_QUERY-1]=0;
    assert(desk_file_search_query_valid_mode(long_query,DESK_SEARCH_CONTENTS));
    long_query[DESK_SEARCH_QUERY-1]='a';long_query[DESK_SEARCH_QUERY]=0;
    assert(!desk_file_search_query_valid_mode(long_query,DESK_SEARCH_CONTENTS));
    const char text[]={ 'A','/', 'b',0,'C','a','f', (char)0xc3,(char)0xa9 };
    assert(desk_file_search_text_matches(text,sizeof(text),"a/B"));
    assert(desk_file_search_text_matches(text,sizeof(text),"café"));
    assert(!desk_file_search_text_matches(text,sizeof(text),"CAFÉ"));
    assert(!desk_file_search_text_matches(text,2,"a/b"));
    assert(!desk_file_search_text_matches(text,sizeof(text),""));
    content_valid();assert(content_decode(1,DESK_SEARCH_CONTENTS,"a/b")==1);
    content_valid();content_rejects(0,DESK_SEARCH_CONTENTS,"a/b");
    content_valid();content_rejects(2,DESK_SEARCH_CONTENTS,"a/b");
    content_valid();content_rejects(3,DESK_SEARCH_CONTENTS,"a/b");
    content_valid();content_rejects(1,99,"a/b");
    content_valid();content_rejects(1,DESK_SEARCH_NAME,"unrelated");
    valid();content_rejects(1,DESK_SEARCH_CONTENTS,"report");
    content_valid();payload.entries[0].directory=1;content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();payload.entries[0].inode=0;content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();payload.entries[0].reserved=1;content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();payload.entries[0].size=DESK_SEARCH_CONTENT_FILE_BYTES;assert(content_decode(1,DESK_SEARCH_CONTENTS,"x")==1);
    content_valid();payload.entries[0].size=DESK_SEARCH_CONTENT_FILE_BYTES+1;content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();payload.header.flags=DESK_SEARCH_BYTE_LIMIT;assert(content_decode(1,DESK_SEARCH_CONTENTS,"x")==1);
    content_valid();payload.header.flags=8;content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();payload.header.scanned=DESK_SEARCH_CONTENT_SCAN_LIMIT;payload.header.flags=DESK_SEARCH_SCAN_LIMIT;assert(content_decode(1,DESK_SEARCH_CONTENTS,"x")==1);
    content_valid();payload.header.scanned=DESK_SEARCH_CONTENT_SCAN_LIMIT;content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();payload.header.flags=DESK_SEARCH_SCAN_LIMIT;content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();payload.header.scanned=DESK_SEARCH_CONTENT_SCAN_LIMIT+1;payload.header.flags=DESK_SEARCH_SCAN_LIMIT;content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();payload.header.matched=2;content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();payload.header.matched=2;payload.header.flags=DESK_SEARCH_TRUNCATED;assert(content_decode(1,DESK_SEARCH_CONTENTS,"x")==1);
    content_valid();strcpy(payload.entries[0].path,"/fat/homework/x");content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();payload.header.count=payload.header.matched=2;payload.entries[1]=payload.entries[0];content_rejects(1,DESK_SEARCH_CONTENTS,"x");
    content_valid();struct desk_search_header old=header;memcpy(before,entries,sizeof(before));
    assert(desk_file_search_decode_mode(&header,entries,&payload,length()-1,"/fat/home","x",1,DESK_SEARCH_CONTENTS)<0);
    assert(!memcmp(&old,&header,sizeof(old)) && !memcmp(before,entries,sizeof(before)));
    /* Independently check literal matching against a simple ASCII reference. */
    uint32_t rng=1;char name[64],query[32],lower[64],needle[32],path[80];
    for(unsigned round=0;round<10000;round++) {
        unsigned n=round%63,q=round%31;
        for(unsigned i=0;i<n;i++) { rng=rng*1664525u+1013904223u;name[i]="abAB.*[]"[rng%8];lower[i]=name[i]>='A' && name[i]<='Z'?name[i]+32:name[i]; }
        name[n]=lower[n]=0;
        for(unsigned i=0;i<q;i++) { rng=rng*1664525u+1013904223u;query[i]="abAB.*[]"[rng%8];needle[i]=query[i]>='A' && query[i]<='Z'?query[i]+32:query[i]; }
        query[q]=needle[q]=0;snprintf(path,sizeof(path),"/fat/%s",name);assert(desk_file_search_matches(path,query)==(q && strstr(lower,needle)!=NULL));
    }
    puts("file-search: PASS canonical UTF-8 paths, literal ASCII folding, root boundaries, 10000 reference matches, strict reply validation and atomic failure");
}
