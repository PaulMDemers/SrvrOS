/* Offline staging image adapter using the actual kernel exFAT/VFS code. */
#define main exfat_fixture_driver_main
#include "exfat_host_driver.c"
#undef main
#include <srvros/exfat_stream.h>
#include <strings.h>

static bool bootstrap_placeholder(const struct vfs_node *node, const uint8_t **data, uint64_t *size) {
    (void)node; (void)data; (void)size;
    return false;
}

static bool document_path(const char *path) {
    if(strncasecmp(path,"/fat/home/",10))return false;
    const char *subpath=strchr(path+10,'/');
    return subpath && !strncasecmp(subpath,"/Documents/",11);
}

/* Audit the entire profile namespace, including case-folded scratch names. */
static bool stream_audit(bool fresh) {
    const char *roots[]={"/fat/bin","/fat/var","/fat/home"};
    for(size_t i=0;i<sizeof(roots)/sizeof(roots[0]);i++) {
        const struct vfs_node *n=vfs_lookup(roots[i]);
        if(!n || n->type!=VFS_NODE_DIRECTORY) { fputs("profile image driver: missing installed directory\n",stderr);return false; }
    }
    struct exfat_check_result check={0};
    if(!exfat_check("/fat",&check) || check.errors || check.leaked_clusters || check.fat_bitmap_mismatches) {
        fputs("profile image driver: allocation audit failed\n",stderr);return false;
    }
    for(uint64_t i=0;i<vfs_count();i++) {
        const struct vfs_node *n=vfs_node_at(i);
        if(!n || !n->used || strncasecmp(n->path,"/fat/home",9) || (n->path[9] && n->path[9]!='/'))continue;
        if(n->type!=VFS_NODE_FILE && n->type!=VFS_NODE_DIRECTORY) {
            fputs("profile image driver: unsupported profile namespace entry\n",stderr);
            return false;
        }
        const char *leaf=strrchr(n->path,'/');leaf=leaf?leaf+1:n->path;
        if(fresh && !strncasecmp(n->path,"/fat/home/",10)) {
            const char *first=strchr(n->path+10,'/');
            if(first && first==leaf-1 && (!strcasecmp(leaf,"identity.json") ||
               !strcasecmp(leaf,"profile.id") || !strcasecmp(leaf,"profile-local.json"))) {
                fputs("profile image driver: base already contains a profile\n",stderr);return false;
            }
        }
        char folded[SRV_PATH_MAX];size_t len=strlen(leaf);
        if(len>=sizeof(folded))return false;
        for(size_t k=0;k<=len;k++)folded[k]=(leaf[k]>='A' && leaf[k]<='Z') ? (char)(leaf[k]+('a'-'A')) : leaf[k];
        if(!document_path(n->path) && (strstr(folded,".profile-import") ||
            !strncmp(folded,".profile-copy-temp",18) || !strncmp(folded,".desktop-profile.tmp",20))) {
            fputs("profile image driver: pending profile marker, journal, or scratch\n",stderr);return false;
        }
        for(uint64_t j=0;j<i;j++) {
            const struct vfs_node *other=vfs_node_at(j);
            if(other && other->used && !strcasecmp(n->path,other->path)) {
                fputs("profile image driver: profile namespace alias\n",stderr);return false;
            }
        }
    }
    return true;
}

static bool stream_write(const char *path,int input,const struct stat *original) {
    uint8_t payload[32768],verify[32768];
    struct srv_file_stream_stat info={0};struct stat after;
    int64_t h=-1,r;bool ok=false;uint64_t offset=0,size=(uint64_t)original->st_size;
    h=exfat_stream_create(path,0600);
    if(h<0) { fprintf(stderr,"profile image driver: stream create failed (%lld; capacity or I/O)\n",(long long)h);goto done; }
    while(offset<size) {
        size_t count=(size-offset>sizeof(payload))?sizeof(payload):(size_t)(size-offset);
        ssize_t n=pread(input,payload,count,(off_t)offset);
        if(n!=(ssize_t)count) { fputs("profile image driver: payload read failed\n",stderr);goto done; }
        r=exfat_stream_append((uint64_t)h,payload,count);
        if(r!=(int64_t)count) { fprintf(stderr,"profile image driver: stream append failed (%lld; capacity or I/O)\n",(long long)r);goto done; }
        offset+=count;
    }
    if(exfat_stream_stat((uint64_t)h,&info)<0 || info.size!=size || exfat_stream_sync((uint64_t)h)<0) {
        fputs("profile image driver: stream size or sync failed\n",stderr);goto done;
    }
    r=exfat_stream_close((uint64_t)h);h=-1;
    if(r<0) { fputs("profile image driver: stream close failed\n",stderr);goto done; }
    h=exfat_stream_open_read(path);
    if(h<0 || exfat_stream_stat((uint64_t)h,&info)<0 || info.size!=size) {
        fputs("profile image driver: stream reopen or size failed\n",stderr);goto done;
    }
    for(offset=0;offset<size;) {
        size_t count=(size-offset>sizeof(payload))?sizeof(payload):(size_t)(size-offset);
        if(pread(input,payload,count,(off_t)offset)!=(ssize_t)count ||
           exfat_stream_pread((uint64_t)h,verify,count,offset)!=(int64_t)count || memcmp(payload,verify,count)) {
            fputs("profile image driver: stream readback mismatch or I/O failure\n",stderr);goto done;
        }
        offset+=count;
    }
    if(fstat(input,&after) || after.st_dev!=original->st_dev || after.st_ino!=original->st_ino ||
       after.st_nlink!=1 || after.st_size!=original->st_size || after.st_mtime!=original->st_mtime || after.st_ctime!=original->st_ctime) {
        fputs("profile image driver: payload changed\n",stderr);goto done;
    }
    ok=true;
done:
    if(h>=0 && exfat_stream_close((uint64_t)h)<0) { fputs("profile image driver: stream close failed\n",stderr);ok=false; }
    return ok;
}

