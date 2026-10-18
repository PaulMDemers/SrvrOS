#include "menus.h"
#include <srvros/desk_menu.h>
#include <string.h>
int desk_menu_receive(struct desk_menu_store *s,const struct desk_packet *p) {
    if(p->type!=DESK_MENU_SET || !p->serial || p->serial<=s->revision ||
       p->width>DESK_APP_MENU_ITEMS || p->value>1 || p->generation>=DESK_APP_MENU_ITEMS) return 0;
    if(!p->generation) { s->staging_revision=p->serial;s->total=p->width;s->next=0; }
    if(p->serial!=s->staging_revision || p->width!=s->total || p->generation!=s->next ||
       p->value!=(!s->total || s->next+1==s->total)) { s->staging_revision=0;return 0; }
    if(s->total) memcpy(&s->staging[s->next++],p->text,sizeof(s->staging[0]));
    if(!p->value) return 0;
    s->staging_revision=0;
    if(!desk_menu_valid(s->staging,s->total)) return 0;
    unsigned headings=0;
    for(unsigned i=0;i<s->total;i++) {
        if((s->staging[i].flags&DESK_MENU_HEADING) &&
            (!strcmp(s->staging[i].label,"SrvrOS") || !strcmp(s->staging[i].label,"Window") || !strcmp(s->staging[i].label,"Arrange"))) return 0;
        if(s->staging[i].id>65535 || ((s->staging[i].flags&DESK_MENU_HEADING) && ++headings>4)) return 0;
    }
    memcpy(s->items,s->staging,s->total*sizeof(s->items[0]));s->count=s->total;s->revision=p->serial;
    return 1;
}
