#define _POSIX_C_SOURCE 200809L
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

bool test_process_busy(const char *path) {
    return unused_process_has_open_vfs_prefix(path);
}
#undef process_has_open_vfs_prefix
bool process_has_open_vfs_prefix(const char *path);
#define SRVROS_WORKSPACE_HOST_TEST
#include "../kernel/src/workspace.c"

static unsigned test_disables;
bool usb_storage_disable_volume_writes(struct block_device *device) {
    test_disables++;device->write=NULL;device->flush=NULL;return true;
}
void test_workspace_counts(void);
void test_workspace_arm_clean(void);
void test_workspace_clean_unchanged(void);
void test_workspace_exercise(struct block_device *device,bool close_first,bool fault,bool barrier,bool clean) {
    /* Admission/UUID selection has its own suite. Adopt the real mounted fixture
     * here to isolate failure/retirement without mocking writeback or sync. */
    active=device;failed=false;
    if(barrier) {
        assert(!workspace_sync() && failed && exfat_is_writable("/work"));
        assert(!workspace_sync() && !workspace_unmount() && !test_disables);
        assert(active==device && vfs_lookup("/work/source"));
        active=NULL;return;
    }
    const struct vfs_node *node=vfs_lookup("/work/source");
    const uint8_t *data;uint64_t size;
    assert(node && vfs_read_all(node,&data,&size));
    struct process_file_content *content=content_acquire(node,node->path,data,size);
    assert(content);content->refs++; /* independent retained reader */
    write_files[0]=(struct process_write_file){.used=true,.refs=1,.content=content};
    read_files[0]=(struct process_read_file){.used=true,.refs=1,.content=content};
    assert(ensure_write_capacity(&write_files[0],13));
    memcpy((void *)content->data,"workspace-new",13);content->size=13;content->dirty=true;
    assert(!workspace_unmount() && !failed && active==device && !test_disables);
    if(clean) {
        test_workspace_arm_clean();
        assert(!workspace_sync() && failed && content->dirty && !content->failed);
        assert(exfat_is_writable("/work") && !memcmp(content->data,"workspace-new",13));
        test_workspace_clean_unchanged();
        /* Explicit content retry remains valid after a pre-mutation refusal,
         * but does not silently reset the managed workspace failure latch. */
        assert(process_file_sync_all()==0 && !content->dirty && !content->failed);
        assert(write_file_close(1)==0 && read_file_close(1)==0);
        assert(!workspace_sync() && !workspace_unmount() && active==device && !test_disables);
        active=NULL;return;
    }
    int64_t result=close_first?write_file_close(1):process_file_sync_all();
    test_workspace_counts();
    if(fault) {
        assert(result<0 && content->dirty && content->failed);
        assert(!memcmp(content->data,"workspace-new",13) && !exfat_is_writable("/work"));
        assert(process_file_sync_all()<0 && !workspace_sync() && failed);
        if(!close_first) assert(write_file_close(1)==SRV_ERR_IO);
        assert(content->refs==1 && content->dirty && content->failed);
        assert(!workspace_unmount() && active==device && !test_disables);
        assert(read_file_close(1)==SRV_ERR_IO && !content_find("/work/source"));
        assert(!test_process_busy("/work"));
        assert(!workspace_sync() && !workspace_unmount() && active==device && !test_disables);
        /* No retry/repair or clean-retirement claim after the last close. The
         * close error is observable; the unreferenced RAM buffer is released. */
        assert(vfs_lookup("/work/source"));active=NULL;
    } else {
        assert(result==0 && !content->dirty && !content->failed);
        if(!close_first) assert(write_file_close(1)==0);
        assert(read_file_close(1)==0 && workspace_unmount() && !active && test_disables==1);
    }
}

#include "test_process_salvage.inc"
