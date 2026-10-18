#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/vfs.c"
static int64_t wall=-1;
static unsigned notifications;
static const struct initramfs_file boot_files[]={
    {"/etc/motd",(const uint8_t *)"welcome",7},{"/share/test/note",(const uint8_t *)"note",4},
    {"/init",(const uint8_t *)"init",4},{"/init/invalid",NULL,0},{"relative",NULL,0},{"/invalid/../x",NULL,0},{"/double//x",NULL,0}};
uint64_t initramfs_file_count(void) { return sizeof(boot_files)/sizeof(*boot_files); }
const struct initramfs_file *initramfs_file_at(uint64_t index) { return &boot_files[index]; }
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
    vfs_init();assert(vfs_count()==7);
    const char *folders[]={"/","/etc","/share","/share/test"};
    for(unsigned i=0;i<4;i++) { struct vfs_metadata m;uint64_t type,size;
        assert(vfs_stat(folders[i],&m,&size,&type) && type==VFS_NODE_DIRECTORY && m.inode && !size && !m.mtime);
    }
    assert(!vfs_lookup("/init/invalid") && !vfs_lookup("relative") && !vfs_lookup("/invalid") && !vfs_lookup("/double"));
    const struct vfs_node *motd=vfs_lookup("/etc/motd");const uint8_t *bytes;uint64_t length;
    assert(motd && vfs_read_all(motd,&bytes,&length) && length==7 && !memcmp(bytes,"welcome",7));
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
    puts("vfs-time: PASS boot root/parent directories, conflicting paths, UTC create/read/write/change timestamps, explicit utime, imported metadata and unavailable-clock sentinel");
}
