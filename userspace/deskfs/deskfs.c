#include <srvros/desktop_file.h>
#include <srvros/sys.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

static unsigned char input[DESK_FILE_BYTES];
static _Alignas(struct desk_file_entry) unsigned char output[DESK_FILE_BYTES];
static int transfer(int fd,void *buffer,size_t length,int writing) {
    char *p=buffer;
    while(length) { long n=writing?srv_write(fd,p,length):srv_read(fd,p,length);if(n<=0) return -1;p+=n;length-=n; }
    return 0;
}
static unsigned read_file(const char *path,unsigned char *data,uint32_t *length) {
    int fd=open(path,O_RDONLY);if(fd<0) return DESK_FILE_IO;
    struct stat st;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<0 || (uint64_t)st.st_size>DESK_FILE_BYTES) {
        close(fd);return DESK_FILE_TOO_LARGE;
    }
    *length=0;
    while(*length<(uint32_t)st.st_size) {
        ssize_t n=read(fd,data+*length,(size_t)st.st_size-*length);
        if(n<=0) { close(fd);return DESK_FILE_IO; } *length+=n;
    }
    return close(fd)?DESK_FILE_IO:DESK_FILE_OK;
}
static void perform(const struct desk_file_request *q,struct desk_file_response *r) {
    *r=(struct desk_file_response){.magic=DESK_FILE_MAGIC,.id=q->id};
    if(q->operation==DESK_FILE_READ) r->status=read_file(q->path,output,&r->length);
    else if(q->operation==DESK_FILE_WRITE) {
        struct stat st;int exists=stat(q->path,&st)==0;
        if(!exists && errno!=ENOENT) { r->status=DESK_FILE_IO;r->crc=0;return; }
        if(exists && !(q->flags&DESK_FILE_EXPECT_EXISTING)) {
            uint32_t bytes=0;
            r->status=read_file(q->path,output,&bytes);
            if(!r->status) { r->existing_crc=desk_file_crc(output,bytes);r->status=DESK_FILE_EXISTS; }
        }
        else if(q->flags&DESK_FILE_EXPECT_EXISTING) {
            uint32_t bytes=0;
            if(!exists || read_file(q->path,output,&bytes)!=DESK_FILE_OK || desk_file_crc(output,bytes)!=q->expected_crc)
                r->status=DESK_FILE_CHANGED;
        }
        if(!r->status) {
            if(srv_fs_replace(q->path,input,q->length)!=(long)q->length || srv_sync()!=0) r->status=DESK_FILE_IO;
            else {
                uint32_t bytes=0;
                if(read_file(q->path,output,&bytes)!=DESK_FILE_OK || bytes!=q->length || memcmp(output,input,bytes)) r->status=DESK_FILE_IO;
            }
        }
    } else if(q->operation==DESK_FILE_MKDIR) {
        if(mkdir(q->path,0755)) r->status=DESK_FILE_IO;
        else if(srv_sync()!=0) r->status=DESK_FILE_IO;
    } else if(q->operation==DESK_FILE_LIST) {
        DIR *dir=opendir(q->path);
        if(!dir) r->status=DESK_FILE_IO;
        else {
            struct desk_file_entry *entries=(struct desk_file_entry *)output;
            struct dirent *entry;
            for(;;) {
                errno=0;entry=readdir(dir);
                if(!entry) { if(errno) r->status=DESK_FILE_IO;break; }
                if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;
                if(r->count==DESK_FILE_ENTRIES) { r->truncated=1;break; }
                char path[DESK_FILE_PATH];int n=snprintf(path,sizeof(path),"%s%s%s",q->path,!strcmp(q->path,"/")?"":"/",entry->d_name);
                if(n<0 || (size_t)n>=sizeof(path)) { r->truncated=1;continue; }
                struct stat st;if(stat(path,&st)) { r->status=DESK_FILE_IO;break; }
                struct desk_file_entry *out=&entries[r->count++];*out=(struct desk_file_entry){0};
                size_t len=strlen(entry->d_name);if(len>=sizeof(out->name)) { r->status=DESK_FILE_INVALID;break; }
                memcpy(out->name,entry->d_name,len);out->directory=S_ISDIR(st.st_mode);out->size=st.st_size;
            }
            if(closedir(dir)) r->status=DESK_FILE_IO;
            r->length=r->count*sizeof(*entries);
        }
    } else r->status=DESK_FILE_INVALID;
    if(r->status) r->length=r->count=0;
    r->crc=desk_file_crc(output,r->length);
}
int main(void) {
    srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE);
    for(;;) {
        struct desk_file_request q;
        if(transfer(0,&q,sizeof(q),0)) return 0;
        if(q.magic!=DESK_FILE_MAGIC || q.length>DESK_FILE_BYTES || q.path[0]!='/' ||
           !memchr(q.path,0,sizeof(q.path)) || q.operation<DESK_FILE_READ || q.operation>DESK_FILE_MKDIR ||
           (q.operation!=DESK_FILE_WRITE && q.length) || q.flags>DESK_FILE_EXPECT_EXISTING) return 1;
        if(q.length && transfer(0,input,q.length,0)) return 1;
        struct desk_file_response r;perform(&q,&r);
        if(transfer(1,&r,sizeof(r),1) || (r.length && transfer(1,output,r.length,1))) return 0;
    }
}
