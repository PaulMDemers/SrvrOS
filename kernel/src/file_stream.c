#include <srvros/file_stream_kernel.h>
#include <srvros/exfat_stream.h>
#include <srvros/process.h>
#include <srvros/syscall_numbers.h>
#include <stddef.h>
#include <stdint.h>

/* All access occurs in the existing storage-service window. Tokens are opaque,
 * globally unique until exhaustion, and tied to one never-reused process instance. No file payload or
 * userspace pointer is retained. Fork therefore inherits no stream resources. */
struct file_stream_entry { uint64_t owner,token,backend;bool writing; };
static struct file_stream_entry streams[SRV_FILE_STREAM_GLOBAL_MAX];
static uint64_t next_stream_token=1;
static struct file_stream_entry *stream_find(uint64_t owner,uint64_t token) {
    if(!owner || !token)return NULL;
    for(unsigned i=0;i<SRV_FILE_STREAM_GLOBAL_MAX;i++)
        if(streams[i].owner==owner && streams[i].token==token)return &streams[i];
    return NULL;
}
int64_t file_stream_open(uint64_t owner,const char *path,bool create,uint64_t mode) {
    if(!owner || !path || (!create && mode) || (mode&~07777ull))return SRV_ERR_INVAL;
    struct file_stream_entry *free_slot=NULL;unsigned owned=0;
    for(unsigned i=0;i<SRV_FILE_STREAM_GLOBAL_MAX;i++) {
        if(streams[i].owner==owner)owned++;
        if(!streams[i].owner && !free_slot)free_slot=&streams[i];
    }
    if(!free_slot || owned>=SRV_FILE_STREAM_PROCESS_MAX)return SRV_ERR_MFILE;
    if(!next_stream_token || next_stream_token>INT64_MAX)return SRV_ERR_OVERFLOW;
    uint64_t token=next_stream_token++;
    *free_slot=(struct file_stream_entry){.owner=owner,.token=token,.writing=create};
    int64_t backend=create?exfat_stream_create(path,mode):exfat_stream_open_read(path);
    if(backend<=0) { *free_slot=(struct file_stream_entry){0};return backend<0?backend:SRV_ERR_IO; }
    free_slot->backend=(uint64_t)backend;return (int64_t)token;
}
int64_t file_stream_pread(uint64_t owner,uint64_t token,void *buffer,uint64_t length,uint64_t offset) {
    struct file_stream_entry *entry=stream_find(owner,token);
    if(!entry || entry->writing)return SRV_ERR_BADF;
    if(length>SRV_FILE_STREAM_CHUNK || (!buffer && length) || offset>UINT64_MAX-length)return SRV_ERR_INVAL;
    return exfat_stream_pread(entry->backend,buffer,length,offset);
}
int64_t file_stream_append_count(uint64_t owner,uint64_t token,uint64_t length) {
    struct file_stream_entry *entry=stream_find(owner,token);
    if(!entry || !entry->writing)return SRV_ERR_BADF;
    if(length>SRV_FILE_STREAM_CHUNK)return SRV_ERR_INVAL;
    /* Validate backend identity/sticky status even for an empty request. This
     * operation does not flush and is never a substitute for SYNC or CLOSE. */
    int64_t result=exfat_stream_append(entry->backend,NULL,0);if(result<0 || !length)return result;
    struct srv_file_stream_stat info;result=exfat_stream_stat(entry->backend,&info);
    if(result<0)return result;
    if(info.size>UINT64_MAX-length)return SRV_ERR_FBIG;
    return process_stream_write_allowed(info.size,length);
}
int64_t file_stream_append(uint64_t owner,uint64_t token,const void *buffer,uint64_t length) {
    if(!buffer && length)return SRV_ERR_INVAL;
    int64_t allowed=file_stream_append_count(owner,token,length);
    if(allowed<=0)return allowed;
    struct file_stream_entry *entry=stream_find(owner,token);
    return exfat_stream_append(entry->backend,buffer,(uint64_t)allowed);
}
int64_t file_stream_stat(uint64_t owner,uint64_t token,struct srv_file_stream_stat *info) {
    struct file_stream_entry *entry=stream_find(owner,token);
    return !entry?SRV_ERR_BADF:!info?SRV_ERR_INVAL:exfat_stream_stat(entry->backend,info);
}
int64_t file_stream_sync(uint64_t owner,uint64_t token) {
    struct file_stream_entry *entry=stream_find(owner,token);
    return entry?exfat_stream_sync(entry->backend):SRV_ERR_BADF;
}
int64_t file_stream_close(uint64_t owner,uint64_t token) {
    struct file_stream_entry *entry=stream_find(owner,token);
    if(!entry)return SRV_ERR_BADF;
    int64_t result=exfat_stream_close(entry->backend);
    /* A pre-admission refusal has not consumed the backend lease. Production
     * callers hold the service window, so foreign-owner refusal is excluded. */
    if(result==SRV_ERR_BUSY)return result;
    *entry=(struct file_stream_entry){0};return result;
}
bool file_stream_cleanup(uint64_t owner) {
    bool okay=true;
    for(unsigned i=0;i<SRV_FILE_STREAM_GLOBAL_MAX;i++)
        if(owner && streams[i].owner==owner && file_stream_close(owner,streams[i].token)<0)okay=false;
    return okay;
}
