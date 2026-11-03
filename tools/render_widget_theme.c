/* Host-only visual specimen: pass an output PPM path. */
#include <srvros/desk_widgets.h>
#include <srvros/desk_slider.h>
#include <srvros/desk_tabs.h>
#include <srvros/desk_edit.h>
#include <srvros/desk_list.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void specimen(struct desk_canvas *c,int x,int w) {
    const struct desk_theme *t=&desk_theme_silver;
    desk_fill(c,(struct desk_rect){x,0,w,470},t->surface);
    desk_label_draw(c,(struct desk_rect){x+12,10,w-24,24},t->heading,w>350?"Silver · wide":"Silver · compact",t->text);
    struct desk_button b={0};
    const char *labels[]={"Normal","Pressed","Focused","Disabled"};
    for(int i=0;i<4;i++) {
        desk_button_configure(&b,(struct desk_rect){x+12+(i%2)*((w-32)/2),42+(i/2)*36,(w-40)/2,28},i!=3);
        b.focused=i==2;b.armed=b.inside=i==1;
        desk_button_centered(&b,1);desk_button_draw(c,&b,labels[i],t);
    }
    struct desk_check check={0};
    desk_check_configure(&check,(struct desk_rect){x+12,120,w-24,26},1,1,0);desk_check_draw(c,&check,"Remember this choice",t);
    desk_check_configure(&check,(struct desk_rect){x+12,148,w-24,26},1,1,1);desk_check_draw(c,&check,"Selected option",t);
    struct desk_slider s;desk_slider_init(&s);desk_slider_configure(&s,(struct desk_rect){x+12,179,w-44,24},1);desk_slider_set(&s,65);s.focused=1;desk_slider_draw(c,&s,t);
    struct desk_scrollbar bar={0};desk_scrollbar_configure(&bar,(struct desk_rect){x+w-24,120,14,84},500,120);desk_scrollbar_set(&bar,180);desk_scrollbar_draw(c,&bar,t);
    struct desk_tabs tabs;desk_tabs_init(&tabs);struct desk_tab items[]={{1,"General",0},{2,"Long content label",0},{3,"Other",1}};
    desk_tabs_items(&tabs,items,3);desk_tabs_configure(&tabs,(struct desk_rect){x+12,215,w-24,28},1);desk_tabs_draw(c,&tabs,t);
    desk_label_ellipsis(c,(struct desk_rect){x+12,255,w-24,20},t->body,"A long document title that should truncate cleanly at either width",t->text);
    char data[128],history[128];struct desk_text_buffer text;
    desk_text_buffer_init(&text,data,history,sizeof(data));
    const char *value="A field with a longer value";desk_text_buffer_set(&text,value,strlen(value));
    struct desk_edit edit;desk_edit_init(&edit,&text,0);desk_edit_configure(&edit,(struct desk_rect){x+12,285,w-24,28},t->body,1);edit.focused=1;desk_edit_draw(c,&edit,t);
    struct desk_list list;desk_list_init(&list);
    struct desk_list_item rows[]={{"Documents","",1,1},{"A long file name that is clipped in the list","",2,0},{"Notes.txt","",3,0}};
    desk_list_set_items(&list,rows,3);desk_list_configure(&list,(struct desk_rect){x+12,325,w-24,120},1);desk_list_select(&list,1);desk_list_draw(c,&list,t);
}
int main(int argc,char **argv) {
    enum { W=760,H=470 };if(argc!=2) return 2;
    uint32_t *pixels=calloc(W*H,sizeof(*pixels));if(!pixels) return 1;
    struct desk_canvas c;if(desk_canvas_init(&c,pixels,W,H,W)) return 1;
    specimen(&c,0,280);specimen(&c,290,470);
    FILE *f=fopen(argv[1],"wb");if(!f) return 1;fprintf(f,"P6\n%d %d\n255\n",W,H);
    for(int i=0;i<W*H;i++) { unsigned char rgb[]={pixels[i]>>16,pixels[i]>>8,pixels[i]};fwrite(rgb,1,3,f); }
    int error=fclose(f);free(pixels);return error!=0;
}
