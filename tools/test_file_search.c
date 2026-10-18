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
