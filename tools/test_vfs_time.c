#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/vfs.c"
static int64_t wall=-1;
static unsigned notifications;
int64_t timer_realtime_us(void) { return wall; }
void console_write(const char *s) { (void)s; }
void console_printf(const char *fmt,...) { (void)fmt; }
static bool read_test(const struct vfs_node *n,const uint8_t **data,uint64_t *size) {
    (void)n;*data=(const uint8_t *)"abc";*size=3;return true;
}
static void changed(const char *path,const struct vfs_metadata *m) {
    assert(!strcmp(path,"/test") && m->mtime<=1792326900);notifications++;
}
int main(void) {
    assert(vfs_register_file("/early",3,NULL,read_test));
    const struct vfs_node *early=vfs_lookup("/early");assert(early && !early->metadata.mtime);
    wall=1792326896123456LL;assert(vfs_register_file("/test",3,NULL,read_test));
    const struct vfs_node *file=vfs_lookup("/test");assert(file);
    assert(file->metadata.atime==1792326896 && file->metadata.mtime==1792326896 && file->metadata.ctime==1792326896);
    vfs_set_metadata_changed_callback(changed);
    wall+=1000000;const uint8_t *data;uint64_t size;
    assert(vfs_read_all(file,&data,&size));assert(file->metadata.atime==1792326897 && file->metadata.mtime==1792326896);
    wall+=1000000;assert(vfs_update_file_size("/test",8));
    assert(file->metadata.mtime==1792326898 && file->metadata.ctime==1792326898 && notifications==1);
    wall+=1000000;assert(vfs_chmod("/test",0600));assert(file->metadata.ctime==1792326899 && file->metadata.mtime==1792326898);
    wall+=1000000;assert(vfs_utime("/test",1000,2000));
    assert(file->metadata.atime==1000 && file->metadata.mtime==2000 && file->metadata.ctime==1792326900 && notifications==3);
    struct vfs_metadata persisted=file->metadata;
    assert(vfs_register_file_with_metadata("/imported",3,NULL,read_test,NULL,&persisted));
    assert(vfs_lookup("/imported")->metadata.mtime==2000);
    wall=-1;assert(vfs_register_directory("/unknown"));assert(!vfs_lookup("/unknown")->metadata.ctime);
    puts("vfs-time: PASS UTC create/read/write/change timestamps, explicit utime, imported metadata and unavailable-clock sentinel");
}
