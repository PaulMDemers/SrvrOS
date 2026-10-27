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

_Static_assert(DESK_FILE_COPY_BYTES==SRV_FILE_DUPLICATE_MAX,"worker/kernel duplicate limit");
static unsigned char input[DESK_FILE_BYTES];
static _Alignas(struct desk_file_entry) unsigned char output[DESK_FILE_BYTES];
static int transfer(int fd,void *buffer,size_t length,int writing) {
    char *p=buffer;
    while(length) { long n=writing?srv_write(fd,p,length):srv_read(fd,p,length);if(n<=0) return -1;p+=n;length-=n; }
    return 0;
}
static unsigned read_file(const char *path,unsigned char *data,uint32_t *length) {
    struct stat st;
    if(stat(path,&st)) return errno==ENOENT?DESK_FILE_NOT_FOUND:DESK_FILE_IO;
    /* open currently folds read failures into ENOENT. An existing directory
     * entry must never be mistaken for a missing file after a data I/O error. */
    int fd=open(path,O_RDONLY);if(fd<0) return DESK_FILE_IO;
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
/* Copies verify bounded streams, independently of the editor's 64 KiB buffer.
 * Opening a source fd takes a stable read snapshot; close it before invoking the
 * guarded namespace operation, which rejects open-file conflicts. */
static unsigned file_checksum(const char *path,uint64_t *length,uint32_t *checksum) {
    struct stat st;if(stat(path,&st)) return errno==ENOENT?DESK_FILE_NOT_FOUND:DESK_FILE_IO;
    if(!S_ISREG(st.st_mode) || st.st_size<0 || (uint64_t)st.st_size>DESK_FILE_COPY_BYTES) return DESK_FILE_TOO_LARGE;
    int fd=open(path,O_RDONLY);if(fd<0) return DESK_FILE_IO;
    unsigned result=DESK_FILE_OK;uint64_t total=0;uint32_t crc=~0u;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<0 || (uint64_t)st.st_size>DESK_FILE_COPY_BYTES) result=DESK_FILE_IO;
    while(!result && total<(uint64_t)st.st_size) {
        size_t bytes=(uint64_t)st.st_size-total;if(bytes>sizeof(output)) bytes=sizeof(output);
        ssize_t n=read(fd,output,bytes);
        if(n<=0 || (size_t)n>bytes) result=DESK_FILE_IO;
        else { crc=desk_file_crc_update(crc,output,(uint32_t)n);total+=n; }
    }
    if(close(fd)) result=DESK_FILE_IO;
    if(!result) { *length=total;*checksum=~crc; }return result;
}
static unsigned change_file(const struct desk_file_request *q) {
    struct desk_file_change change;memcpy(&change,input,sizeof(change));
    if(!change.inode || change.directory>1 || change.reserved || !memchr(change.destination,0,sizeof(change.destination)) ||
        (q->operation!=DESK_FILE_DELETE && change.destination[0]!='/')) return DESK_FILE_INVALID;
    struct srv_file_change request={.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(request),
        .operation=q->operation==DESK_FILE_RENAME?SRV_FILE_RENAME:q->operation==DESK_FILE_DELETE?SRV_FILE_DELETE:SRV_FILE_DUPLICATE,
        .expected_inode=change.inode,.expected_size=change.size,.expected_mtime=change.modified,.expected_type=change.directory,
        .path=q->path,.destination=change.destination};
    uint32_t expected_crc=0;
    if(q->operation==DESK_FILE_COPY) {
        if(change.directory || change.size>DESK_FILE_COPY_BYTES) return DESK_FILE_TOO_LARGE;
        uint64_t length=0;unsigned status=file_checksum(q->path,&length,&expected_crc);if(status) return status;
        if(length!=change.size) return DESK_FILE_CHANGED;
    }
    long result=srv_file_change(&request);
    if(result==SRV_FILE_CHANGED) return DESK_FILE_CHANGED;
    if(result==SRV_FILE_EXISTS) return DESK_FILE_EXISTS;
    if(result==SRV_FILE_BUSY) return DESK_FILE_BUSY;
    if(result==SRV_FILE_LIMIT) return DESK_FILE_TOO_LARGE;
    if(result==SRV_FILE_INVALID) return DESK_FILE_INVALID;
    if(result || srv_sync()) return DESK_FILE_IO;
    struct stat st;
    if(q->operation==DESK_FILE_DELETE) return stat(q->path,&st)<0 && errno==ENOENT?DESK_FILE_OK:DESK_FILE_IO;
    if(stat(change.destination,&st) || (uint64_t)st.st_size!=change.size || !!S_ISDIR(st.st_mode)!=!!change.directory) return DESK_FILE_IO;
    if(q->operation==DESK_FILE_RENAME && !(stat(q->path,&st)<0 && errno==ENOENT)) return DESK_FILE_IO;
    if(q->operation==DESK_FILE_COPY) {
        uint32_t crc=0;uint64_t length=0;
        if(file_checksum(change.destination,&length,&crc) || length!=change.size || crc!=expected_crc) return DESK_FILE_IO;
    }
    return DESK_FILE_OK;
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
        int fd=open(q->path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        if(fd<0)r->status=DESK_FILE_IO;
        else {
            struct desk_file_entry *entries=(struct desk_file_entry *)output;
            struct srv_dirent_info batch[SRV_DIRECTORY_BATCH_MAX];int done=0;
            while(!done) {
                long count=srv_directory(SRV_DIRECTORY_READ_INFO,fd,batch,sizeof(batch));
                if(count<0 || count>SRV_DIRECTORY_BATCH_MAX){r->status=DESK_FILE_IO;break;}
                if(!count)break;
                for(long i=0;i<count;i++) {
                    const struct srv_dirent_info *info=&batch[i];const struct srv_dirent *entry=&info->entry;
                    if(!memchr(entry->name,0,sizeof(entry->name)) || entry->type>1){r->status=DESK_FILE_IO;done=1;break;}
                    if(!strcmp(entry->name,".") || !strcmp(entry->name,".."))continue;
                    if(r->count==DESK_FILE_ENTRIES){r->truncated=1;done=1;break;}
                    char path[DESK_FILE_PATH];int n=snprintf(path,sizeof(path),"%s%s%s",q->path,!strcmp(q->path,"/")?"":"/",entry->name);
                    if(n<0 || (size_t)n>=sizeof(path)){r->truncated=1;continue;}
                    struct desk_file_entry *out=&entries[r->count++];*out=(struct desk_file_entry){0};
                    size_t len=strlen(entry->name);if(len>=sizeof(out->name)){r->status=DESK_FILE_INVALID;done=1;break;}
                    memcpy(out->name,entry->name,len);out->directory=entry->type==1;out->size=info->size;out->inode=entry->inode;out->modified=info->modified;
                }
            }
            if(close(fd))r->status=DESK_FILE_IO;
            r->length=r->count*sizeof(*entries);
        }
    } else if(q->operation==DESK_FILE_STAT) {
        struct desk_file_probe probe;memcpy(&probe,input,sizeof(probe));struct stat st;
        if(stat(q->path,&st)) r->status=errno==ENOENT?DESK_FILE_NOT_FOUND:DESK_FILE_IO;
        else if(probe.inode && probe.inode!=(uint64_t)st.st_ino) r->status=DESK_FILE_CHANGED;
        else if(st.st_size<0) r->status=DESK_FILE_INVALID;
        else {
            struct desk_file_info info={.inode=st.st_ino,.size=st.st_size,.modified=st.st_mtime,
                .kind=S_ISREG(st.st_mode)?DESK_FILE_KIND_FILE:S_ISDIR(st.st_mode)?DESK_FILE_KIND_FOLDER:DESK_FILE_KIND_OTHER,
                .permissions=st.st_mode&0777};
            memcpy(output,&info,sizeof(info));r->length=sizeof(info);
        }
    } else if(q->operation>=DESK_FILE_RENAME && q->operation<=DESK_FILE_COPY) r->status=change_file(q);
    else r->status=DESK_FILE_INVALID;
    if(r->status) r->length=r->count=0;
    r->crc=desk_file_crc(output,r->length);
}
int main(void) {
    srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE);
    for(;;) {
        struct desk_file_request q;
        if(transfer(0,&q,sizeof(q),0)) return 0;
        if(q.magic!=DESK_FILE_MAGIC || q.length>DESK_FILE_BYTES || q.path[0]!='/' ||
           !memchr(q.path,0,sizeof(q.path)) || !desk_file_payload_valid(q.operation,q.length,q.flags)) return 1;
        if(q.length && transfer(0,input,q.length,0)) return 1;
        struct desk_file_response r;perform(&q,&r);
        if(transfer(1,&r,sizeof(r),1) || (r.length && transfer(1,output,r.length,1))) return 0;
    }
}
