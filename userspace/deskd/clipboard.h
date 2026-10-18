#ifndef DESKD_CLIPBOARD_H
#define DESKD_CLIPBOARD_H
#include <srvros/desktop.h>
#define DESK_CLIPBOARD_PEERS 8
struct desk_clipboard_peer {
    uint64_t pid,request,version;
    uint32_t offset;
    int reading;
    struct desk_packet reply;
};
struct desk_clipboard_store {
    unsigned char data[DESK_CLIPBOARD_BYTES],staging[DESK_CLIPBOARD_BYTES];
    uint32_t length,stage_length,stage_offset;
    uint64_t version,writer,write_request,write_tick;
    struct desk_clipboard_peer peers[DESK_CLIPBOARD_PEERS];
};
void desk_clipboard_request(struct desk_clipboard_store *store,uint64_t pid,int focused,const struct desk_packet *packet,uint64_t tick);
void desk_clipboard_service(struct desk_clipboard_store *store,uint64_t pid,uint64_t tick);
void desk_clipboard_forget(struct desk_clipboard_store *store,uint64_t pid);
#endif
