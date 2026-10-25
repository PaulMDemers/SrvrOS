#ifndef SRVROS_DESK_CLIPBOARD_H
#define SRVROS_DESK_CLIPBOARD_H
#include <srvros/desktop.h>
#include <stddef.h>
/* One nonblocking operation, with caller-owned storage. Buffer must hold
 * DESK_CLIPBOARD_BYTES+1 bytes. Copy snapshots immediately. Paste is exposed
 * only after an entire consistent transfer. The app decides when to cut or
 * insert, and must recheck the document revision/selection before doing so. */
struct desk_clipboard_client {
    unsigned char *buffer;
    uint64_t server,window,request,tick;
    uint32_t length,offset;
    int mode,sent;
};
void desk_clipboard_init(struct desk_clipboard_client *client,void *buffer);
int desk_clipboard_copy(struct desk_clipboard_client *client,uint64_t server,const void *data,size_t length,uint64_t tick);
int desk_clipboard_paste(struct desk_clipboard_client *client,uint64_t server,uint64_t tick);
/* tick: 0 pending/idle, -1 timeout. receive: 0 unrelated/pending, 1 done, -1 failed. */
int desk_clipboard_tick(struct desk_clipboard_client *client,uint64_t tick);
int desk_clipboard_receive(struct desk_clipboard_client *client,const struct desk_packet *packet);
#endif
