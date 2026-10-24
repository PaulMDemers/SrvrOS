#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../kernel/src/vfs.c"
static bool fail_alloc;
void *kcalloc(size_t n,size_t size) { return fail_alloc?NULL:calloc(n,size); }
void kfree(void *p) { free(p); }
static int64_t wall=-1;
static unsigned notifications;
static const struct initramfs_file boot_files[]={
    {"/etc/motd",(const uint8_t *)"welcome",7,0640},{"/share/test/note",(const uint8_t *)"note",4,0600},
    {"/init",(const uint8_t *)"init",4,0755},{"/init/invalid",NULL,0,0},{"relative",NULL,0,0},
    {"/invalid/../x",NULL,0,0},{"/double//x",NULL,0,0}};
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
    assert((vfs_lookup("/init")->metadata.mode&0777)==0755 && (vfs_lookup("/etc/motd")->metadata.mode&0777)==0640);
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
    assert(vfs_utime("/test",SRV_TIME_OMIT,SRV_TIME_NOW));
    assert(file->metadata.atime==1000 && file->metadata.mtime==1792326900 && notifications==4);
    assert(vfs_utime("/test",SRV_TIME_OMIT,SRV_TIME_OMIT) && notifications==4);
    assert(vfs_utime("/test",1000,2000));
    struct vfs_metadata orphan=file->metadata;
    vfs_metadata_utime(&orphan,SRV_TIME_NOW,SRV_TIME_OMIT);
    assert(orphan.atime==1792326900 && orphan.mtime==2000);
    struct vfs_metadata persisted=file->metadata;
    assert(vfs_register_file_with_metadata("/imported",3,NULL,read_test,NULL,&persisted));
    assert(vfs_lookup("/imported")->metadata.mtime==2000);
    wall=-1;assert(vfs_register_directory("/unknown"));assert(!vfs_lookup("/unknown")->metadata.ctime);
    /* Persisted IDs can collide with files imported from Linux or with another
     * persisted record. Attributes survive; aliases must never reach a picker. */
    vfs_set_metadata_changed_callback(NULL);
    struct vfs_metadata saved = file->metadata;
    saved.inode = early->metadata.inode;
    assert(vfs_set_metadata("/test", &saved));
    assert(file->metadata.inode != early->metadata.inode);
    assert(file->metadata.mode == saved.mode && file->metadata.mtime == saved.mtime);
    assert(vfs_register_directory_with_metadata("/collision-dir", &saved));
    assert(vfs_register_file_with_metadata("/collision-file",3,NULL,read_test,NULL,&saved));
    assert(vfs_register_directory_with_metadata("/collision-dir", &saved));
    assert(vfs_register_file_with_metadata("/collision-file",3,NULL,read_test,NULL,&saved));
    saved.inode = 100000;
    assert(vfs_set_metadata("/test", &saved) && file->metadata.inode == 100000);
    assert(vfs_set_metadata("/test", &saved) && file->metadata.inode == 100000);
    assert(vfs_register_directory("/after-import"));
    assert(vfs_lookup("/after-import")->metadata.inode > 100000);
    saved.inode = UINT64_MAX;
    assert(vfs_set_metadata("/test", &saved));
    assert(vfs_register_directory("/after-wrap"));
    saved.inode = 0;assert(vfs_set_metadata("/test", &saved));
    for(uint64_t i=0;i<vfs_count();i++) {
        const struct vfs_node *a=vfs_node_at(i);assert(a && a->metadata.inode);
        for(uint64_t j=0;j<i;j++) assert(a->metadata.inode!=vfs_node_at(j)->metadata.inode);
    }
    /* Cross the previous 4096-node ceiling without moving referenced nodes. */
    static char names[6000][32];
    const struct vfs_node *retained=file;
    uint64_t old_count=vfs_count();
    for(unsigned i=0;i<6000;i++) {
        snprintf(names[i],sizeof(names[i]),"/scale/%u",i);
        assert(vfs_register_file(names[i],3,NULL,read_test));
    }
    assert(vfs_lookup("/test")==retained && vfs_count()==old_count+6000);
    for(uint64_t i=0;i<vfs_count();i++) assert(vfs_node_at(i) && vfs_node_at(i)->used);
    assert(!vfs_node_at(vfs_count()));
    for(unsigned i=0;i<6000;i+=2) assert(vfs_unregister_path(names[i]));
    for(unsigned i=0;i<6000;i+=2) assert(vfs_register_file(names[i],3,NULL,read_test));
    assert(vfs_lookup("/test")==retained);
    fail_alloc=true;assert(!vfs_reserve_nodes(vfs_free_nodes()));
    assert(vfs_lookup("/test")==retained && vfs_count()==old_count+6000);
    fail_alloc=false;assert(!vfs_reserve_nodes(UINT64_MAX));
    assert(vfs_unregister_prefix("/scale")==6000 && vfs_count()==old_count);
    assert(vfs_lookup("/test")==retained);
    puts("vfs-time: PASS boot root/parent directories, conflicting paths, UTC create/read/write/change timestamps, explicit utime, imported metadata and unavailable-clock sentinel");
}
