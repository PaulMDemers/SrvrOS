#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_clipboard.h>
#include "../userspace/deskd/clipboard.h"
static struct desk_clipboard_store store;
static struct desk_clipboard_client client;
static unsigned char buffer[DESK_CLIPBOARD_BYTES+1],document[DESK_CLIPBOARD_BYTES];
static int focused=1,block_client,block_server,completed,received,corrupt;
static uint64_t tick=1;
struct desk_packet desk_packet_new(uint32_t type) {
    return (struct desk_packet){.version=DESK_PROTOCOL_VERSION,.size=sizeof(struct desk_packet),.type=type};
}
int desk_send(uint64_t target,const struct desk_packet *packet) {
    if(target==42) {
        if(block_client) return -1;
        desk_clipboard_request(&store,99,focused,packet,tick);
    } else {
        assert(target==99);if(block_server) return -1;
        struct desk_packet p=*packet;if(corrupt) p.generation++;
        int result=desk_clipboard_receive(&client,&p);received++;if(result) completed=result;
    }
    return 0;
}
static void pump(void) {
    for(int i=0;i<1000 && client.mode;i++) {
        assert(!desk_clipboard_tick(&client,tick++));desk_clipboard_service(&store,99,tick);
    }
    assert(!client.mode);
}
int main(void) {
    desk_clipboard_init(&client,buffer);
    for(unsigned i=0;i<sizeof(document);i++) document[i]='a'+i%26;
    assert(!desk_clipboard_copy(&client,42,document,sizeof(document),tick));
    block_client=1;assert(!desk_clipboard_tick(&client,tick) && !client.offset && !store.writer);
    block_client=0;block_server=1;
    for(int i=0;i<200;i++) desk_clipboard_tick(&client,tick);
    assert(client.sent && store.length==sizeof(document) && !memcmp(store.data,document,sizeof(document)));
    assert(client.mode && !completed);block_server=0;desk_clipboard_service(&store,99,tick);
    assert(completed==1 && !client.mode);
    completed=0;assert(!desk_clipboard_paste(&client,42,tick));pump();
    assert(completed==1 && client.length==sizeof(document) && !memcmp(buffer,document,sizeof(document)) && !buffer[sizeof(document)]);
    /* An incomplete new copy must not destroy the previous committed value. */
    completed=0;assert(!desk_clipboard_copy(&client,42,"new contents",12,tick));
    struct desk_packet p=desk_packet_new(DESK_CLIPBOARD_SET);
    p.serial=client.request;p.width=100;p.height=3;memcpy(p.text,"new",3);
    desk_clipboard_request(&store,99,1,&p,tick);
    assert(store.writer==99 && store.length==sizeof(document));
    desk_clipboard_forget(&store,99);assert(!store.writer && store.length==sizeof(document));
    client.mode=0;focused=0;assert(!desk_clipboard_copy(&client,42,"bad",3,tick));pump();
    assert(completed==-1 && store.length==sizeof(document));focused=1;
    /* A revision change during paste cancels rather than mixing documents. */
    completed=0;assert(!desk_clipboard_paste(&client,42,tick));desk_clipboard_tick(&client,tick);
    desk_clipboard_service(&store,99,tick);assert(client.mode && client.offset==4*88);
    store.version++;desk_clipboard_service(&store,99,tick);assert(completed==-1 && !client.mode);
    completed=0;assert(!desk_clipboard_copy(&client,42,NULL,0,tick));pump();assert(completed==1 && !store.length);
    completed=0;assert(!desk_clipboard_paste(&client,42,tick));pump();assert(completed==1 && !client.length && !buffer[0]);
    assert(!desk_clipboard_paste(&client,42,tick));corrupt=1;pump();assert(completed==-1);corrupt=0;
    assert(!desk_clipboard_copy(&client,42,"held",4,tick));block_client=1;
    assert(desk_clipboard_tick(&client,tick+1001)<0 && !client.mode);block_client=0;
    assert(desk_clipboard_copy(&client,42,document,DESK_CLIPBOARD_BYTES+1,tick)<0);
    /* Out-of-order and incomplete final chunks never publish. */
    p=desk_packet_new(DESK_CLIPBOARD_SET);p.serial=101;p.width=9;p.height=3;memcpy(p.text,"bad",3);
    desk_clipboard_request(&store,99,1,&p,tick);p.generation=2;p.value=1;
    desk_clipboard_request(&store,99,1,&p,tick);assert(!store.writer && !store.length);
    p.generation=0;p.serial++;desk_clipboard_request(&store,99,1,&p,tick);assert(!store.writer && !store.length);
    assert(received>700);
    puts("clipboard: PASS 64 KiB chunking, retry, focused access, staged publication, interrupted copy, revision-consistent paste, empty data, bounds and timeout");
}
