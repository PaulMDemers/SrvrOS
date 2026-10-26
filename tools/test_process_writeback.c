/* Link real process content writeback to the import fixture's real exFAT/VFS.
 * The fixture owns teardown admission; this bridge exercises retained content. */
#include <assert.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#define process_has_open_vfs_prefix unused_process_has_open_vfs_prefix
#include "../kernel/src/process.c"

void test_process_writeback(const char *path) {
    const struct vfs_node *node=vfs_lookup(path);
    const uint8_t *data=NULL;uint64_t size=0;
    assert(node && vfs_read_all(node,&data,&size));
    struct process_file_content *content=content_acquire(node,path,data,size);
    assert(content);
    struct process_write_file write={.content=content};
    assert(ensure_write_capacity(&write,3));
    memcpy((void *)content->data,"new",3);content->size=3;content->dirty=true;
    assert(content_flush(content)==SRV_ERR_IO);
    assert(content->dirty && !content->failed && !memcmp(content->data,"new",3));
    assert(content_flush(content)==SRV_ERR_IO && !content->failed);
    assert(exfat_delete_file("/vol/filler"));
    assert(content_flush(content)==0 && !content->dirty && !content->failed);
    assert(content_release(content)==0 && !content_find(path));
}
