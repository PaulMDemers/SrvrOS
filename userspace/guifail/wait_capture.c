/* Exercise the real GUI wait ABI, scheduler, pipe lifetimes and cancellation. */
#include <srvros/gui.h>
#include <srvros/sys.h>
#include <srvros/desk_log.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#define CHECK(x) do { if(!(x)) { desk_log("gui-wait-capture: FAIL line=%u\n",__LINE__);return 1; } } while(0)
static int receive(struct gui_message *message) {
    uint64_t start=srv_ticks();
    while((uint64_t)srv_ticks()-start<300) {
        int r=gui_recv(message);if(r)return r;
        srv_sleep_ticks(1);
    }
    return -1;
}
static int server(int ready_fd) {
    if(gui_register_server())return 1;
    if(write(ready_fd,"R",1)!=1)return 2;
    close(ready_fd);
    for(;;) {
        struct gui_message m;
        if(receive(&m)!=1)return 3;
        if(m.value==99)return 0;
        srv_sleep_ticks(4);
        struct gui_message reply={.type=GUI_MSG_EVENT_CLICK,.target_pid=m.source_pid,.value=m.value};
        if(gui_send(&reply))return 4;
    }
}
struct action { int kind,fd,other; };
static void *act(void *argument) {
    struct action *a=argument;srv_sleep_ticks(4);
    if(a->kind==1)return (void *)(uintptr_t)(write(a->fd,"x",1)!=1);
    if(a->kind==2)return (void *)(uintptr_t)(gui_cancel_wait()!=1);
    if(a->kind==3)return (void *)(uintptr_t)(close(a->fd) || dup2(a->other,a->fd)!=a->fd);
    return (void *)1;
}
static struct srv_gui_wait request(uint64_t peer,int fd,uint64_t epoch) {
    struct srv_gui_wait r={.abi_version=SRV_GUI_WAIT_VERSION,.struct_size=sizeof(r),.server=peer,
        .deadline_us=(uint64_t)srv_monotonic_us()+500000,.cancel_epoch=epoch};
    if(fd>=0) {r.count=1;r.fds[0]=(struct srv_gui_wait_fd){.fd=fd,.events=SRV_POLLIN};}
    return r;
}
static int client(uint64_t peer) {
    struct gui_message message={.type=GUI_MSG_SET_TEXT,.value=1};
    CHECK(!gui_send(&message) && receive(&message)==1 && message.source_pid==peer && message.value==1);
    struct srv_gui_wait r=request(peer,-1,0);r.deadline_us=0;
    int probe=gui_wait_io(&r);
    desk_log("gui-wait-capture: probe result=%d ready=%u epoch=%llu peer=%llu\n",probe,r.ready,(unsigned long long)r.cancel_epoch,(unsigned long long)peer);
    CHECK(!probe && r.ready==SRV_GUI_DEADLINE);
    r=request(peer,-1,0);uint64_t start=srv_monotonic_us();r.deadline_us=start+35000;
    CHECK(!gui_wait_io(&r) && r.ready==SRV_GUI_DEADLINE && (uint64_t)srv_monotonic_us()-start>=35000);
    int pipefd[2],spare[2];CHECK(!pipe(pipefd) && !pipe(spare));
    CHECK(!srv_fcntl(pipefd[0],SRV_F_SETFL,SRV_FD_NONBLOCK) && !srv_fcntl(spare[0],SRV_F_SETFL,SRV_FD_NONBLOCK));
    pthread_t task;void *result;char byte;
    struct action a={.kind=1,.fd=pipefd[1]};
    CHECK(!pthread_create(&task,NULL,act,&a));r=request(peer,pipefd[0],0);
    CHECK(!gui_wait_io(&r) && r.ready==SRV_GUI_FD_READY && r.fds[0].revents==SRV_POLLIN);
    CHECK(!pthread_join(task,&result) && !result && read(pipefd[0],&byte,1)==1 && byte=='x');
    a.kind=2;CHECK(!pthread_create(&task,NULL,act,&a));r=request(peer,pipefd[0],0);
    CHECK(!gui_wait_io(&r) && r.ready==SRV_GUI_CANCELED && r.cancel_epoch==1);
    CHECK(!pthread_join(task,&result) && !result);
    a=(struct action){.kind=3,.fd=pipefd[0],.other=spare[0]};
    CHECK(!pthread_create(&task,NULL,act,&a));r=request(peer,pipefd[0],1);
    CHECK(!gui_wait_io(&r) && r.ready==SRV_GUI_FD_READY && r.fds[0].revents==SRV_POLLNVAL);
    CHECK(!pthread_join(task,&result) && !result);
    message=(struct gui_message){.type=GUI_MSG_SET_TEXT,.value=2};CHECK(!gui_send(&message));
    r=request(peer,pipefd[0],1);CHECK(!gui_wait_io(&r) && r.ready==SRV_GUI_READY);
    CHECK(receive(&message)==1 && message.value==2);
    message=(struct gui_message){.type=GUI_MSG_SET_TEXT,.value=3};CHECK(!gui_send(&message));
    CHECK(gui_wait(peer,100)==1 && write(spare[1],"s",1)==1);
    r=request(peer,pipefd[0],1);r.deadline_us=0;
    CHECK(!gui_wait_io(&r) && r.ready==(SRV_GUI_READY|SRV_GUI_FD_READY) && r.fds[0].revents==SRV_POLLIN);
    CHECK(receive(&message)==1 && message.value==3 && read(pipefd[0],&byte,1)==1 && byte=='s');
    CHECK(!close(pipefd[0]) && !close(pipefd[1]) && !close(spare[0]) && !close(spare[1]));
    message=(struct gui_message){.type=GUI_MSG_SET_TEXT,.value=99};CHECK(!gui_send(&message));
    r=request(peer,-1,1);CHECK(!gui_wait_io(&r) && r.ready==SRV_GUI_DISCONNECTED);
    return 0;
}
int gui_wait_capture(void) {
    int ready[2];CHECK(!pipe(ready));
    pid_t child=fork();CHECK(child>=0);
    if(!child) {close(ready[0]);_exit(server(ready[1]));}
    close(ready[1]);char byte;int ok=read(ready[0],&byte,1)==1 && byte=='R';close(ready[0]);
    if(ok)ok=!client(child);
    if(!ok)kill(child,SIGKILL);
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status) && ok);
    srv_puts("gui-wait-capture: PASS deadlines, delayed pipe, thread cancellation, descriptor replacement, simultaneous GUI/pipe and peer exit\n");return 0;
}
