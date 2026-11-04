/* Actual file operations; only unrelated PTY/signal wake branches are adapters. */
void *scheduler_current_user_context(void) { return NULL; }
void *scheduler_current_user_thread_context(void) { return NULL; }
void net_process_wake(struct process *process) { (void)process; }
void keyboard_wake_waiters(void) { assert(0); }
void scheduler_wake_all(struct scheduler_wait_queue *queue) { (void)queue; }
int64_t pty_read(uint64_t h,bool master,uint8_t *b,uint64_t n,bool nonblock) {
    (void)h;(void)master;(void)b;(void)n;(void)nonblock;assert(0);return SRV_ERR_IO;
}
int64_t pty_write(uint64_t h,bool master,const uint8_t *b,uint64_t n,bool nonblock) {
    (void)h;(void)master;(void)b;(void)n;(void)nonblock;assert(0);return SRV_ERR_IO;
}
int64_t pty_get_flags(uint64_t handle,bool master) { (void)handle;(void)master;assert(0);return SRV_ERR_IO; }
void test_salvage_source_fault(void);
void test_salvage_destination_arm(void);
void test_salvage_verify_destination(const uint8_t *,size_t,bool);
static void salvage_read_held(struct process *p,const uint8_t *expected,size_t size) {
    uint8_t bytes[127];size_t off=0;assert(process_file_seek(p,1,0,0)==0);
    while(off<size) {
        int64_t n=process_file_read(p,1,bytes,sizeof(bytes));
        assert(n>0 && (uint64_t)n<=size-off && !memcmp(bytes,expected+off,(size_t)n));
        off+=(size_t)n;
    }
    assert(process_file_read(p,1,bytes,sizeof(bytes))==0);
    assert(process_file_seek(p,1,0,0)==0);
}
void test_process_salvage(struct block_device *device,bool expect_success) {
    active=device;failed=false;
    struct process subject={0};subject.fsize_limit.current=UINT64_MAX;
    strcpy(subject.cwd,"/");strcpy(subject.cwd_storage,"/");
    const struct vfs_node *node=vfs_lookup("/work/source");const uint8_t *data;uint64_t size;
    assert(node && vfs_read_all(node,&data,&size));
    struct process_file_content *content=content_acquire(node,node->path,data,size);
    assert(content);content->refs++;
    write_files[0]=(struct process_write_file){.used=true,.refs=1,.content=content,.open_flags=SRV_OPEN_WRITE};
    read_files[0]=(struct process_read_file){.used=true,.refs=1,.content=content};
    subject.files[0]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS_WRITE,.handle=1};
    subject.files[1]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS,.handle=1};
    uint8_t expected[2049];for(unsigned i=0;i<sizeof(expected);i++)expected[i]=(uint8_t)(i*37+11);
    assert(process_file_write(&subject,0,expected,sizeof(expected))==sizeof(expected));
    test_salvage_source_fault();
    assert(process_file_flush(&subject,0)==SRV_ERR_IO && content->dirty && content->failed);
    assert(!workspace_sync() && failed && !exfat_is_writable("/work"));
    assert(write_file_close(1)==SRV_ERR_IO);subject.files[0].used=false;
    assert(content->refs==1);salvage_read_held(&subject,expected,sizeof(expected));
    test_salvage_destination_arm();
    int64_t fd=process_file_open_write(&subject,"/home/salvage",SRV_OPEN_WRITE|SRV_OPEN_CREATE|SRV_OPEN_EXCL);
    bool copied=fd>=0;
    if(fd>=0) {
        uint8_t bytes[127];size_t off=0;
        while(off<sizeof(expected)) {
            int64_t n=process_file_read(&subject,1,bytes,sizeof(bytes));
            if(n<=0 || process_file_write(&subject,(uint64_t)fd,bytes,(uint64_t)n)!=n) {copied=false;break;}
            off+=(size_t)n;
        }
        if(copied && process_file_flush(&subject,(uint64_t)fd)<0) copied=false;
        if(write_file_close(subject.files[fd].handle)<0) copied=false;
        subject.files[fd].used=false;
    }
    assert(copied==expect_success);
    /* Keep the source description, not a reopened pathname, across destination
     * errors and healthy-volume retirement/readback. */
    assert(content->refs==1 && content->dirty && content->failed);
    salvage_read_held(&subject,expected,sizeof(expected));
    test_salvage_verify_destination(expected,sizeof(expected),copied);
    assert(content->refs==1 && content->dirty && content->failed);
    salvage_read_held(&subject,expected,sizeof(expected));
    assert(!workspace_sync() && !workspace_unmount() && active==device && !test_disables);
    assert(read_file_close(1)==SRV_ERR_IO);subject.files[1].used=false;
    assert(!content_find("/work/source") && !workspace_unmount() && active==device);
    active=NULL;
}
