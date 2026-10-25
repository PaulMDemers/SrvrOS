#include <srvros/desk_notify.h>
#include <string.h>
static int valid(const char *text,size_t capacity) {
    if(!text) return 0;
    size_t n=0;while(n<capacity && text[n]) { unsigned c=(unsigned char)text[n];if(c<32 || c==127) return 0;n++; }
    return n<capacity;
}
void desk_notify_init(struct desk_notify_client *c) { *c=(struct desk_notify_client){.window=DESK_DEFAULT_WINDOW}; }
int desk_notify_begin(struct desk_notify_client *c,uint64_t server,unsigned tone,const char *title,const char *message,uint64_t tick) {
    if(c->busy || !server || !c->window || c->serial==UINT64_MAX || tone>DESK_NOTIFY_MAX_TONE || !valid(title,DESK_NOTIFY_TITLE) || !*title || !valid(message,DESK_NOTIFY_MESSAGE)) return -1;
    struct desk_notification text={0};strcpy(text.title,title);strcpy(text.message,message);
    c->text=text;c->server=server;c->serial++;c->started=c->last_send=tick;c->tone=tone;c->busy=1;c->sent=0;return 0;
}
int desk_notify_tick(struct desk_notify_client *c,uint64_t tick) {
    if(!c->busy) return 0;
    if(tick<c->started || tick-c->started>1000) { c->busy=0;return -1; }
    if(c->sent && tick>=c->last_send && tick-c->last_send<100) return 0;
    struct desk_packet packet=desk_packet_new(DESK_NOTIFY);packet.window=c->window;packet.serial=c->serial;packet.value=c->tone;memcpy(packet.text,&c->text,sizeof(c->text));
    if(!desk_send(c->server,&packet)) { c->sent=1;c->last_send=tick; }
    return 0;
}
int desk_notify_receive(struct desk_notify_client *c,const struct desk_packet *p) {
    if(!c->busy || p->window!=c->window || p->type!=DESK_NOTIFY_ACK || p->serial!=c->serial) return 0;
    int result=c->sent && p->value==0?1:-1;c->busy=0;return result;
}
