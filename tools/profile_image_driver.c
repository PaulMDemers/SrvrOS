/* Offline staging image adapter using the actual kernel exFAT/VFS code. */
#define main exfat_fixture_driver_main
#include "exfat_host_driver.c"
#undef main

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
    if (strcmp(argv[2],"mkdir") && strcmp(argv[2],"ensure-home") && strcmp(argv[2],"write") && strcmp(argv[2],"write-desktop") && strcmp(argv[2],"check-clean")) goto done;
    if ((!strcmp(argv[2],"write") || !strcmp(argv[2],"write-desktop")) != (argc==5)) goto done;
    if ((!strcmp(argv[2],"ensure-home") || !strcmp(argv[2],"check-clean")) && strcmp(argv[3],"/fat/home")) goto done;
    if (!strcmp(argv[2],"write-desktop") && strcmp(argv[3],"/fat/home/desktop.conf")) goto done;
    snprintf(path,sizeof(path),"%s",argv[3]);
    fd=open(argv[1],O_RDWR|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0 || fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_nlink!=1 || st.st_size<=0 || st.st_size%512) goto done;
    if(argc==5) {
        input=open(argv[4],O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
        if(input<0 || fstat(input,&data_st) || !S_ISREG(data_st.st_mode) || data_st.st_nlink!=1 ||
           data_st.st_size<0 || data_st.st_size>1024*1024) goto done;
        data=malloc(data_st.st_size ? (size_t)data_st.st_size : 1);
        if(!data) goto done;
        size_t n=0;
        while(n<(size_t)data_st.st_size) { ssize_t got=read(input,data+n,(size_t)data_st.st_size-n);if(got<=0) goto done;n+=(size_t)got; }
    }
    struct block_device device={.used=true,.name="profile-stage",.block_size=512,.block_count=(uint64_t)st.st_size/512,
        .read=block_read,.write=block_write,.flush=flush};
    /* Sidecar records contain absolute paths: use the guest mount, and refuse
     * pending/malformed metadata without repair before any mutation. */
    if(!exfat_mount_block_device_clean("/fat",&device)) goto done;
    mounted=true;
    if(!exfat_is_writable("/fat")) goto done;
    const struct vfs_node *existing=vfs_lookup(path);
    if(!strcmp(argv[2],"check-clean")) ok=existing && existing->type==VFS_NODE_DIRECTORY;
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
