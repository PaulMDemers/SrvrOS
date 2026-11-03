/* Host-only visual specimen: pass an output PPM path. */
#include <srvros/desk_widgets.h>
#include <srvros/desk_slider.h>
#include <srvros/desk_tabs.h>
#include <srvros/desk_edit.h>
#include <srvros/desk_list.h>
#include <srvros/desk_popup.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void specimen(struct desk_canvas *c,int x,int w,unsigned style) {
    struct desk_theme theme;desk_theme_configure_style(&theme,style,0,2,16);const struct desk_theme *t=&theme;
    desk_fill(c,(struct desk_rect){x,0,w,650},t->surface);
    desk_label_draw(c,(struct desk_rect){x+12,10,w-24,24},t->heading,style==0?"Silver":style==1?"Aero":"Aqua",t->text);
    struct desk_button b={0};
    const char *labels[]={"Normal","Pressed","Focused","Disabled"};
    for(int i=0;i<4;i++) {
        desk_button_configure(&b,(struct desk_rect){x+12+(i%2)*((w-32)/2),42+(i/2)*36,(w-40)/2,28},i!=3);
        b.focused=i==2;b.armed=b.inside=i==1;
        desk_button_centered(&b,1);desk_button_draw(c,&b,labels[i],t);
    }
    struct desk_check check={0};
    desk_check_configure(&check,(struct desk_rect){x+12,120,w-24,26},1,1,0);desk_check_draw(c,&check,"Remember this choice",t);
    desk_check_configure(&check,(struct desk_rect){x+12,148,w-24,26},1,1,1);check.button.focused=1;desk_check_draw(c,&check,"Selected option",t);
    struct desk_slider s;desk_slider_init(&s);desk_slider_configure(&s,(struct desk_rect){x+12,179,w-44,24},1);desk_slider_set(&s,65);s.focused=1;desk_slider_draw(c,&s,t);
    struct desk_scrollbar bar={0};desk_scrollbar_configure(&bar,(struct desk_rect){x+w-24,120,14,84},500,120);desk_scrollbar_set(&bar,180);desk_scrollbar_draw(c,&bar,t);
    struct desk_tabs tabs;desk_tabs_init(&tabs);struct desk_tab items[]={{1,"General",0},{2,"Long content label",0},{3,"Other",1}};
    desk_tabs_items(&tabs,items,3);desk_tabs_configure(&tabs,(struct desk_rect){x+12,215,w-24,28},1);desk_tabs_draw(c,&tabs,t);
    desk_label_ellipsis(c,(struct desk_rect){x+12,255,w-24,20},t->body,"A long document title that should truncate cleanly at either width",t->text);
    char data[128],history[128];struct desk_text_buffer text;
    desk_text_buffer_init(&text,data,history,sizeof(data));
    const char *value="A field with a longer value";desk_text_buffer_set(&text,value,strlen(value));
    struct desk_edit edit;desk_edit_init(&edit,&text,0);desk_edit_configure(&edit,(struct desk_rect){x+12,285,w-24,28},t->body,1);desk_text_buffer_select(&text,2,12);edit.focused=1;desk_edit_draw(c,&edit,t);
    struct desk_list list;desk_list_init(&list);
    struct desk_list_item rows[]={{"Documents","",1,1},{"A long file name that is clipped in the list","",2,0},{"Notes.txt","",3,0}};
    desk_list_set_items(&list,rows,3);desk_list_configure(&list,(struct desk_rect){x+12,325,w-24,120},1);desk_list_select(&list,1);desk_list_draw(c,&list,t);
    struct desk_popup popup;desk_popup_init(&popup);
    struct desk_menu_item menu[]={{1,DESK_MENU_CHECKED,"Selected item","Ctrl+S"},{2,DESK_MENU_DISABLED,"Disabled item",""},{3,0,"Another action",""}};
    desk_popup_open(&popup,menu,3,x+12,535,(struct desk_rect){x,530,w,120},t->body,1);
    desk_popup_draw(c,&popup,t);
}
int main(int argc,char **argv) {
    enum { W=1040,H=650 };if(argc!=2) return 2;
    uint32_t *pixels=calloc(W*H,sizeof(*pixels));if(!pixels) return 1;
    struct desk_canvas c;if(desk_canvas_init(&c,pixels,W,H,W)) return 1;
    desk_fill(&c,c.clip,0xd3d9e0);
    specimen(&c,0,340,0);specimen(&c,350,340,1);specimen(&c,700,340,2);
    for(unsigned i=0;i<3;i++) {
        struct desk_theme t;desk_theme_configure_style(&t,i,0,2,16);
        int x=i*350;
        desk_theme_gradient(&c,(struct desk_rect){x,470,340,60},0x587fa4,0xa4bfd3);
        desk_fill(&c,(struct desk_rect){x+100,470,80,60},0x53716a);
        desk_theme_glass(&c,(struct desk_rect){x+12,480,316,38},t.chrome_top,t.chrome_bottom,t.glass_alpha,t.corner_radius);
        desk_label_draw(&c,(struct desk_rect){x+24,488,280,24},t.body,"Glass over two backgrounds",t.text);
    }
    FILE *f=fopen(argv[1],"wb");if(!f) return 1;fprintf(f,"P6\n%d %d\n255\n",W,H);
    for(int i=0;i<W*H;i++) { unsigned char rgb[]={pixels[i]>>16,pixels[i]>>8,pixels[i]};fwrite(rgb,1,3,f); }
    int error=fclose(f);free(pixels);return error!=0;
}
