#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#include "../kernel/src/process.c"

static unsigned saves;
static bool fail_save;
static bool fail_alloc;
static uint8_t saved[4096];
static size_t saved_size;
static struct process *current_subject;
static unsigned pipe_wait_mode;
void *scheduler_current_user_context(void) { return current_subject; }
void *scheduler_current_user_thread_context(void) { return NULL; }
void net_process_wake(struct process *process) { (void)process; }
void keyboard_wake_waiters(void) {}
void scheduler_wake_all(struct scheduler_wait_queue *queue) { (void)queue; }
__attribute__((noinline)) bool scheduler_wait(struct scheduler_wait_queue *queue, bool (*ready)(void *), void *arg) {
    (void)queue;struct process_pipe *pipe=arg;
    if(pipe_wait_mode==1) { pipe->size=0;pipe->read_pos=pipe->write_pos; }
    else if(pipe_wait_mode==2) { memcpy(pipe->buffer,"ready",5);pipe->read_pos=0;pipe->write_pos=pipe->size=5; }
    else if(pipe_wait_mode==3) pipe->write_refs=0;
    else if(pipe_wait_mode==4) pipe->read_refs=0;
    else assert(0);
    pipe_wait_mode=0;return ready(arg);
}
void vfs_release_data(const struct vfs_node *node, const uint8_t *data) {
    (void)node; (void)data; assert(0);
}
int64_t net_hold(uint64_t handle) { (void)handle; assert(0);return -1; }
int64_t net_close(uint64_t handle) { (void)handle; assert(0); return -1; }
void *kmalloc(uint64_t size) { return fail_alloc ? NULL : malloc(size); }
void kfree(void *ptr) { free(ptr); }
bool exfat_write_file(const char *path, const uint8_t *data, uint64_t size) {
    assert(!strcmp(path,"/test/output")); saves++;
    if (fail_save) return false;
    assert(size<=sizeof(saved));memcpy(saved,data,size);saved_size=size;return true;
}
static struct process subject;
static void setup(unsigned refs) {
    memset(&subject,0,sizeof(subject));memset(write_files,0,sizeof(write_files));
    saves=0;fail_save=false;saved_size=0;
    uint64_t handle=write_file_alloc();assert(handle==1);
    write_files[0].refs=refs;strcpy(write_files[0].path,"/test/output");
    subject.files[0]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS_WRITE,.handle=1};
    subject.stdout_redirect=true;subject.stdout_fd=3;
}
static void pipe_regression(void) {
    memset(&subject,0,sizeof(subject));memset(pipes,0,sizeof(pipes));uint64_t fds[2];
    assert(!process_file_pipe(&subject,fds) && fds[0]==3 && fds[1]==4);
    subject.files[0].flags=subject.files[1].flags=SRV_FD_NONBLOCK;
    static uint8_t input[PROCESS_PIPE_CAPACITY+512],output[PROCESS_PIPE_CAPACITY+512],model[PROCESS_PIPE_CAPACITY];
    size_t size=0;unsigned seed=12345,sequence=0;
    for(unsigned iteration=0;iteration<20000;iteration++) {
        seed=seed*1664525+1013904223;size_t request=seed%(PROCESS_PIPE_CAPACITY+113);
        if(seed&0x10000) {
            for(size_t i=0;i<request;i++) input[i]=(uint8_t)(sequence+i);
            size_t expected=PROCESS_PIPE_CAPACITY-size;if(expected>request) expected=request;
            long result=process_file_pipe_write(&subject,4,input,request);
            assert(result==(request && !expected?SRV_ERR_AGAIN:(long)expected));
            memcpy(model+size,input,expected);size+=expected;sequence+=expected;
        } else {
            memset(output,0xa7,sizeof(output));size_t expected=request<size?request:size;
            long result=process_file_pipe_read(&subject,3,output,request);
            assert(result==(request && !expected?SRV_ERR_AGAIN:(long)expected));
            assert(!memcmp(output,model,expected));for(size_t i=expected;i<sizeof(output);i++) assert(output[i]==0xa7);
            memmove(model,model+expected,size-expected);size-=expected;
        }
        assert(pipes[0].size==size && pipes[0].read_pos<PROCESS_PIPE_CAPACITY && pipes[0].write_pos<PROCESS_PIPE_CAPACITY);
    }
    if(size) assert(process_file_pipe_read(&subject,3,output,size)==(long)size && !memcmp(output,model,size));
    /* Rights offsets still follow consumed bytes across a wrapped bulk read. */
    assert(process_file_pipe_write(&subject,4,input,200)==200);pipes[0].rights[0]=(struct process_pipe_rights){.used=true,.offset=100};
    assert(process_file_pipe_read(&subject,3,output,100)==100 && pipes[0].rights[0].used && !pipes[0].rights[0].offset);
    assert(process_file_pipe_read(&subject,3,output,100)==100 && !pipes[0].rights[0].used);
    subject.files[0].flags=subject.files[1].flags=0;
    pipe_wait_mode=1;assert(process_file_pipe_write(&subject,4,input,PROCESS_PIPE_CAPACITY+17)==PROCESS_PIPE_CAPACITY+17 && !pipe_wait_mode);
    assert(process_file_pipe_read(&subject,3,output,100)==17);
    pipe_wait_mode=2;assert(process_file_pipe_read(&subject,3,output,100)==5 && !memcmp(output,"ready",5));
    pipe_wait_mode=3;assert(process_file_pipe_read(&subject,3,output,100)==0);
    pipes[0].write_refs=1;assert(process_file_pipe_write(&subject,4,input,PROCESS_PIPE_CAPACITY)==PROCESS_PIPE_CAPACITY);
    pipe_wait_mode=4;assert(process_file_pipe_write(&subject,4,input,10)==-1);
    assert(process_file_close(&subject,3)==0 && process_file_close(&subject,4)==0 && !pipes[0].used);
}
static void snapshot_read_regression(void) {
    static uint8_t data[32777],out[16386];
    for(unsigned i=0;i<sizeof(data);i++)data[i]=(uint8_t)(i*37);
    memset(&subject,0,sizeof(subject));memset(read_files,0,sizeof(read_files));
    read_files[0]=(struct process_read_file){.used=true,.refs=2,.data=data,.size=sizeof(data)};
    subject.files[0]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS,.handle=1};
    subject.files[1]=subject.files[0];
    memset(out,0xa5,sizeof(out));
    assert(process_file_read(&subject,3,out+1,16384)==16384);
    assert(out[0]==0xa5 && out[16385]==0xa5 && !memcmp(out+1,data,16384));
    assert(process_file_read(&subject,4,out+1,16384)==16384);
    assert(!memcmp(out+1,data+16384,16384) && read_files[0].offset==32768);
    read_file_close(1);assert(read_files[0].used && read_files[0].refs==1);
    assert(process_file_read(&subject,4,out+1,16384)==9 && !memcmp(out+1,data+32768,9));
    assert(!process_file_read(&subject,4,out+1,16384));
    read_files[0].offset=UINT64_MAX;assert(!process_file_read(&subject,4,out+1,16));
    read_files[0].used=false;assert(process_file_read(&subject,4,out+1,16)==-1);
    assert(process_file_read(&subject,5,out+1,16)==-1);
    memset(read_files,0,sizeof(read_files));memset(&subject,0,sizeof(subject));
}
static void positional_regression(void) {
    setup(2);
    write_files[0].open_flags=SRV_OPEN_READ|SRV_OPEN_WRITE|SRV_OPEN_APPEND;
    subject.files[1]=subject.files[0];
    assert(process_file_write(&subject,3,(const uint8_t *)"hello",5)==5);
    assert(process_file_pwrite(&subject,4,(const uint8_t *)"H",1,0)==1);
    assert(write_files[0].offset==5 && write_files[0].size==5);
    uint8_t out[16];memset(out,0xa7,sizeof(out));
    assert(process_file_pread(&subject,4,out,10,0)==5 && !memcmp(out,"Hello",5));
    assert(out[5]==0xa7 && write_files[0].offset==5);
    assert(process_file_pwrite(&subject,1,(const uint8_t *)"!",1,8)==1);
    assert(write_files[0].offset==5 && write_files[0].size==9);
    assert(process_file_pread(&subject,3,out,9,0)==9);
    assert(out[5]==0 && out[6]==0 && out[7]==0 && out[8]=='!');
    assert(process_file_pread(&subject,3,out,1,INT64_MAX)==0);
    assert(process_file_pread(&subject,3,out,UINT64_MAX,0)==SRV_ERR_INVAL);
    assert(process_file_pread(&subject,3,out,1,-1)==SRV_ERR_INVAL);
    assert(process_file_pread(&subject,3,NULL,0,0)==0);
    assert(process_file_pwrite(&subject,3,NULL,0,INT64_MAX)==0);
    assert(process_file_pwrite(&subject,3,out,1,INT64_MAX)==SRV_ERR_FBIG);
    assert(process_file_pread(&subject,3,NULL,1,0)==SRV_ERR_FAULT);
    assert(process_file_pwrite(&subject,3,NULL,1,0)==SRV_ERR_FAULT);
    assert(process_file_pwrite(&subject,40,out,1,0)==SRV_ERR_BADF);
    assert(process_file_pread(&subject,0,out,1,0)==SRV_ERR_SPIPE);
    uint64_t old_capacity=write_files[0].capacity;
    fail_alloc=true;
    assert(process_file_pwrite(&subject,3,out,1,(int64_t)old_capacity)==SRV_ERR_NOMEM);
    fail_alloc=false;
    assert(write_files[0].capacity==old_capacity && write_files[0].size==9 &&
        write_files[0].offset==5 && write_files[0].dirty && !write_files[0].failed);
    write_files[0].open_flags=SRV_OPEN_WRITE;
    assert(process_file_pread(&subject,3,out,1,0)==SRV_ERR_BADF);
    subject.files[2]=(struct process_file){.used=true,.type=PROCESS_FILE_PIPE_READ};
    assert(process_file_pread(&subject,5,out,1,0)==SRV_ERR_SPIPE);
    assert(process_file_pwrite(&subject,5,out,1,0)==SRV_ERR_SPIPE);
    subject.files[2].used=false;
    assert(process_file_close(&subject,3)==0 && process_file_close(&subject,4)==0);
    assert(saved_size==9 && !memcmp(saved,"Hello\0\0\0!",9));
    memset(&subject,0,sizeof(subject));
}
int main(void) {
    positional_regression();
    snapshot_read_regression();
    pipe_regression();
    setup(2); /* Parent retains a duplicate until wait returns. */
    for(unsigned i=0;i<1000;i++) assert(process_output_write(&subject,1,(const uint8_t *)"x",1)==1);
    assert(saves==0 && write_files[0].size==1000);
    assert(finish_process_files(&subject,0)==0);
    assert(saves==1 && saved_size==1000 && write_files[0].refs==1);
    for(unsigned i=0;i<1000;i++)assert(saved[i]=='x');
    assert(write_file_close(1)==0 && saves==1);
    setup(1);assert(process_output_write(&subject,1,(const uint8_t *)"first",5)==5);
    assert(process_file_flush(&subject,1)==0 && saves==1 && saved_size==5);
    subject.stderr_redirect=true;subject.stderr_fd=3;
    assert(process_output_write(&subject,2,(const uint8_t *)"second",6)==6);
    assert(finish_process_files(&subject,0)==0 && saves==2 && saved_size==11);
    assert(!memcmp(saved,"firstsecond",11));
    setup(2);assert(process_output_write(&subject,1,(const uint8_t *)"bad",3)==3);
    fail_save=true;assert(finish_process_files(&subject,0)==1);assert(write_file_close(1)==-1);
    setup(1);assert(process_output_write(&subject,1,(const uint8_t *)"bad",3)==3);
    fail_save=true;assert(finish_process_files(&subject,7)==7);
    setup(1);assert(process_output_write(&subject,1,(const uint8_t *)"bad",3)==3);
    subject.stdout_redirect=false;fail_save=true;
    assert(finish_process_files(&subject,0)==1); /* Last-close failure too. */
    setup(1);subject.stdin_redirect=true;subject.stdin_fd=3;
    assert(process_input_write(&subject,(const uint8_t *)"stdin",5)==5 && saves==0);
    assert(process_file_flush(&subject,0)==0 && saves==1);
    assert(finish_process_files(&subject,0)==0 && saves==1);
    memset(&subject,0,sizeof(subject));
    assert(process_fd_is_console(&subject,0,true) && process_fd_is_console(&subject,1,false));
    assert(!process_fd_is_console(&subject,0,false) && !process_fd_is_console(&subject,1,true));
    assert(!process_fd_is_console(&subject,3,false));
    subject.files[0]=(struct process_file){.used=true,.type=PROCESS_FILE_STDIO,.handle=1};
    assert(process_fd_is_console(&subject,3,false));
    subject.stdout_redirect=true;subject.stdout_fd=-1;
    assert(!process_fd_is_console(&subject,1,false) && !process_fd_is_console(&subject,3,false));
    subject.stdout_fd=3;
    assert(process_fd_is_console(&subject,1,false));
    subject.stdout_redirect=false;
    assert(process_output_write(&subject,1,(const uint8_t *)"tty",3)==-2);
    pipes[0]=(struct process_pipe){.used=true,.refs=2,.read_refs=1,.write_refs=1};
    subject.files[0]=(struct process_file){.used=true,.type=PROCESS_FILE_PIPE_WRITE,.handle=1};
    subject.stdout_redirect=true;subject.stdout_fd=3;
    assert(!process_fd_is_console(&subject,1,false) && !process_fd_is_console(&subject,3,false));
    assert(process_output_write(&subject,1,(const uint8_t *)"pipe",4)==4);
    assert(pipes[0].size==4 && !memcmp(pipes[0].buffer,"pipe",4));
    assert(finish_process_files(&subject,0)==0 && pipes[0].write_refs==0);
    /* PTY duplicates retain endpoints; stdio resolves to the slave and
     * last-master close queues HUP only for its own terminal session. */
    memset(&subject,0,sizeof(subject));uint64_t fds[2];
    assert(process_file_pty_pair(&subject,fds)==0 && fds[0]==3 && fds[1]==4);
    uint64_t handle=process_tty_handle(&subject,3);assert(handle && process_tty_handle(&subject,4)==handle);
    subject.stdin_redirect=true;subject.stdin_fd=4;subject.stdout_redirect=true;subject.stdout_fd=4;
    assert(process_tty_handle(&subject,0)==handle && process_tty_handle(&subject,1)==handle);
    assert(!process_fd_is_console(&subject,0,true) && !process_fd_is_console(&subject,1,false));
    subject.files[0].flags=SRV_FD_NONBLOCK;subject.files[1].flags=SRV_FD_NONBLOCK;
    uint8_t buffer[32];assert(pty_read(handle,true,buffer,sizeof(buffer),true)==SRV_ERR_AGAIN);
    assert(pty_write(handle,true,(const uint8_t *)"line\n",5,true)==5);
    assert(pty_read(handle,false,buffer,sizeof(buffer),true)==5 && !memcmp(buffer,"line\n",5));
    assert(process_output_write(&subject,1,(const uint8_t *)"out\n",4)==4);
    assert(pty_read(handle,true,buffer,sizeof(buffer),true)==11 && !memcmp(buffer,"line\r\nout\r\n",11));
    assert(process_file_flush(&subject,1)==0);
    struct process_file copy={0};assert(process_file_clone_entry(&copy,&subject.files[0]));
    processes[0]=(struct process){.allocated=true,.active=true,.pid=101,.signal_catch_mask=(1ull<<SRV_SIGNAL_INT)|(1ull<<SRV_SIGNAL_HUP),.controlling_tty=handle,.process_group=101};
    processes[1]=(struct process){.allocated=true,.active=true,.pid=102,.controlling_tty=handle+256,.process_group=102};
    assert(process_tty_set_group(&subject,4,101) && !process_tty_set_group(&subject,4,102));
    assert(pty_write(handle,true,(const uint8_t *)"\003",1,true)==1);
    assert(processes[0].pending_signals&(1ull<<SRV_SIGNAL_INT));
    assert(!processes[1].pending_signals);
    processes[1].process_group=101; /* Matching numeric group still cannot cross PTYs. */
    process_tty_signal(handle,SRV_SIGNAL_INT);assert(!processes[1].killed);
    assert(process_file_close(&subject,3)==0 && !pty_state(handle)->master_closed);
    process_file_release_entry(&copy);
    assert(pty_state(handle)->master_closed && (processes[0].pending_signals&(1ull<<SRV_SIGNAL_HUP)));
    assert(!processes[1].pending_signals);
    assert(process_file_close(&subject,4)==0 && !pty_state(handle));
    assert(process_file_pty_pair(&subject,fds)==0 && process_tty_handle(&subject,fds[0])!=handle);
    assert(process_file_close(&subject,fds[0])==0 && process_file_close(&subject,fds[1])==0);
    memset(processes,0,sizeof(processes));
    current_subject=&subject;subject.main_signal_mask=1ull<<SRV_SIGNAL_WINCH;
    subject.pending_signals=1ull<<SRV_SIGNAL_WINCH;subject.killed=false;
    assert(!process_signal_pending_current());
    assert(process_signal_mask_current(SRV_SIGNAL_UNBLOCK,1ull<<SRV_SIGNAL_WINCH,NULL));
    assert(!subject.killed && !subject.pending_signals);
    subject.signal_catch_mask=1ull<<SRV_SIGNAL_WINCH;subject.main_signal_mask=1ull<<SRV_SIGNAL_WINCH;
    subject.pending_signals=1ull<<SRV_SIGNAL_WINCH;
    assert(process_signal_mask_current(SRV_SIGNAL_UNBLOCK,1ull<<SRV_SIGNAL_WINCH,NULL));
    assert(process_signal_pending_current() && !subject.killed);
    current_subject=NULL;
    puts("process-output-host: PASS (20,000 pipe reference transfers, wrap/backpressure/EOF/rights, 1000 small writes batched, parent duplicate, explicit flush, stderr alias, exit errors, PTY inheritance/refcounts/session signals and stale handles)");
}
