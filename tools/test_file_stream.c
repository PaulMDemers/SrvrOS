#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/file_stream.c"
static unsigned creates, opens, closes, appends;
static int close_failure,close_busy,backend_error;
static int64_t limit=65536;
static uint64_t current_size, last_write;
int64_t exfat_stream_create(const char *p,uint64_t m){(void)p;(void)m;creates++;return creates+1000;}
int64_t exfat_stream_open_read(const char *p){(void)p;opens++;return opens;}
int64_t exfat_stream_pread(uint64_t h,void *b,uint64_t n,uint64_t off){(void)h;(void)b;(void)off;return n;}
int64_t exfat_stream_append(uint64_t h,const void *b,uint64_t n){(void)h;(void)b;if(backend_error)return SRV_ERR_IO;if(n){appends++;last_write=n;}return n;}
int64_t exfat_stream_stat(uint64_t h,struct srv_file_stream_stat *s){(void)h;memset(s,0,sizeof(*s));s->size=current_size;return 0;}
int64_t exfat_stream_sync(uint64_t h){(void)h;return backend_error?SRV_ERR_IO:0;}
int64_t exfat_stream_close(uint64_t h){(void)h;closes++;if(close_busy)return SRV_ERR_BUSY;return close_failure?SRV_ERR_IO:0;}
int64_t process_stream_write_allowed(uint64_t p,uint64_t n){(void)p;if(n && !limit)return SRV_ERR_FBIG;return limit<(int64_t)n?limit:(int64_t)n;}
int main(void){
    char data[2]={0};uint64_t h[256];
    assert(file_stream_open(0,"x",false,0)==SRV_ERR_INVAL);
    assert(file_stream_open(1,"x",false,1)==SRV_ERR_INVAL);
    for(unsigned i=0;i<65;i++){int64_t t=file_stream_open(1,"x",true,0600);assert(t>0);h[i]=t;}
    assert(file_stream_open(1,"x",true,0600)==SRV_ERR_MFILE && creates==65);
    assert(file_stream_append(2,h[0],data,1)==SRV_ERR_BADF);
    assert(file_stream_pread(1,h[0],data,1,0)==SRV_ERR_BADF);
    assert(file_stream_append(1,h[0],data,65537)==SRV_ERR_INVAL);
    assert(file_stream_append(1,h[0],NULL,1)==SRV_ERR_INVAL);
    assert(file_stream_append(1,h[0],NULL,0)==0 && !appends);
    backend_error=1;assert(file_stream_append(1,h[0],NULL,0)==SRV_ERR_IO && !appends);
    assert(file_stream_sync(1,h[0])==SRV_ERR_IO);
    backend_error=0;assert(file_stream_sync(1,h[0])==0);
    limit=1;assert(file_stream_append_count(1,h[0],2)==1 && !appends);
    assert(file_stream_append(1,h[0],data,2)==1 && last_write==1);
    limit=0;assert(file_stream_append_count(1,h[0],2)==SRV_ERR_FBIG);
    assert(file_stream_append(1,h[0],data,2)==SRV_ERR_FBIG && appends==1);
    current_size=UINT64_MAX;assert(file_stream_append(1,h[0],data,1)==SRV_ERR_FBIG && appends==1);current_size=0;
    for(unsigned i=65;i<256;i++){int64_t t=file_stream_open(2+(i-65)/65,"x",false,0);assert(t>0);h[i]=t;}
    unsigned oldcreates=creates;assert(file_stream_open(99,"x",true,0)==SRV_ERR_MFILE && creates==oldcreates);
    assert(file_stream_pread(2,h[65],data,1,UINT64_MAX)==SRV_ERR_INVAL);
    assert(file_stream_append(2,h[65],data,1)==SRV_ERR_BADF);
    close_busy=1;assert(file_stream_close(1,h[0])==SRV_ERR_BUSY);
    assert(file_stream_sync(1,h[0])==0);
    assert(file_stream_open(1,"x",true,0600)==SRV_ERR_MFILE && creates==oldcreates);
    close_busy=0;assert(file_stream_sync(1,h[0])==0);
    assert(file_stream_close(1,h[0])==0);
    assert(file_stream_close(1,h[0])==SRV_ERR_BADF);
    close_failure=1;assert(file_stream_close(1,h[1])==SRV_ERR_IO);close_failure=0;
    assert(file_stream_close(1,h[1])==SRV_ERR_BADF);
    int64_t fresh=file_stream_open(99,"x",false,0);assert(fresh>0 && (uint64_t)fresh!=h[0]);
    assert(file_stream_stat(1,h[0],NULL)==SRV_ERR_BADF);
    close_failure=1;assert(!file_stream_cleanup(1));close_failure=0;
    assert(file_stream_sync(99,fresh)==0 && file_stream_cleanup(99));
    for(unsigned owner=2;owner<=4;owner++)assert(file_stream_cleanup(owner));
    next_stream_token=INT64_MAX;int64_t last=file_stream_open(10,"x",false,0);assert(last==INT64_MAX);
    assert(file_stream_open(10,"x",true,0)==SRV_ERR_OVERFLOW && creates==oldcreates);
    assert(file_stream_close(10,last)==0);
    puts("file stream manager: PASS");return 0;
}
