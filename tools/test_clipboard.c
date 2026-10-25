#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_clipboard.h>
#include "../userspace/deskd/clipboard.h"
static struct desk_clipboard_store store;
static struct desk_clipboard_peer peer={.pid=99,.window=DESK_DEFAULT_WINDOW};
static struct desk_clipboard_client client;
static unsigned char buffer[DESK_CLIPBOARD_BYTES+1],document[DESK_CLIPBOARD_BYTES];
static int focused=1,block_client,block_server,completed,received,corrupt;
static uint64_t tick=1;
/* Deliberately exceed today's window count: the service has no peer registry. */
#define MANY (DESK_MAX_WINDOWS*2)
static struct desk_clipboard_peer peers[MANY];
static struct desk_clipboard_client clients[MANY];
static unsigned char buffers[MANY][DESK_CLIPBOARD_BYTES+1];
static int multi,source,blocked=-1,results[MANY];
struct desk_packet desk_packet_new(uint32_t type) {
    return (struct desk_packet){.window=DESK_DEFAULT_WINDOW,.version=DESK_PROTOCOL_VERSION,.size=sizeof(struct desk_packet),.type=type};
}
int desk_send(uint64_t target,const struct desk_packet *packet) {
    if(multi) {
        if(target==42) {
            assert(source>=0 && source<MANY);
            desk_clipboard_request(&store,&peers[source],focused,packet,tick);
        } else {
            assert(target>=1000 && target<1000+MANY);
            unsigned index=(unsigned)(target-1000);
            if((int)index==blocked) return -1;
            int result=desk_clipboard_receive(&clients[index],packet);
            if(result) results[index]=result;
        }
        return 0;
    }
    if(target==42) {
        if(block_client) return -1;
        desk_clipboard_request(&store,&peer,focused,packet,tick);
    } else {
        assert(target==99);if(block_server) return -1;
        struct desk_packet p=*packet;if(corrupt) p.generation++;
        int result=desk_clipboard_receive(&client,&p);received++;if(result) completed=result;
    }
    return 0;
}
static void pump(void) {
    for(int i=0;i<1000 && client.mode;i++) {
        assert(!desk_clipboard_tick(&client,tick++));desk_clipboard_service(&store,&peer,tick);
    }
    assert(!client.mode);
}
static void many_clients(void) {
    memset(&store,0,sizeof(store));multi=1;focused=1;
    for(unsigned i=0;i<MANY;i++) {
        peers[i].pid=1000+i;peers[i].window=100+i;desk_clipboard_init(&clients[i],buffers[i]);clients[i].window=peers[i].window;
        /* Every distinct owner must be able to copy, including peers 9..32. */
        memset(document,'A'+i%26,sizeof(document));source=i;
        assert(!desk_clipboard_copy(&clients[i],42,document,sizeof(document),tick));
        for(unsigned n=0;n<1000 && clients[i].mode;n++) {
            desk_clipboard_tick(&clients[i],tick++);desk_clipboard_service(&store,&peers[i],tick);
        }
        assert(results[i]==1 && !clients[i].mode && !store.writer);
        assert(!memcmp(store.data,document,sizeof(document)));
    }
    memset(results,0,sizeof(results));blocked=MANY-1;
    for(unsigned i=0;i<MANY;i++) {
        source=i;assert(!desk_clipboard_paste(&clients[i],42,tick));
        assert(!desk_clipboard_tick(&clients[i],tick));
    }
    for(unsigned n=0;n<200;n++) {
        tick++;
        for(unsigned i=0;i<MANY;i++) desk_clipboard_service(&store,&peers[i],tick);
    }
    /* One full client queue cannot prevent the other independent readers. */
    for(unsigned i=0;i<MANY-1;i++) {
        assert(results[i]==1 && !clients[i].mode);
        assert(clients[i].length==sizeof(document) && !memcmp(buffers[i],document,sizeof(document)));
    }
    assert(!results[MANY-1] && clients[MANY-1].mode && peers[MANY-1].offset==0);
    blocked=-1;
    for(unsigned n=0;n<200 && clients[MANY-1].mode;n++) desk_clipboard_service(&store,&peers[MANY-1],tick++);
    assert(results[MANY-1]==1 && !memcmp(buffers[MANY-1],document,sizeof(document)));
    /* Unfocused reads return cancellation, never clipboard bytes. */
    focused=0;source=MANY-1;results[source]=0;
    memset(buffers[source],0,sizeof(buffers[source]));
    assert(!desk_clipboard_paste(&clients[source],42,tick));desk_clipboard_tick(&clients[source],tick);
    desk_clipboard_service(&store,&peers[source],tick);
    assert(results[source]==-1 && !clients[source].offset && !buffers[source][0]);focused=1;
    /* A timed-out writer retains an error reply across queue pressure. */
    struct desk_packet q=desk_packet_new(DESK_CLIPBOARD_SET);
    q.window=peers[0].window;q.serial=77;q.width=10;q.height=1;q.text[0]='X';
    desk_clipboard_request(&store,&peers[0],1,&q,tick);
    assert(store.writer==&peers[0]);blocked=0;
    desk_clipboard_service(&store,&peers[0],tick+1001);
    assert(!store.writer && peers[0].reply.type==DESK_CLIPBOARD_ACK && peers[0].reply.value==1);
    blocked=-1;desk_clipboard_service(&store,&peers[0],tick+1002);assert(!peers[0].reply.type);
    assert(!memcmp(store.data,document,sizeof(document)));
    /* Retiring/reusing a writer must release the staging lease, not publish it. */
    desk_clipboard_request(&store,&peers[0],1,&q,tick);assert(store.writer==&peers[0]);
    desk_clipboard_forget(&store,&peers[0]);assert(!store.writer && !peers[0].pid);
    peers[0].pid=1000;peers[0].window=200;clients[0].window=200;source=0;results[0]=0;
    assert(!desk_clipboard_copy(&clients[0],42,"reused",6,tick));
    desk_clipboard_tick(&clients[0],tick);desk_clipboard_service(&store,&peers[0],tick);
    assert(results[0]==1 && store.length==6 && !memcmp(store.data,"reused",6));
    for(unsigned i=0;i<MANY;i++) { desk_clipboard_forget(&store,&peers[i]);assert(!peers[i].pid && !peers[i].reading && !peers[i].reply.type); }
    assert(!store.writer);multi=0;
    puts("clipboard-many: PASS 32 owners, full-size concurrent reads, independent queue retry, focus refusal, writer timeout and slot reuse");
}
int main(void) {
    desk_clipboard_init(&client,buffer);
    for(unsigned i=0;i<sizeof(document);i++) document[i]='a'+i%26;
    assert(!desk_clipboard_copy(&client,42,document,sizeof(document),tick));
    struct desk_packet foreign=desk_packet_new(DESK_CLIPBOARD_ACK);foreign.serial=client.request;foreign.window=2;
    assert(!desk_clipboard_receive(&client,&foreign) && client.mode);
    foreign=desk_packet_new(DESK_CLIPBOARD_SET);foreign.serial=1;foreign.window=2;
    foreign.width=foreign.height=1;foreign.value=1;foreign.text[0]='X';
    desk_clipboard_request(&store,&peer,1,&foreign,tick);assert(!store.writer && !store.length);

    block_client=1;assert(!desk_clipboard_tick(&client,tick) && !client.offset && !store.writer);
    block_client=0;block_server=1;
    for(int i=0;i<200;i++) desk_clipboard_tick(&client,tick);
    assert(client.sent && store.length==sizeof(document) && !memcmp(store.data,document,sizeof(document)));
    assert(client.mode && !completed);block_server=0;desk_clipboard_service(&store,&peer,tick);
    assert(completed==1 && !client.mode);
    completed=0;assert(!desk_clipboard_paste(&client,42,tick));pump();
    assert(completed==1 && client.length==sizeof(document) && !memcmp(buffer,document,sizeof(document)) && !buffer[sizeof(document)]);
    /* An incomplete new copy must not destroy the previous committed value. */
    completed=0;assert(!desk_clipboard_copy(&client,42,"new contents",12,tick));
    struct desk_packet p=desk_packet_new(DESK_CLIPBOARD_SET);
    p.serial=client.request;p.width=100;p.height=3;memcpy(p.text,"new",3);
    desk_clipboard_request(&store,&peer,1,&p,tick);
    assert(store.writer==&peer && store.length==sizeof(document));
    desk_clipboard_forget(&store,&peer);peer.pid=99;peer.window=DESK_DEFAULT_WINDOW;assert(!store.writer && store.length==sizeof(document));
    client.mode=0;focused=0;assert(!desk_clipboard_copy(&client,42,"bad",3,tick));pump();
    assert(completed==-1 && store.length==sizeof(document));focused=1;
    /* A revision change during paste cancels rather than mixing documents. */
    completed=0;assert(!desk_clipboard_paste(&client,42,tick));desk_clipboard_tick(&client,tick);
    desk_clipboard_service(&store,&peer,tick);assert(client.mode && client.offset==4*88);
    store.version++;desk_clipboard_service(&store,&peer,tick);assert(completed==-1 && !client.mode);
    completed=0;assert(!desk_clipboard_copy(&client,42,NULL,0,tick));pump();assert(completed==1 && !store.length);
    completed=0;assert(!desk_clipboard_paste(&client,42,tick));pump();assert(completed==1 && !client.length && !buffer[0]);
    assert(!desk_clipboard_paste(&client,42,tick));corrupt=1;pump();assert(completed==-1);corrupt=0;
    assert(!desk_clipboard_copy(&client,42,"held",4,tick));block_client=1;
    assert(desk_clipboard_tick(&client,tick+1001)<0 && !client.mode);block_client=0;
    assert(desk_clipboard_copy(&client,42,document,DESK_CLIPBOARD_BYTES+1,tick)<0);
    /* Out-of-order and incomplete final chunks never publish. */
    p=desk_packet_new(DESK_CLIPBOARD_SET);p.serial=101;p.width=9;p.height=3;memcpy(p.text,"bad",3);
    desk_clipboard_request(&store,&peer,1,&p,tick);p.generation=2;p.value=1;
    desk_clipboard_request(&store,&peer,1,&p,tick);assert(!store.writer && !store.length);
    p.generation=0;p.serial++;desk_clipboard_request(&store,&peer,1,&p,tick);assert(!store.writer && !store.length);
    p=desk_packet_new(DESK_CLIPBOARD_SET);p.serial=900;p.width=12;p.height=3;memcpy(p.text,"old",3);
    desk_clipboard_request(&store,&peer,1,&p,tick);assert(store.writer==&peer);
    uint64_t previous=store.version;assert(!desk_clipboard_write_local(&store,"Files",5));
    assert(!store.writer && store.version==previous+1 && store.length==5 && !memcmp(store.data,"Files",5));
    p.generation=3;p.height=9;p.value=1;memcpy(p.text," contents",9);desk_clipboard_request(&store,&peer,1,&p,tick);
    assert(store.length==5 && !memcmp(store.data,"Files",5));
    assert(desk_clipboard_write_local(&store,NULL,1)<0 && store.length==5);
    assert(!desk_clipboard_write_local(&store,store.data+1,4) && !memcmp(store.data,"iles",4));
    assert(received>700);
    /* Callers may build a page directly in the transfer buffer or copy a slice. */
    strcpy((char *)buffer,"shared buffer");completed=0;
    assert(!desk_clipboard_copy(&client,42,buffer,13,tick));pump();assert(completed==1 && store.length==13 && !memcmp(store.data,"shared buffer",13));
    completed=0;assert(!desk_clipboard_copy(&client,42,buffer+7,6,tick));pump();
    assert(completed==1 && store.length==6 && !memcmp(store.data,"buffer",6));
    many_clients();
    puts("clipboard: PASS 64 KiB chunking, retry, focused access, staged publication, interrupted copy, revision-consistent paste, empty data, bounds and timeout");
}
