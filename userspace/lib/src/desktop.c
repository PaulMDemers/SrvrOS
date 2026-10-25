#include <srvros/desktop.h>
#include <srvros/gui.h>
#include <string.h>
#include <srvros/sys.h>
#include <sys/mman.h>

_Static_assert(sizeof(struct desk_packet)<=GUI_TEXT_MAX,"desktop packet fits transport");

struct desk_packet desk_packet_new(uint32_t type) {
    return (struct desk_packet){.version=DESK_PROTOCOL_VERSION,
        .size=sizeof(struct desk_packet), .type=type, .window=DESK_DEFAULT_WINDOW};
}
int desk_send(uint64_t target, const struct desk_packet *packet) {
    struct gui_message message={.type=DESK_TRANSPORT_MESSAGE, .target_pid=target};
    memcpy(message.text,packet,sizeof(*packet));
    return gui_send(&message);
}
int desk_receive(uint64_t *source, struct desk_packet *packet) {
    struct gui_message message;
    int result=gui_recv(&message);
    if (result<=0) return result;
    if (message.type!=DESK_TRANSPORT_MESSAGE) return 2;
    memcpy(packet,message.text,sizeof(*packet));
    if (packet->version!=DESK_PROTOCOL_VERSION || packet->size!=sizeof(*packet) ||
        packet->type<DESK_OPEN || packet->type>DESK_OPEN_DOCUMENT_REPLY || packet->reserved || packet->reserved2 || !packet->window) return 2;
    *source=message.source_pid;
    return 1;
}
int desk_process_alive(uint64_t pid) {
    uint64_t index=0;
    struct srv_process_info info;
    while ((index=(uint64_t)srv_proc_list(index,&info))!=0) {
        if (index==(uint64_t)-1) break;
        if (info.pid==pid) return info.state[0] && strcmp(info.state,"exited")!=0;
    }
    return 0;
}
uint32_t *desk_pixels_alloc(uint32_t width, uint32_t height) {
    if (!width || !height || width>3840 || height>2160) return 0;
    void *p=mmap(0,(size_t)width*height*4,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    return p==MAP_FAILED?0:p;
}
void desk_pixels_free(uint32_t *pixels,uint32_t width,uint32_t height) {
    if (pixels) munmap(pixels,(size_t)width*height*4);
}
