#include <srvros/file_verify_kernel.h>
#include <srvros/syscall_numbers.h>
#include <srvros/vfs.h>
#include <srvros/exfat_stream.h>

static bool matches(const struct vfs_node *node,const struct srv_file_verify_request *q) {
    return node && node->type==VFS_NODE_FILE && node->metadata.inode==q->expected_inode &&
        node->size==q->length && node->metadata.mtime==q->expected_mtime;
}
int64_t file_verify(const char *path,const struct srv_file_verify_request *q,
    file_verify_copy_fn copy,void *context) {
    if(!path || !q || !copy || q->abi_version!=SRV_ABI_VERSION || q->struct_size!=sizeof(*q) ||
       q->reserved || !q->expected_inode || q->length>SRV_FILE_VERIFY_MAX || (q->length && !q->data))return SRV_ERR_INVAL;
    if(exfat_stream_busy(path,false))return SRV_ERR_BUSY;
    const struct vfs_node *node=vfs_lookup(path);
    if(!node)return SRV_ERR_NOENT;
    if(!matches(node,q))return SRV_ERR_AGAIN;
    const uint8_t *snapshot=NULL;uint64_t length=0;
    int64_t result=SRV_ERR_IO;
    /* Read directly from VFS, never content_acquire or a process descriptor.
     * Admission remains held across backend continuations and release. */
    if(!vfs_read_all(node,&snapshot,&length) || length!=q->length || (length && !snapshot))goto done;
    if(vfs_lookup(path)!=node || !matches(node,q)) {result=SRV_ERR_AGAIN;goto done;}
    uint8_t expected[SRV_FILE_VERIFY_CHUNK];
    for(uint64_t offset=0;offset<length;) {
        size_t count=length-offset>sizeof(expected)?sizeof(expected):(size_t)(length-offset);
        if(!copy(context,expected,offset,count)) {result=SRV_ERR_FAULT;goto done;}
        for(size_t i=0;i<count;i++)if(expected[i]!=snapshot[offset+i])goto done;
        offset+=count;
    }
    result=vfs_lookup(path)==node && matches(node,q)?0:SRV_ERR_AGAIN;
done:
    vfs_release_data(node,snapshot);
    return result;
}
