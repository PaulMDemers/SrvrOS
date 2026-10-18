#include <srvros/desk_clipboard.h>
#include <string.h>
void desk_clipboard_init(struct desk_clipboard_client *c,void *buffer) {
    *c=(struct desk_clipboard_client){.buffer=buffer};
}
static int begin(struct desk_clipboard_client *c,uint64_t server,int mode,uint64_t tick) {
    if(c->mode || !server || !c->buffer) return -1;
    c->mode=mode;c->server=server;c->tick=tick;c->offset=c->length=0;c->sent=0;
    if(!++c->request) c->request++;
    return 0;
}
int desk_clipboard_copy(struct desk_clipboard_client *c,uint64_t server,const void *data,size_t length,uint64_t tick) {
    if(length>DESK_CLIPBOARD_BYTES || (length && !data) || begin(c,server,1,tick)) return -1;
    if(length) memcpy(c->buffer,data,length);
    c->length=length;c->buffer[length]=0;return 0;
}
int desk_clipboard_paste(struct desk_clipboard_client *c,uint64_t server,uint64_t tick) { return begin(c,server,2,tick); }
int desk_clipboard_tick(struct desk_clipboard_client *c,uint64_t tick) {
    if(!c->mode) return 0;
    if(tick-c->tick>1000) { c->mode=0;return -1; }
    for(unsigned n=0;n<4 && !c->sent;n++) {
        struct desk_packet p=desk_packet_new(c->mode==1?DESK_CLIPBOARD_SET:DESK_CLIPBOARD_GET);
        p.serial=c->request;
        if(c->mode==1) {
            p.generation=c->offset;p.width=c->length;p.height=c->length-c->offset;
            if(p.height>sizeof(p.text)) p.height=sizeof(p.text);
            memcpy(p.text,c->buffer+c->offset,p.height);p.value=c->offset+p.height==c->length;
        }
        if(desk_send(c->server,&p)) break;
        c->offset+=p.height;if(c->mode==2 || p.value) c->sent=1;
    }
    return 0;
}
int desk_clipboard_receive(struct desk_clipboard_client *c,const struct desk_packet *p) {
    if(!c->mode || p->serial!=c->request) return 0;
    if(c->mode==1 && p->type==DESK_CLIPBOARD_ACK) {
        int ok=c->sent && !p->value;c->mode=0;return ok?1:-1;
    }
    if(c->mode!=2 || p->type!=DESK_CLIPBOARD_DATA) return 0;
    if(!c->sent || p->value>1 || p->generation!=c->offset || p->width>DESK_CLIPBOARD_BYTES ||
       p->height>sizeof(p->text) || c->offset>p->width || p->height>p->width-c->offset ||
       (c->offset && c->length!=p->width) || (!p->height && !p->value)) { c->mode=0;return -1; }
    c->length=p->width;memcpy(c->buffer+c->offset,p->text,p->height);c->offset+=p->height;
    if(p->value) {
        c->mode=0;if(c->offset!=c->length) return -1;c->buffer[c->length]=0;return 1;
    }
    return 0;
}
