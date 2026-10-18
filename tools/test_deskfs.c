#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#define main deskfs_guest_main
#include "../userspace/deskfs/deskfs.c"
#undef main
static int write_failure,sync_failure,writes;
long srv_fs_replace(const char *path,const void *data,size_t length) {
    writes++;if(write_failure) return -1;
    int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0600);if(fd<0) return -1;
    ssize_t n=write(fd,data,length);if(close(fd)) return -1;return n;
}
long srv_sync(void) { return sync_failure?-1:0; }
long srv_file_change(const struct srv_file_change *q) {
    struct stat st;
    if(stat(q->path,&st) || (uint64_t)st.st_ino!=q->expected_inode || (uint64_t)st.st_size!=q->expected_size ||
        (uint64_t)st.st_mtime!=q->expected_mtime || !!S_ISDIR(st.st_mode)!=!!q->expected_type) return SRV_FILE_CHANGED;
    if(q->operation!=SRV_FILE_DELETE && !stat(q->destination,&st)) return SRV_FILE_EXISTS;
    if(q->operation==SRV_FILE_DELETE) return q->expected_type?rmdir(q->path):unlink(q->path);
    if(q->operation==SRV_FILE_RENAME) return rename(q->path,q->destination);
    int fd=open(q->destination,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0) return -1;
    ssize_t n=write(fd,q->data,q->length);int result=close(fd);return n==(ssize_t)q->length && !result?0:-1;
}
int main(void) {
    char dir[]="/tmp/srvros-deskfs-test-XXXXXX";assert(mkdtemp(dir));
    struct desk_file_request q={.magic=DESK_FILE_MAGIC,.id=31,.operation=DESK_FILE_WRITE,.length=5};
    snprintf(q.path,sizeof(q.path),"%s/document",dir);memcpy(input,"hello",5);
    struct desk_file_response r;perform(&q,&r);
    assert(r.magic==DESK_FILE_MAGIC && r.id==31 && !r.status && writes==1 && !r.length);
    perform(&q,&r);assert(r.status==DESK_FILE_EXISTS && writes==1 && r.existing_crc==desk_file_crc("hello",5));
    q.flags=DESK_FILE_EXPECT_EXISTING;q.expected_crc=r.existing_crc;
    memcpy(input,"world",5);perform(&q,&r);assert(!r.status && writes==2);
    perform(&q,&r);assert(r.status==DESK_FILE_CHANGED && writes==2);
    q.operation=DESK_FILE_READ;q.length=0;perform(&q,&r);
    assert(!r.status && r.length==5 && !memcmp(output,"world",5) && r.crc==desk_file_crc(output,5));
    q.operation=DESK_FILE_WRITE;q.expected_crc=r.crc;q.length=0;perform(&q,&r);
    assert(!r.status && writes==3);q.operation=DESK_FILE_READ;perform(&q,&r);assert(!r.status && !r.length);
    q.operation=DESK_FILE_WRITE;q.expected_crc=0;q.length=5;write_failure=1;perform(&q,&r);assert(r.status==DESK_FILE_IO);
    write_failure=0;sync_failure=1;perform(&q,&r);assert(r.status==DESK_FILE_IO);sync_failure=0;
    q.operation=DESK_FILE_MKDIR;snprintf(q.path,sizeof(q.path),"%s/folder",dir);perform(&q,&r);assert(!r.status);
    q.operation=DESK_FILE_LIST;strcpy(q.path,dir);perform(&q,&r);
    assert(!r.status && r.count==2 && r.length==2*sizeof(struct desk_file_entry) && !r.truncated);
    struct desk_file_entry *entries=(void *)output;int file=0,folder=0;
    for(unsigned i=0;i<r.count;i++) {
        if(!strcmp(entries[i].name,"document")) { assert(!entries[i].directory && entries[i].size==5);file++; }
        if(!strcmp(entries[i].name,"folder")) { assert(entries[i].directory);folder++; }
    }
    assert(file==1 && folder==1);
    q.operation=DESK_FILE_READ;perform(&q,&r);assert(r.status==DESK_FILE_TOO_LARGE);
    snprintf(q.path,sizeof(q.path),"%s/document",dir);int fd=open(q.path,O_WRONLY);
    assert(fd>=0 && !ftruncate(fd,DESK_FILE_BYTES+1) && !close(fd));perform(&q,&r);assert(r.status==DESK_FILE_TOO_LARGE);
    assert(!unlink(q.path));perform(&q,&r);assert(r.status==DESK_FILE_NOT_FOUND);q.operation=DESK_FILE_WRITE;q.flags=DESK_FILE_EXPECT_EXISTING;
    perform(&q,&r);assert(r.status==DESK_FILE_CHANGED);
    /* Duplicate/rename/delete retain identity guards and verify the result. */
    snprintf(q.path,sizeof(q.path),"%s/source.txt",dir);fd=open(q.path,O_WRONLY|O_CREAT,0600);assert(fd>=0 && write(fd,"copy me",7)==7 && !close(fd));
    struct stat st;assert(!stat(q.path,&st));
    struct desk_file_change change={.inode=st.st_ino,.size=st.st_size,.modified=st.st_mtime};
    snprintf(change.destination,sizeof(change.destination),"%s/copy.txt",dir);memcpy(input,&change,sizeof(change));
    q.operation=DESK_FILE_COPY;q.length=sizeof(change);q.flags=0;perform(&q,&r);assert(!r.status);
    perform(&q,&r);assert(r.status==DESK_FILE_EXISTS);
    snprintf(change.destination,sizeof(change.destination),"%s/renamed.txt",dir);change.inode++;memcpy(input,&change,sizeof(change));
    q.operation=DESK_FILE_RENAME;perform(&q,&r);assert(r.status==DESK_FILE_CHANGED);change.inode--;memcpy(input,&change,sizeof(change));
    perform(&q,&r);assert(!r.status && stat(q.path,&st)<0);
    strcpy(q.path,change.destination);q.operation=DESK_FILE_DELETE;perform(&q,&r);assert(!r.status && stat(q.path,&st)<0);
    snprintf(q.path,sizeof(q.path),"%s/copy.txt",dir);assert(!unlink(q.path));
    snprintf(q.path,sizeof(q.path),"%s/folder",dir);assert(!rmdir(q.path));assert(!rmdir(dir));
    puts("deskfs: PASS create, guarded overwrite, changed/missing files, empty save, readback, write/sync failure, listing, mkdir, size limits, guarded duplicate/rename/delete");
}
