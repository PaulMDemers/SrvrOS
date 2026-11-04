void test_process_salvage(struct block_device *,bool);
static const char *salvage_mode;
static uint8_t *salvage_work_before,*salvage_destination_before;
static unsigned salvage_work_writes,salvage_work_flushes;
void test_salvage_source_fault(void) { fail_write_at=writes+1; }
void test_salvage_destination_arm(void) {
    salvage_work_before=malloc(BYTES);assert(salvage_work_before);memcpy(salvage_work_before,disk,BYTES);
    salvage_destination_before=malloc(BYTES);assert(salvage_destination_before);memcpy(salvage_destination_before,system_bytes,BYTES);
    salvage_work_writes=writes;salvage_work_flushes=flushes;
    system_writes=system_flushes=0;
    unsigned point;
    if(sscanf(salvage_mode,"salvage-write-%u",&point)==1) fail_system_write_at=point;
    if(sscanf(salvage_mode,"salvage-flush-%u",&point)==1) fail_system_flush_at=point;
    if(!strcmp(salvage_mode,"salvage-oom")) fail_allocation=allocations+1;
}
void test_salvage_verify_destination(const uint8_t *expected,size_t size,bool copied) {
    printf("salvage-ops writes=%u flushes=%u\n",system_writes,system_flushes);
    assert(!memcmp(salvage_work_before,disk,BYTES));
    assert(writes==salvage_work_writes && flushes==salvage_work_flushes);
    if(copied) {
        assert(exfat_is_writable("/fat"));
        assert(exfat_unmount("/fat") && !vfs_namespace_system_lookup("/home"));
        assert(exfat_mount_block_device("/fat",system_backend) && vfs_namespace_system_activate());
        const struct vfs_node *node=vfs_namespace_system_lookup("/home/salvage");
        const uint8_t *bytes;uint64_t length;
        assert(node && node==vfs_lookup("/fat/home/salvage") && vfs_read_all(node,&bytes,&length));
        assert(length==size && !memcmp(bytes,expected,size));vfs_release_data(node,bytes);
    } else {
        if(!strcmp(salvage_mode,"salvage-conflict") || !strcmp(salvage_mode,"salvage-oom")) {
            assert(!system_writes && !system_flushes && !memcmp(salvage_destination_before,system_bytes,BYTES));
            if(!strcmp(salvage_mode,"salvage-oom")) assert(allocations==fail_allocation);
        } else {
            unsigned point;
            if(sscanf(salvage_mode,"salvage-write-%u",&point)==1) assert(system_writes>=point);
            else { assert(sscanf(salvage_mode,"salvage-flush-%u",&point)==1 && system_flushes>=point); }
            assert(!exfat_is_writable("/fat"));
        }
    }
    check_data("/fat/source",1100);
    assert(vfs_namespace_system_lookup("/home")==vfs_lookup("/fat/home"));
    assert(vfs_namespace_system_lookup("/usr/source")==vfs_lookup("/fat/source"));
    assert(!memcmp(salvage_work_before,disk,BYTES));
    free(salvage_work_before);free(salvage_destination_before);
}
static void workspace_salvage_case(const char *mode) {
    salvage_mode=mode;
    assert(vfs_register_directory("/"));fixture(1100,true);
    system_bytes=malloc(BYTES);assert(system_bytes);memcpy(system_bytes,disk,BYTES);
    struct block_device system=device;system_backend=&system;
    assert(exfat_mount_block_device("/fat",&system));
    assert(exfat_create_directory("/fat/bin") && exfat_create_directory("/fat/home") && exfat_create_directory("/fat/var"));
    assert(vfs_namespace_system_activate() && exfat_mount_block_device("/work",&device));
    if(!strcmp(mode,"salvage-conflict")) assert(exfat_write_file("/fat/home/salvage",(const uint8_t *)"preserve",8));
    test_process_salvage(&device,!strcmp(mode,"salvage-control"));
    assert(exfat_unmount("/work") && exfat_unmount("/fat")); /* teardown, not managed recovery */
    assert(vfs_unregister_path("/") && !vfs_count() && !io_depth && !fs_operations);
    free(system_bytes);system_bytes=NULL;system_backend=NULL;
    printf("workspace-salvage: %s PASS\n",mode);
}
