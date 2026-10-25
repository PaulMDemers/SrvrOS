/* Real process/ABI receive-queue pressure; no desktop windows or hardware I/O. */
#include <srvros/gui.h>
#include <srvros/sys.h>
#include <srvros/desk_log.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdint.h>

#define CLIENTS 40
static int receive(struct gui_message *message) {
    uint64_t start=srv_ticks();
    while((uint64_t)srv_ticks()-start<6000) {
        int r=gui_recv(message);if(r)return r;
        srv_sleep_ticks(1);
    }
    return -1;
}
static int client(uint64_t parent,unsigned index,unsigned round) {
    if(gui_register_server()!=-1)return 1;
    struct gui_message message={.type=GUI_MSG_SET_TEXT,.value=index};
    if(gui_send(&message))return 2;
    if(receive(&message)!=1 || message.source_pid!=parent ||
       message.target_pid!=(uint64_t)getpid() || message.value!=(int64_t)(round*100+index))return 3;
    message=(struct gui_message){.type=GUI_MSG_SET_TEXT,.value=index};
    if(gui_send(&message))return 4;
    /* Retain this process and its allocated receive FIFO until every sibling
     * has completed its handshake: at least 41 queues are live simultaneously. */
    if(receive(&message)!=1 || message.source_pid!=parent || message.value!=-1)return 5;
    return 0;
}
int gui_queue_capture(void) {
    if(gui_register_server())return 1;
    uint64_t parent=getpid();pid_t children[CLIENTS]={0};
    for(unsigned round=1;round<=3;round++) {
        for(unsigned i=0;i<CLIENTS;i++) {
            pid_t pid=fork();if(pid<0)goto failure;
            if(!pid)_exit(client(parent,i,round));
            children[i]=pid;
        }
        uint64_t seen=0;
        for(unsigned i=0;i<CLIENTS;i++) {
            struct gui_message message;
            if(receive(&message)!=1 || message.value<0 || message.value>=CLIENTS ||
               message.source_pid!=(uint64_t)children[message.value] || (seen&(1ull<<message.value)))goto failure;
            seen|=1ull<<message.value;
        }
        for(unsigned i=0;i<CLIENTS;i++) {
            struct gui_message message={.type=GUI_MSG_EVENT_CLICK,.target_pid=children[i],.value=round*100+i};
            if(gui_send(&message))goto failure;
        }
        seen=0;
        for(unsigned i=0;i<CLIENTS;i++) {
            struct gui_message message;
            if(receive(&message)!=1 || message.value<0 || message.value>=CLIENTS ||
               message.source_pid!=(uint64_t)children[message.value] || (seen&(1ull<<message.value)))goto failure;
            seen|=1ull<<message.value;
        }
        desk_log("gui-queues: round=%u 40 clients authenticated; 41 live FIFOs\n",round);
        for(unsigned i=0;i<CLIENTS;i++) {
            struct gui_message message={.type=GUI_MSG_EVENT_CLOSE,.target_pid=children[i],.value=-1};
            if(gui_send(&message))goto failure;
        }
        for(unsigned i=0;i<CLIENTS;i++) {
            int status;
            if(waitpid(children[i],&status,0)!=children[i] || !WIFEXITED(status) || WEXITSTATUS(status))goto failure;
            children[i]=0;
        }
        desk_log("gui-queues: round=%u 40 clients retired and reaped\n",round);
    }
    srv_puts("gui-queues: PASS three rounds, 120 clients, all reaped\n");return 0;
failure:
    for(unsigned i=0;i<CLIENTS;i++)if(children[i]>0)kill(children[i],SIGKILL);
    for(unsigned i=0;i<CLIENTS;i++)if(children[i]>0)waitpid(children[i],NULL,0);
    srv_puts("gui-queues: FAIL\n");return 1;
}
