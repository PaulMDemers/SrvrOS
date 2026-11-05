#define main worker_main
#include "../userspace/apps/deskfind/worker.c"
#undef main
#include <assert.h>
#include <stdio.h>
static unsigned count=400;static int missing,list_failure,write_failure,stat_missing,long_path,write_calls;
static int content_case,open_calls,close_calls,read_calls,fstat_calls,path_stats;
static size_t read_offset;static uint64_t actual_read_bytes;static unsigned char file_data[DESK_SEARCH_CONTENT_FILE_BYTES];static size_t file_size;
static unsigned char captured[DESK_SEARCH_BYTES];static size_t captured_length;
long srv_stat(const char *path,struct srv_stat *s) {
    if(missing || (stat_missing && strstr(path,"item-002"))) return -1;
    *s=(struct srv_stat){.inode=2,.type=!strcmp(path,"/fat/home") || strstr(path,"folder")?1:0,.size=content_case?file_size:42,.mtime=1234};
    if(content_case && strcmp(path,"/fat/home") && content_case==8 && ++path_stats%2==0) s->mtime++;
    return 0;
}
long srv_list(uint64_t index,char *path,size_t capacity,uint64_t *size) {
    assert(capacity>=1024);*size=42;if(list_failure && index==3) return -1;if(index>=count) return 0;
    if(content_case && content_case==10) strcpy(path,"/fat/homework/outside");
    else if(content_case) snprintf(path,capacity,"/fat/home/sub/file-%05llu.txt",(unsigned long long)index);
    else if(index==0) strcpy(path,"/fat/homework/item-outside");
    else if(index==1) strcpy(path,"/fat/home/folder-item");
    else if(index==2 && long_path) { strcpy(path,"/fat/home/");memset(path+10,'x',200);path[210]=0; }
    else snprintf(path,capacity,"/fat/home/sub/item-%03llu.txt",(unsigned long long)index);
    return 1;
}
long srv_open(const char *path) { (void)path;open_calls++;read_offset=0;return content_case==5?-1:7; }
long srv_close(int fd) { assert(fd==7);close_calls++;return 0; }
long srv_fstat(int fd,struct srv_stat *s) {
    assert(fd==7);fstat_calls++;*s=(struct srv_stat){.inode=2,.type=0,.size=file_size,.mtime=1234};
    if(content_case==6) return -1;
    if(content_case==7 && fstat_calls%2==0) s->inode++;
    return 0;
}
long srv_read(int fd,void *out,size_t n) {
    assert(fd==7);read_calls++;
    if(content_case==3 && read_calls%3==1) return SRV_ERR_INTR;
    if((content_case==4 || content_case==11) && read_offset) return -1;
    if(content_case==11 && n>4096) n=4096;
    if(n>3 && (content_case==3 || content_case==4)) n=3;
    if(n>file_size-read_offset) n=file_size-read_offset;
    memcpy(out,file_data+read_offset,n);read_offset+=n;actual_read_bytes+=n;return n;
}
long srv_write(int fd,const void *bytes,size_t length) {
    assert(fd==1);if(write_failure) return -1;if(++write_calls%3==0) return SRV_ERR_INTR;
    if(length>71) length=71;
    assert(captured_length+length<=sizeof(captured));memcpy(captured+captured_length,bytes,length);captured_length+=length;return length;
}
static char *args[]={"deskfind-worker","/fat/home","ITEM","0",NULL,NULL};
static void reset(void) { captured_length=0;write_calls=missing=list_failure=write_failure=stat_missing=long_path=0;count=400;args[1]="/fat/home";args[2]="ITEM";args[3]="0";args[4]=NULL;content_case=open_calls=close_calls=read_calls=fstat_calls=path_stats=0;file_size=0;actual_read_bytes=0; }
static struct desk_search_header header;static struct desk_search_entry found[DESK_SEARCH_LIMIT];
static int decode(void) { return desk_file_search_decode(&header,found,captured,captured_length,args[1],args[2],args[3][0]-'0'); }
static int decode_content(void) { return desk_file_search_decode_mode(&header,found,captured,captured_length,args[1],args[2],1,DESK_SEARCH_CONTENTS); }
static void setup_content(const char *data,size_t n) {
    reset();content_case=1;count=1;args[2]="needle";args[3]="1";args[4]="1";
    file_size=n;if(n<=sizeof(file_data)) memcpy(file_data,data,n);
}
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
    setup_content("prefix NeEdLe suffix",20);assert(!worker_main(5,args) && decode_content()==1 && !strcmp(found[0].path,"/fat/home/sub/file-00000.txt"));
    setup_content("nothing",7);assert(!worker_main(5,args) && decode_content()==0 && !header.skipped);
    setup_content("caf\xc3\xa9/NEEDLE",12);args[2]="\xc3\xa9/needle";assert(!worker_main(5,args) && decode_content()==1);
    setup_content("caf\xc3\xa9",5);args[2]="\xc3\x89";assert(!worker_main(5,args) && decode_content()==0);
    setup_content("needle\x7f",7);assert(!worker_main(5,args) && decode_content()==0 && header.skipped==1);
    setup_content("needle\0binary",13);assert(!worker_main(5,args) && decode_content()==0 && header.skipped==1);
    setup_content("needle\xc0\x80",8);assert(!worker_main(5,args) && decode_content()==0 && header.skipped==1);
    setup_content("",0);assert(!worker_main(5,args) && decode_content()==0 && !header.skipped);
    setup_content("x",DESK_SEARCH_CONTENT_FILE_BYTES+1);assert(!worker_main(5,args) && decode_content()==0 && header.skipped==1 && !open_calls);
    setup_content("xxNeEdLe\xc3\xa9",10);content_case=3;assert(!worker_main(5,args) && decode_content()==1 && read_calls>3 && close_calls==1);
    setup_content("needle",6);content_case=4;assert(!worker_main(5,args) && decode_content()==0 && header.skipped==1 && close_calls==1);
    for(int scenario=5;scenario<=8;scenario++) { setup_content("needle",6);content_case=scenario;assert(!worker_main(5,args) && decode_content()==0 && header.skipped==1 && close_calls==(scenario!=5)); }
    setup_content("needle",6);count=300;assert(!worker_main(5,args) && decode_content()==256 && header.matched==300 && header.flags==DESK_SEARCH_TRUNCATED);
    setup_content("",0);content_case=10;count=DESK_SEARCH_CONTENT_SCAN_LIMIT+1;assert(!worker_main(5,args) && decode_content()==0 && header.scanned==DESK_SEARCH_CONTENT_SCAN_LIMIT && header.flags==DESK_SEARCH_SCAN_LIMIT);
    setup_content("",0);file_size=sizeof(file_data);memset(file_data,'x',file_size);memcpy(file_data,"needle",6);count=300;
    assert(!worker_main(5,args) && decode_content()==256 && header.scanned==256 && header.matched==256 && header.flags==DESK_SEARCH_BYTE_LIMIT);
    setup_content("",0);file_size=sizeof(file_data)-1;memset(file_data,'x',file_size);memcpy(file_data,"needle",6);count=300;
    assert(!worker_main(5,args) && decode_content()==256 && header.scanned==257 && header.matched==256 && header.skipped==1 && header.flags==DESK_SEARCH_BYTE_LIMIT && open_calls==257 && close_calls==257 && actual_read_bytes==DESK_SEARCH_CONTENT_TOTAL_BYTES);
    setup_content("",0);file_size=sizeof(file_data);memset(file_data,'x',file_size);content_case=11;count=5000;
    assert(!worker_main(5,args) && decode_content()==0 && header.scanned==4096 && header.matched==0 && header.skipped==4096 && header.flags==DESK_SEARCH_BYTE_LIMIT && open_calls==4096 && close_calls==4096 && actual_read_bytes==DESK_SEARCH_CONTENT_TOTAL_BYTES);
    /* Reaching an exact ceiling deliberately remains conservative: the
     * worker does not enumerate beyond its budget to prove there is no tail. */
    setup_content("",0);content_case=10;count=DESK_SEARCH_CONTENT_SCAN_LIMIT;
    assert(!worker_main(5,args) && decode_content()==0 && header.flags==DESK_SEARCH_SCAN_LIMIT);
    setup_content("",0);file_size=sizeof(file_data);memset(file_data,'x',file_size);count=256;
    assert(!worker_main(5,args) && decode_content()==0 && header.flags==DESK_SEARCH_BYTE_LIMIT && actual_read_bytes==DESK_SEARCH_CONTENT_TOTAL_BYTES);
    setup_content("needle",6);args[3]="0";assert(worker_main(5,args));args[3]="1";args[4]="2";assert(worker_main(5,args));
    puts("find-worker: PASS root/kind filters, 256-match cap with total counts, long-path skipping, disappearing entries, partial/EINTR writes and failures");
}
