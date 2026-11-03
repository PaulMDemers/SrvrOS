/* Included by the exFAT import fixture: reuse its real backend and disposable
 * media, not a second mock filesystem. No process/global namespace is enabled. */
#include <srvros/vfs_namespace.h>

static void namespace_resolves(const struct vfs_namespace *ns,const char *path,const char *expected) {
    char out[VFS_NAMESPACE_PATH_MAX];
    assert(vfs_namespace_resolve(ns,path,out,sizeof(out)) && !strcmp(out,expected));
}
static void namespace_refuses(const struct vfs_namespace *ns,const char *path) {
    char out[VFS_NAMESPACE_PATH_MAX]="stale";
    assert(!vfs_namespace_resolve(ns,path,out,sizeof(out)) && !out[0]);
}
static void namespace_data(const struct vfs_namespace *ns,const char *path,const char *expected) {
    const struct vfs_node *node=vfs_namespace_lookup(ns,path);
    const uint8_t *data;uint64_t size;
    assert(node && vfs_read_all(node,&data,&size));
    assert(size==strlen(expected) && !memcmp(data,expected,(size_t)size));
    vfs_release_data(node,data);
}
static void namespace_case(const char *mode) {
    struct vfs_namespace ns;vfs_namespace_init(&ns);
    assert(vfs_register_directory("/"));
    fixture(1100,true);
    assert(exfat_mount_block_device("/fat",&device));
    assert(exfat_create_directory("/fat/home"));
    assert(exfat_create_directory("/fat/bin"));
    assert(exfat_create_directory("/fat/var"));
    assert(exfat_write_file("/fat/home/note",(const uint8_t *)"before",6));
    assert(exfat_write_file("/fat/bin/tool",(const uint8_t *)"executable",10));
    assert(vfs_namespace_bind(&ns,"/home","/fat/home"));
    assert(vfs_namespace_bind(&ns,"/bin","/fat/bin"));
    assert(vfs_namespace_bind(&ns,"/usr","/fat"));
    assert(vfs_namespace_bind(&ns,"/var","/fat/var"));
    unsigned before=writes;
    if(!strcmp(mode,"namespace-paths")) {
        namespace_resolves(&ns,"/","/");
        namespace_resolves(&ns,"/home","/fat/home");
        namespace_resolves(&ns,"/home/note","/fat/home/note");
        namespace_resolves(&ns,"/fat/home/note","/fat/home/note");
        namespace_resolves(&ns,"/home/new","/fat/home/new");
        namespace_resolves(&ns,"/home2","/home2");
        namespace_refuses(&ns,"/home2/note");
        const char *bad[]={NULL,"","relative","//home/note","/home/","/home//note",
            "/home/.","/home/./note","/home/..","/home/../source","/home/note/../new",
            "/home/note/child","/home/missing/child","/fat/home/../../source"};
        for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) namespace_refuses(&ns,bad[i]);
        assert(writes==before);
    } else if(!strcmp(mode,"namespace-bounds")) {
        char path[VFS_NAMESPACE_PATH_MAX+1],out[VFS_NAMESPACE_PATH_MAX+1];
        memset(path,'a',sizeof(path));memcpy(path,"/home/",6);
        path[VFS_NAMESPACE_PATH_MAX]=0;namespace_refuses(&ns,path);
        path[VFS_NAMESPACE_PATH_MAX-1]=0;namespace_refuses(&ns,path); /* expansion */
        path[VFS_NAMESPACE_PATH_MAX-1-4]=0; /* '/fat' expansion: exact bound */
        assert(vfs_namespace_resolve(&ns,path,out,sizeof(out)) && strlen(out)==159);
        out[0]='x';assert(!vfs_namespace_resolve(&ns,path,out,159) && !out[0]);
        out[0]='x';assert(!vfs_namespace_resolve(&ns,"/",out,1) && !out[0]);
        assert(!vfs_namespace_resolve(&ns,"/",NULL,1));
        assert(!vfs_namespace_resolve(NULL,"/",out,sizeof(out)) && !out[0]);
        assert(!vfs_namespace_resolve(&ns,"/",out,0));
        assert(writes==before);
    } else if(!strcmp(mode,"namespace-bindings")) {
        unsigned count=ns.count;
        const char *sources[]={"/","/home","/home/nested","/fat","/chain","/bad/../x","/missing/alias","/fat/source/alias"};
        const char *targets[]={"/fat","/fat/home","/fat/home","/fat/home","/home","/fat","/fat/home","/fat/home"};
        for(unsigned i=0;i<sizeof(sources)/sizeof(sources[0]);i++)
            assert(!vfs_namespace_bind(&ns,sources[i],targets[i]) && ns.count==count);
        assert(!vfs_namespace_bind(&ns,"/absent","/missing"));
        assert(!vfs_namespace_bind(&ns,"/file","/fat/source"));
        assert(vfs_register_directory("/etc"));
        assert(!vfs_namespace_bind(&ns,"/etc","/fat/var"));
        assert(vfs_register_directory("/hidden/child"));
        assert(!vfs_namespace_bind(&ns,"/hidden","/fat/var"));
        char source[32]="/copy",target[32]="/fat/home";
        assert(vfs_namespace_bind(&ns,source,target));source[1]='X';target[1]='X';
        namespace_resolves(&ns,"/copy/note","/fat/home/note");
        static const char *more[]={"/a","/b","/c"};
        for(unsigned i=0;i<3;i++) assert(vfs_namespace_bind(&ns,more[i],"/fat/home"));
        assert(ns.count==VFS_NAMESPACE_MAX_MAPPINGS && !vfs_namespace_bind(&ns,"/full","/fat/home"));
        assert(writes==before);
        assert(vfs_unregister_path("/etc") && vfs_unregister_path("/hidden/child"));
    } else if(!strcmp(mode,"namespace-lifecycle")) {
        assert(vfs_register_directory("/attach"));
        assert(vfs_namespace_bind(&ns,"/attach/view","/fat/home"));
        namespace_data(&ns,"/attach/view/note","before");
        assert(vfs_unregister_path("/attach"));
        namespace_refuses(&ns,"/attach/view/note");
        assert(vfs_register_directory("/attach"));
        namespace_data(&ns,"/attach/view/note","before");
        assert(vfs_register_directory("/home"));
        namespace_refuses(&ns,"/home/note");
        assert(vfs_unregister_path("/home"));
        assert(vfs_register_directory("/home/new-child"));
        namespace_refuses(&ns,"/home/note");
        assert(vfs_unregister_path("/home/new-child"));
        namespace_data(&ns,"/home/note","before");
        assert(vfs_unregister_path("/attach"));
        assert(writes==before);
    } else if(!strcmp(mode,"namespace-identity")) {
        const struct vfs_node *node=vfs_lookup("/fat/bin/tool");uint64_t count=vfs_count();
        assert(node==vfs_namespace_lookup(&ns,"/bin/tool") && node==vfs_namespace_lookup(&ns,"/usr/bin/tool"));
        assert(node==vfs_namespace_lookup(&ns,"/fat/bin/tool") && vfs_count()==count);
        assert(!vfs_lookup("/bin/tool")); /* explicitly scoped; no global alias */
        namespace_data(&ns,"/bin/tool","executable");namespace_data(&ns,"/usr/bin/tool","executable");
        char path[160];assert(vfs_namespace_resolve(&ns,"/home/note",path,sizeof(path)));
        assert(vfs_chmod(path,0600));
        assert((vfs_namespace_lookup(&ns,"/home/note")->metadata.mode&0777)==0600);
        assert(vfs_namespace_lookup(&ns,"/home/note")==vfs_lookup("/fat/home/note"));
    } else if(!strcmp(mode,"namespace-persistence")) {
        char path[160];assert(vfs_namespace_resolve(&ns,"/home/note",path,sizeof(path)));
        assert(exfat_write_file(path,(const uint8_t *)"new contents",12));
        namespace_data(&ns,"/home/note","new contents");namespace_data(&ns,"/fat/home/note","new contents");
        assert(vfs_chmod(path,0600));assert(vfs_utime(path,11,12));
        assert(vfs_namespace_resolve(&ns,"/home/created",path,sizeof(path)));
        assert(exfat_write_file_mode(path,(const uint8_t *)"created",7,0640));
        assert(exfat_unmount("/fat"));
        assert(!vfs_namespace_lookup(&ns,"/home/note"));namespace_refuses(&ns,"/home/new");
        assert(exfat_mount_block_device("/fat",&device));
        namespace_data(&ns,"/home/note","new contents");namespace_data(&ns,"/fat/home/created","created");
        const struct vfs_node *node=vfs_namespace_lookup(&ns,"/home/note");
        assert(node==vfs_lookup("/fat/home/note") && (node->metadata.mode&0777)==0600 && node->metadata.mtime==12);
        assert((vfs_namespace_lookup(&ns,"/home/created")->metadata.mode&0777)==0640);
    } else if(!strcmp(mode,"namespace-rename")) {
        char from[160],to[160];
        assert(vfs_namespace_resolve(&ns,"/home/note",from,sizeof(from)));
        assert(vfs_namespace_resolve(&ns,"/usr/home/moved",to,sizeof(to)));
        assert(exfat_same_mount(from,to) && exfat_rename(from,to));
        assert(!vfs_namespace_lookup(&ns,"/home/note") && !vfs_lookup("/fat/home/note"));
        namespace_data(&ns,"/home/moved","before");namespace_data(&ns,"/fat/home/moved","before");
        assert(exfat_unmount("/fat") && exfat_mount_block_device("/fat",&device));
        namespace_data(&ns,"/home/moved","before");
        assert(exfat_delete_file(to));assert(!vfs_namespace_lookup(&ns,"/home/moved"));
    } else if(!strcmp(mode,"namespace-failure")) {
        char path[160];assert(vfs_namespace_resolve(&ns,"/home/note",path,sizeof(path)));
        fail_write_offset=106; /* real backend dirty-marker failure */
        assert(!exfat_write_file(path,(const uint8_t *)"changed",7));
        assert(!exfat_is_writable("/fat"));
        namespace_data(&ns,"/home/note","before");namespace_data(&ns,"/fat/home/note","before");
        assert(!exfat_write_file(path,(const uint8_t *)"retry",5));
    } else if(!strcmp(mode,"namespace-workspace")) {
        assert(exfat_unmount("/fat") && exfat_mount_block_device("/work",&device));
        assert(exfat_create_directory("/work/srvros-work"));
        assert(exfat_write_file("/work/srvros-work/keep",(const uint8_t *)"workspace",9));
        namespace_refuses(&ns,"/home/note");
        vfs_namespace_init(&ns);
        assert(vfs_namespace_bind(&ns,"/home","/work/srvros-work"));
        namespace_resolves(&ns,"/home/keep","/work/srvros-work/keep");
        namespace_data(&ns,"/home/keep","workspace");namespace_data(&ns,"/work/srvros-work/keep","workspace");
        assert(exfat_unmount("/work") && exfat_mount_block_device("/work",&device));
        namespace_data(&ns,"/home/keep","workspace");check_data("/work/source",1100);
        assert(exfat_unmount("/work"));
        assert(vfs_unregister_path("/") && !vfs_count() && !io_depth);return;
    } else assert(!"unknown namespace case");
    check_data("/fat/source",1100); /* unchanged pre-existing user data */
    assert(exfat_unmount("/fat"));
    assert(vfs_unregister_path("/") && !vfs_count() && !io_depth);
}
