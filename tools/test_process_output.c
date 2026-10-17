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
void *scheduler_current_user_context(void) { return NULL; }
void scheduler_wake_all(struct scheduler_wait_queue *queue) { (void)queue; }
bool scheduler_wait(struct scheduler_wait_queue *queue, bool (*ready)(void *), void *arg) {
    (void)queue; (void)ready; (void)arg; assert(0); return false;
}
void vfs_release_data(const struct vfs_node *node, const uint8_t *data) {
    (void)node; (void)data; assert(0);
}
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
    puts("process-output-host: PASS (1000 small writes batched, parent duplicate, explicit flush, stderr alias, exit errors)");
}
