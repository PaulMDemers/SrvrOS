#define main worker_main
#include "../userspace/deskfind/worker.c"
#undef main
#include <assert.h>
#include <stdio.h>
static unsigned count=400;static int missing,list_failure,write_failure,stat_missing,long_path,write_calls;
static unsigned char captured[DESK_SEARCH_BYTES];static size_t captured_length;
long srv_stat(const char *path,struct srv_stat *s) {
    if(missing || (stat_missing && strstr(path,"item-002"))) return -1;
    *s=(struct srv_stat){.inode=2,.type=!strcmp(path,"/fat/home") || strstr(path,"folder")?1:0,.size=42,.mtime=1234};return 0;
}
long srv_list(uint64_t index,char *path,uint64_t capacity,uint64_t *size) {
    assert(capacity>=1024);*size=42;if(list_failure && index==3) return -1;if(index>=count) return 0;
    if(index==0) strcpy(path,"/fat/homework/item-outside");
    else if(index==1) strcpy(path,"/fat/home/folder-item");
    else if(index==2 && long_path) { strcpy(path,"/fat/home/");memset(path+10,'x',200);path[210]=0; }
    else snprintf(path,capacity,"/fat/home/sub/item-%03llu.txt",(unsigned long long)index);
    return 1;
}
long srv_write(int fd,const void *bytes,uint64_t length) {
    assert(fd==1);if(write_failure) return -1;if(++write_calls%3==0) return SRV_ERR_INTR;
    if(length>71) length=71;
    assert(captured_length+length<=sizeof(captured));memcpy(captured+captured_length,bytes,length);captured_length+=length;return length;
}
static char *args[]={"deskfind-worker","/fat/home","ITEM","0",NULL};
static void reset(void) { captured_length=0;write_calls=missing=list_failure=write_failure=stat_missing=long_path=0;count=400;args[3]="0"; }
static struct desk_search_header header;static struct desk_search_entry found[DESK_SEARCH_LIMIT];
static int decode(void) { return desk_file_search_decode(&header,found,captured,captured_length,args[1],args[2],args[3][0]-'0'); }
int main(void) {
    reset();assert(!worker_main(4,args));assert(decode()==256 && header.scanned==400 && header.matched==399 && header.flags==DESK_SEARCH_TRUNCATED && found[0].directory);
    reset();count=6;args[3]="1";assert(!worker_main(4,args) && decode()==4 && !header.flags && !found[0].directory);
    reset();args[3]="2";assert(!worker_main(4,args) && decode()==1 && found[0].directory);
    reset();count=4;long_path=1;assert(!worker_main(4,args) && decode()==2 && header.skipped==1);
    reset();count=4;stat_missing=1;assert(!worker_main(4,args) && decode()==2);
    reset();count=0;assert(!worker_main(4,args) && decode()==0 && !header.matched);
    reset();missing=1;assert(worker_main(4,args) && !captured_length);
    reset();list_failure=1;assert(worker_main(4,args) && !captured_length);
    reset();write_failure=1;assert(worker_main(4,args));
    reset();assert(worker_main(3,args));args[3]="9";assert(worker_main(4,args));args[3]="0";args[1]="/fat/../home";assert(worker_main(4,args));
    puts("find-worker: PASS root/kind filters, 256-match cap with total counts, long-path skipping, disappearing entries, partial/EINTR writes and failures");
}
