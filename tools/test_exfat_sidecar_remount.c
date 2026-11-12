/* Real VFS/exFAT regression: import order differs from mutation/slot order. */
#define main sidecar_backend_main
#define console_printf sidecar_discard_console_printf
#include "exfat_host_driver.c"
#undef main
#undef console_printf
#include <stdarg.h>
void console_printf(const char *format,...) {
    if(strncmp(format,"exfat-sidecar",13))return;
    va_list args;va_start(args,format);
    for(const char *p=format;*p;p++) {
        if(*p!='%'){putchar(*p);continue;}
        p++;assert(*p=='u');printf("%llu",(unsigned long long)va_arg(args,uint64_t));
    }
    va_end(args);
}

static void snapshot(const char *phase) {
    for (uint64_t i=0;i<vfs_count();i++) {
        const struct vfs_node *n=vfs_node_at(i);
        if (strncmp(n->path,"/vol",4) || strstr(n->path,"/.srvros")) continue;
        const struct vfs_metadata *m=&n->metadata;
        printf("row %s %s %llu %llu %llu %llu %llu %llu %llu %llu\n",phase,n->path,
            (unsigned long long)m->inode,(unsigned long long)m->mode,
            (unsigned long long)m->nlink,(unsigned long long)m->uid,
            (unsigned long long)m->gid,(unsigned long long)m->atime,
            (unsigned long long)m->mtime,(unsigned long long)m->ctime);
    }
}
static void save_sidecar(const char *prefix,const char *phase) {
    char name[1024];assert(snprintf(name,sizeof(name),"%s-%s.meta",prefix,phase)>0);
    const struct vfs_node *node=vfs_lookup("/vol/.srvros/meta");
    const uint8_t *data;uint64_t size;assert(node && vfs_read_all(node,&data,&size));
    FILE *out=fopen(name,"wb");assert(out && fwrite(data,1,size,out)==size && !fclose(out));
    vfs_release_data(node,data);
}
int main(int argc,char **argv) {
    assert(argc==3);fd=open(argv[1],O_RDWR);assert(fd>=0);
    struct stat st;assert(!fstat(fd,&st) && S_ISREG(st.st_mode));
    struct block_device d={.used=true,.block_size=512,.block_count=st.st_size/512,
        .read=block_read,.write=block_write,.flush=flush};
    assert(exfat_mount_block_device("/vol",&d));
    assert(exfat_create_directory("/vol/one"));
    assert(exfat_create_directory("/vol/two"));
    assert(exfat_write_file("/vol/one/a",(const uint8_t *)"a",1));
    assert(exfat_write_file("/vol/two/b",(const uint8_t *)"b",1));
    assert(exfat_write_file("/vol/one/c",(const uint8_t *)"c",1));
    assert(exfat_write_file("/vol/z",(const uint8_t *)"z",1));
    /* A reused VFS hole must not become durable record ordering. */
    assert(exfat_delete_file("/vol/two/b"));
    assert(exfat_write_file("/vol/two/reused",(const uint8_t *)"kept",4));
    snapshot("before");save_sidecar(argv[2],"before");
    assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
    snapshot("imported");
    for (unsigned round=0;round<4;round++) {
        unsigned w=writes,f=flushes;uint64_t bytes=write_bytes;
        exfat_print_sidecar_equality_at(1000+round*2);
        assert(vfs_chmod("/vol/z",0644));
        exfat_print_sidecar_equality_at(1001+round*2);
        printf("noop round=%u writes=%u flushes=%u bytes=%llu\n",round,writes-w,flushes-f,
            (unsigned long long)(write_bytes-bytes));
        snapshot("synced");save_sidecar(argv[2],"after");
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
    }
    /* Genuine field changes must still take the ordinary persistence path. */
    mock_realtime+=2000000;
    const struct vfs_node *z=vfs_lookup("/vol/z");const uint8_t *data;uint64_t size;
    assert(vfs_read_all(z,&data,&size) && size==1);vfs_release_data(z,data);
    snapshot("read");
    unsigned w=writes;assert(vfs_chmod("/vol/z",0640) && writes>w+2);
    snapshot("changed");save_sidecar(argv[2],"changed");
    assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
    snapshot("restored");
    assert(exfat_rename("/vol/one/c","/vol/one/renamed"));
    save_sidecar(argv[2],"renamed");
    assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
    snapshot("renamed");
    equal("/vol/z",(const uint8_t *)"z",1);
    equal("/vol/one/a",(const uint8_t *)"a",1);
    equal("/vol/one/renamed",(const uint8_t *)"c",1);
    equal("/vol/two/reused",(const uint8_t *)"kept",4);
    assert(!vfs_lookup("/vol/one/c"));
    assert(exfat_unmount("/vol") && !vfs_count());
    assert(fsync(fd)==0);close(fd);return 0;
}