int main(int argc, char **argv) {
    struct stat st, data_st;
    char path[SRV_PATH_MAX];
    uint8_t *data=NULL;
    bool mounted=false, ok=false;
    int input=-1;
    char stage_dir[4096];
    const char *slash;
    fd=-1;
    /* Only the private staging image created by the Python provisioner. */
    if(argc<2 || !(slash=strrchr(argv[1],'/')) || strcmp(slash,"/image") ||
       (size_t)(slash-argv[1])>=sizeof(stage_dir)) goto done;
    memcpy(stage_dir,argv[1],(size_t)(slash-argv[1]));
    stage_dir[slash-argv[1]]=0;
    slash=strrchr(stage_dir,'/');
    if(!slash || strncmp(slash+1,".profile-image-",15) || lstat(stage_dir,&st) ||
       !S_ISDIR(st.st_mode) || st.st_uid!=geteuid() || (st.st_mode&077)) goto done;
    if ((argc!=4 && argc!=5) || strncmp(argv[3],"/fat/",5) ||
        strlen(argv[3])>=160 || strstr(argv[3],"//") || strstr(argv[3],"/../") ||
        strstr(argv[3],"/./") || !strcmp(argv[3]+strlen(argv[3])-3,"/..") ||
        argv[3][strlen(argv[3])-1]=='/') goto done;
    bool stream=!strcmp(argv[2],"write-stream");
    bool fresh=!strcmp(argv[2],"check-stream-base");
    bool audit=fresh || !strcmp(argv[2],"check-stream-final");
    bool home=audit || !strcmp(argv[2],"ensure-home") || !strcmp(argv[2],"check-clean");
    bool content=stream || !strcmp(argv[2],"write") || !strcmp(argv[2],"write-desktop");
    if(!home && !content && strcmp(argv[2],"mkdir"))goto done;
    if(content!=(argc==5) || (home && strcmp(argv[3],"/fat/home")))goto done;
    if (!strcmp(argv[2],"write-desktop") && strcmp(argv[3],"/fat/home/desktop.conf")) goto done;
    if(stream && (strncmp(argv[3],"/fat/home/",10) || !argv[3][10] ||
                 !strcmp(argv[3]+strlen(argv[3])-2,"/."))) goto done;
    snprintf(path,sizeof(path),"%s",argv[3]);
    fd=open(argv[1],O_RDWR|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0 || fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_nlink!=1 || st.st_size<=0 || st.st_size%512) goto done;
    if(argc==5) {
        input=open(argv[4],O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
        if(input<0 || fstat(input,&data_st) || !S_ISREG(data_st.st_mode) || data_st.st_nlink!=1 ||
           data_st.st_size<0 || data_st.st_size>(stream ? 16*1024*1024 : 1024*1024)) goto done;
        if(stream) {
            size_t prefix=strlen(stage_dir);
            if(strncmp(argv[4],stage_dir,prefix) || argv[4][prefix]!='/' || !argv[4][prefix+1] || strchr(argv[4]+prefix+1,'/') || data_st.st_uid!=geteuid() || (data_st.st_mode&077)) goto done;
        } else {
            data=malloc(data_st.st_size ? (size_t)data_st.st_size : 1);
            if(!data) goto done;
            size_t n=0;
            while(n<(size_t)data_st.st_size) {
                ssize_t got=read(input,data+n,(size_t)data_st.st_size-n);
                if(got<=0) goto done;
                n+=(size_t)got;
            }
        }
    }
    struct block_device device={.used=true,.name="profile-stage",.block_size=512,.block_count=(uint64_t)st.st_size/512,
        .read=block_read,.write=block_write,.flush=flush};
    /* The installed bootstrap tar contains only /init. Keep both boot VFS
     * identities live so clean sidecar restore and allocation match the guest. */
    if(!vfs_register_directory("/") || !vfs_register_file("/init",0,NULL,bootstrap_placeholder))goto done;
    /* Sidecar records contain absolute paths: use the guest mount, and refuse
     * pending/malformed metadata without repair before any mutation. */
    if(!exfat_mount_block_device_clean("/fat",&device)) goto done;
    mounted=true;
    if(!exfat_is_writable("/fat")) goto done;
    const struct vfs_node *existing=vfs_lookup(path);
    if(audit) ok=stream_audit(fresh);
    else if(stream && !existing) ok=stream_write(path,input,&data_st);
    else if(!strcmp(argv[2],"check-clean")) ok=existing && existing->type==VFS_NODE_DIRECTORY;
    else if(!strcmp(argv[2],"ensure-home")) ok=existing ? existing->type==VFS_NODE_DIRECTORY : exfat_create_directory(path);
    else if(!existing && !strcmp(argv[2],"mkdir")) ok=exfat_create_directory(path);
    else if(!existing && !strcmp(argv[2],"write")) ok=exfat_write_file(path,data,(uint64_t)data_st.st_size);
    else if(!strcmp(argv[2],"write-desktop") && (!existing || existing->type==VFS_NODE_FILE)) ok=exfat_write_file(path,data,(uint64_t)data_st.st_size);
    if(!exfat_unmount("/fat")) ok=false;
    mounted=false;
    if(fsync(fd)) ok=false;
done:
    if(mounted && !exfat_unmount("/fat")) ok=false;
    if(input>=0 && close(input)) ok=false;
    if(fd>=0 && close(fd)) ok=false;
    free(data);
    if(!ok) fputs("profile image driver: operation failed\n",stderr);
    return ok ? 0 : 1;
}
