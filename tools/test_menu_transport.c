#include <srvros/desk_menu.h>
#include "../userspace/deskd/menus.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct desk_menu_store store;
static struct desk_menu_item items[]={ {0,DESK_MENU_HEADING,"File",""},{1,0,"New","Ctrl-N"},{2,0,"Close","Ctrl-W"} };
static int send_item(unsigned index,unsigned total,uint64_t revision,int final) {
    struct desk_packet p={.type=DESK_MENU_SET,.serial=revision,.generation=index,.width=total,.value=final};
    if(index<3) memcpy(p.text,&items[index],sizeof(items[0]));
    return desk_menu_receive(&store,&p);
}
int main(void) {
    assert(!send_item(0,3,1,0) && !store.count);assert(!send_item(1,3,1,0));
    assert(send_item(2,3,1,1) && store.count==3 && store.revision==1);
    items[1].flags=DESK_MENU_DISABLED;
    assert(!send_item(0,3,2,0) && !store.items[1].flags);
    assert(!send_item(2,3,2,1) && store.revision==1); /* Missing item cannot publish. */
    assert(!send_item(1,3,2,0) && store.revision==1);
    assert(!send_item(0,3,3,0));assert(!send_item(1,3,3,0));assert(send_item(2,3,3,1));
    assert(store.items[1].flags==DESK_MENU_DISABLED && store.revision==3);
    assert(!send_item(0,3,2,0) && !send_item(2,3,3,1));
    items[1].id=65536;assert(!send_item(0,3,4,0));assert(!send_item(1,3,4,0));assert(!send_item(2,3,4,1));
    assert(store.revision==3 && store.items[1].id==1);
    items[1].id=2;assert(!send_item(0,3,5,0));assert(!send_item(1,3,5,0));assert(!send_item(2,3,5,1));
    assert(store.revision==3);items[1].id=1;
    assert(!send_item(0,33,6,0));assert(!send_item(0,3,6,1));assert(store.revision==3);
    assert(send_item(0,0,7,1) && !store.count && store.revision==7);
    assert(!send_item(0,0,7,1));
    puts("menu-transport: PASS staged publication, replacement, stale/out-of-order rejection, reserved/duplicate IDs, bounds and clear");
}
