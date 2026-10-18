#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#include "../kernel/src/process.c"

static unsigned saves;
static bool fail_save;
static uint8_t saved[4096];
static size_t saved_size;
static struct process *current_subject;
void *scheduler_current_user_context(void) { return current_subject; }
void *scheduler_current_user_thread_context(void) { return NULL; }
void net_process_wake(struct process *process) { (void)process; }
void keyboard_wake_waiters(void) {}
void scheduler_wake_all(struct scheduler_wait_queue *queue) { (void)queue; }
bool scheduler_wait(struct scheduler_wait_queue *queue, bool (*ready)(void *), void *arg) {
    (void)queue; (void)ready; (void)arg; assert(0); return false;
}
void vfs_release_data(const struct vfs_node *node, const uint8_t *data) {
    (void)node; (void)data; assert(0);
}
int64_t net_hold(uint64_t handle) { (void)handle; assert(0);return -1; }
int64_t net_close(uint64_t handle) { (void)handle; assert(0); return -1; }
void *kmalloc(uint64_t size) { return malloc(size); }
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
int main(void) {
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
    puts("process-output-host: PASS (1000 small writes batched, parent duplicate, explicit flush, stderr alias, exit errors, PTY inheritance/refcounts/session signals and stale handles)");
}
