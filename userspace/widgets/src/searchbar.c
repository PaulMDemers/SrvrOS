#include <srvros/desk_searchbar.h>
#include <string.h>
void desk_searchbar_init(struct desk_searchbar *b) {
    *b=(struct desk_searchbar){0};desk_ui_init(&b->ui);
    desk_text_buffer_init(&b->text,b->storage[0],b->storage[1],DESK_SEARCH_QUERY);desk_edit_init(&b->query,&b->text,0);
    desk_ui_add(&b->ui,DESK_SEARCH_QUERY_FIELD,DESK_CONTROL_EDIT,&b->query,NULL);
    desk_ui_add(&b->ui,DESK_SEARCH_PREVIOUS,DESK_CONTROL_BUTTON,&b->previous,"Prev");
    desk_ui_add(&b->ui,DESK_SEARCH_NEXT,DESK_CONTROL_BUTTON,&b->next,"Next");
    desk_ui_add(&b->ui,DESK_SEARCH_CASE,DESK_CONTROL_CHECK,&b->match_case,"Match case");
    desk_ui_add(&b->ui,DESK_SEARCH_CLOSE,DESK_CONTROL_BUTTON,&b->close,"×");
}
int desk_searchbar_open(struct desk_searchbar *b,const char *seed) {
    if(seed) {
        size_t n=0;while(n<DESK_SEARCH_QUERY && seed[n]) { if((unsigned char)seed[n]<32 || (unsigned char)seed[n]==127) return -1;n++; }
        if(n==DESK_SEARCH_QUERY || desk_text_buffer_set(&b->text,seed,n)<0) return -1;
    }
    desk_ui_window_focus(&b->ui,1);desk_ui_focus(&b->ui,DESK_SEARCH_QUERY_FIELD);
    desk_text_buffer_select(&b->text,0,b->text.length);desk_edit_reveal(&b->query);return 0;
}
void desk_searchbar_configure(struct desk_searchbar *b,struct desk_rect r,int enabled) {
    if(r.w<0 || r.h<0 || r.x<0 || r.y<0 || r.x>16384 || r.y>16384 || r.w>16384 || r.h>16384) return;
    b->frame=r;
    desk_edit_configure(&b->query,desk_intersect(r,(struct desk_rect){r.x,r.y,r.w>40?r.w-40:0,30}),&desk_font_16,enabled);
    desk_button_configure(&b->close,desk_intersect(r,(struct desk_rect){r.x+r.w-32,r.y,32,30}),enabled);
    desk_button_configure(&b->previous,desk_intersect(r,(struct desk_rect){r.x,r.y+36,60,28}),enabled);
    desk_button_configure(&b->next,desk_intersect(r,(struct desk_rect){r.x+68,r.y+36,60,28}),enabled);
    int case_width=r.w>136?r.w-136:0;if(case_width>132) case_width=132;
    b->ui.controls[3].label=case_width>=124?"Match case":"Case";
    desk_check_configure(&b->match_case,desk_intersect(r,(struct desk_rect){r.x+136,r.y+36,case_width,28}),enabled,b->match_case.checked,0);
    desk_ui_layout_changed(&b->ui);
}
unsigned desk_searchbar_focus(struct desk_searchbar *b,int active) { return desk_ui_window_focus(&b->ui,active); }
static struct desk_action action(struct desk_action a,int backwards) {
    if(a.id==DESK_SEARCH_QUERY_FIELD && (a.flags&DESK_EDIT_SUBMIT)) return (struct desk_action){backwards?DESK_SEARCH_PREVIOUS:DESK_SEARCH_NEXT,DESK_WIDGET_ACTIVATED};
    return a;
}
struct desk_action desk_searchbar_key(struct desk_searchbar *b,const struct srv_key_event *key) {
    if(b->ui.active && (key->flags&SRV_KEY_DOWN_EVENT) && key->usage==SRV_KEY_ESCAPE)
        return (struct desk_action){DESK_SEARCH_CLOSE,DESK_WIDGET_ACTIVATED};
    return action(desk_ui_key(&b->ui,key),(key->modifiers&SRV_KEY_SHIFT)!=0);
}
struct desk_action desk_searchbar_pointer(struct desk_searchbar *b,struct desk_widget_event event,uint64_t time_us) {
    return action(desk_ui_pointer(&b->ui,event,time_us),0);
}
void desk_searchbar_draw(struct desk_canvas *c,const struct desk_searchbar *b,const struct desk_theme *theme) {
    struct desk_rect clip=c->clip;desk_clip(c,desk_intersect(clip,b->frame));
    desk_fill(c,b->frame,theme->surface);desk_ui_draw(c,&b->ui,theme);desk_clip(c,clip);
}
