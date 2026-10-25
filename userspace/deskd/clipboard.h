#ifndef DESKD_CLIPBOARD_H
#define DESKD_CLIPBOARD_H
#include <srvros/desktop.h>
struct desk_clipboard_peer {
    uint64_t pid,window,request,version;
    uint32_t offset;
    int reading;
    struct desk_packet reply;
};
struct desk_clipboard_store {
    unsigned char data[DESK_CLIPBOARD_BYTES],staging[DESK_CLIPBOARD_BYTES];
    uint32_t length,stage_length,stage_offset;
    uint64_t version,write_request,write_tick;
    /* Points into an admitted client; forget before destroying that client. */
    struct desk_clipboard_peer *writer;
};
void desk_clipboard_request(struct desk_clipboard_store *store,struct desk_clipboard_peer *peer,int focused,const struct desk_packet *packet,uint64_t tick);
void desk_clipboard_service(struct desk_clipboard_store *store,struct desk_clipboard_peer *peer,uint64_t tick);
int desk_clipboard_write_local(struct desk_clipboard_store *store,const void *data,uint32_t length);
void desk_clipboard_forget(struct desk_clipboard_store *store,struct desk_clipboard_peer *peer);
#endif
