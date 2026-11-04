/* In-memory real-volume orchestration; each executable invocation is fresh. */
void test_workspace_exercise(struct block_device *,bool,bool,bool,bool);
static uint8_t *workspace_before;
void test_workspace_clean_unchanged(void) {
    assert(!writes && !flushes && allocations==fail_allocation);
    assert(!memcmp(workspace_before,disk,BYTES));
}
void test_workspace_counts(void) { printf("workspace-ops writes=%u flushes=%u\n",writes,flushes); }
void test_workspace_arm_clean(void) {
    for(unsigned i=0;i<EXFAT_MAX_VOLUMES;i++) if(mounts[i].used && !strcmp(mounts[i].mountpoint,"/work")) {
        mounts[i].metadata_capacity=0;fail_allocation=allocations+1;return;
    }
    assert(0);
}
static void workspace_failure_case(const char *mode) {
    assert(vfs_register_directory("/"));
    fixture(1100,true);
    system_bytes=malloc(BYTES);assert(system_bytes);memcpy(system_bytes,disk,BYTES);
    struct block_device system=device;system_backend=&system;
    assert(exfat_mount_block_device("/fat",&system));
    assert(exfat_create_directory("/fat/bin") && exfat_create_directory("/fat/home") &&
           exfat_create_directory("/fat/var"));
    assert(vfs_namespace_system_activate());
    uint8_t *system_before=malloc(BYTES);assert(system_before);memcpy(system_before,system_bytes,BYTES);
    unsigned sw=system_writes,sf=system_flushes;
    assert(exfat_mount_block_device("/work",&device));
    bool clean=!strcmp(mode,"workspace-clean");
    bool close_first=strstr(mode,"close-")!=NULL,barrier=strstr(mode,"barrier")!=NULL;
    unsigned point=0;
    const char *kind=mode+(close_first?16:10);
    bool fault=sscanf(kind,"write-%u",&point)==1 || sscanf(kind,"flush-%u",&point)==1;
    writes=flushes=0;
    if(fault && !strncmp(kind,"write-",6)) fail_write_at=point;
    if(fault && !strncmp(kind,"flush-",6)) fail_flush_at=point;
    if(barrier) fail_flush_at=1;
    workspace_before=malloc(BYTES);assert(workspace_before);memcpy(workspace_before,disk,BYTES);
    test_workspace_exercise(&device,close_first,fault,barrier,clean);
    if(barrier) assert(!writes && flushes==1);
    if(fault) assert((!strncmp(kind,"write-",6)?writes:flushes)>=point);
    unsigned wr=writes,fl=flushes;
    if(fault || barrier || clean) {
        assert(vfs_lookup("/work"));
        /* Further mutation fails without another transport write on uncertain
         * exFAT failures. Managed barrier failure instead retains its own latch. */
        if(fault) assert(!exfat_write_file("/work/source",(const uint8_t *)"retry",5));
        assert(writes==wr && flushes==fl);
        assert(exfat_unmount("/work")); /* test teardown, not managed success */
    }
    check_data("/fat/source",1100);
    assert(vfs_namespace_system_lookup("/home")==vfs_lookup("/fat/home"));
    assert(vfs_namespace_system_lookup("/usr/source")==vfs_lookup("/fat/source"));
    assert(vfs_namespace_system_lookup("/bin")==vfs_lookup("/fat/bin"));
    assert(!memcmp(system_before,system_bytes,BYTES) && system_writes==sw && system_flushes==sf);
    if(!fault && !barrier && !clean) {
        assert(exfat_mount_block_device("/work",&device));
        const struct vfs_node *node=vfs_lookup("/work/source");const uint8_t *bytes;uint64_t size;
        assert(node && vfs_read_all(node,&bytes,&size) && size==13 && !memcmp(bytes,"workspace-new",13));
        vfs_release_data(node,bytes);assert(exfat_unmount("/work"));
    }
    assert(exfat_unmount("/fat") && vfs_unregister_path("/") && !vfs_count() && !io_depth && !fs_operations);
    free(workspace_before);free(system_before);free(system_bytes);system_bytes=NULL;system_backend=NULL;
    printf("workspace-writeback: %s PASS\n",mode);
}
