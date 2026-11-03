#include <srvros/desk_widgets.h>
#include <srvros/desk_edit.h>
#include <srvros/desk_list.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

enum { W=73,H=51,STRIDE=79 };
static void scene(struct desk_canvas *c) {
    desk_theme_gradient(c,(struct desk_rect){-9,-7,92,43},0xffffff,0x234567);
    desk_theme_gradient(c,(struct desk_rect){INT32_MIN,0,INT32_MAX,H},0,0xffffff);
    desk_theme_gradient(c,(struct desk_rect){0,INT32_MIN,W,INT32_MAX},0,0xffffff);
    desk_theme_gradient(c,(struct desk_rect){0,-INT32_MAX+47,W,INT32_MAX},0x102030,0x506070);
    desk_theme_gradient(c,(struct desk_rect){0,38,20,1},0x112233,0xaabbcc);
    desk_theme_panel(c,(struct desk_rect){3,5,20,20},&desk_theme_silver,0,0);
    desk_theme_panel(c,(struct desk_rect){27,5,20,20},&desk_theme_silver,1,0);
    desk_theme_panel(c,(struct desk_rect){51,5,20,20},&desk_theme_silver,0,1);
    desk_theme_glass(c,(struct desk_rect){-3,22,42,24},0xcceeef,0x225577,184,8);
    desk_theme_glass(c,(struct desk_rect){42,25,25,21},0xffffff,0x112233,224,99);
    desk_theme_glass(c,(struct desk_rect){INT32_MIN,INT32_MIN,INT32_MAX,INT32_MAX},0,0,255,3);
    desk_theme_panel(c,(struct desk_rect){2,40,1,1},&desk_theme_silver,0,0);
    desk_theme_panel(c,(struct desk_rect){4,40,2,2},&desk_theme_silver,0,0);
}
int main(void) {
    uint32_t full[STRIDE*H+2],tiled[STRIDE*H+2];
    for(unsigned i=0;i<STRIDE*H+2;i++) full[i]=tiled[i]=0x12345678;
    struct desk_canvas a,b;assert(!desk_canvas_init(&a,full+1,W,H,STRIDE));
    assert(!desk_canvas_init(&b,tiled+1,W,H,STRIDE));
    struct desk_rect original=a.clip;scene(&a);assert(!memcmp(&original,&a.clip,sizeof(original)));
    for(int y=0;y<H;y+=7) for(int x=0;x<W;x+=11) {
        desk_clip(&b,(struct desk_rect){x,y,11,7});original=b.clip;scene(&b);
        assert(!memcmp(&original,&b.clip,sizeof(original)));
    }
    assert(!memcmp(full,tiled,sizeof(full)));
    assert(full[0]==0x12345678 && full[STRIDE*H+1]==0x12345678);
    for(int y=0;y<H;y++) for(int x=W;x<STRIDE;x++) assert(full[1+y*STRIDE+x]==0x12345678);
    assert(full[1+5*STRIDE+13]==desk_theme_silver.border);
    assert(full[1+5*STRIDE+61]==desk_theme_silver.accent);
    assert(full[1+15*STRIDE+13]!=full[1+15*STRIDE+37]);
    /* Blend is genuinely destination-dependent and transparent zero is a no-op. */
    uint32_t px[2]={0,0xffffff};struct desk_canvas blend;
    assert(!desk_canvas_init(&blend,px,2,1,2));
    desk_theme_glass(&blend,(struct desk_rect){0,0,2,1},0xffffff,0,128,0);
    assert(px[0]==0x808080 && px[1]==0xffffff && blend.pixel_writes==2);
    desk_theme_glass(&blend,(struct desk_rect){0,0,2,1},0,0,0,0);
    assert(px[0]==0x808080 && blend.pixel_writes==2);
    struct desk_theme theme=desk_theme_silver,prior=theme;
    assert(desk_theme_configure(&theme,3,1,16)<0 && !memcmp(&theme,&prior,sizeof(theme)));
    assert(desk_theme_configure(&theme,0,3,16)<0 && !memcmp(&theme,&prior,sizeof(theme)));
    assert(desk_theme_configure(&theme,0,1,17)<0 && !memcmp(&theme,&prior,sizeof(theme)));
    for(unsigned accent=0;accent<3;accent++) for(unsigned glass=0;glass<3;glass++) {
        assert(!desk_theme_configure(&theme,accent,glass,20));
        assert(theme.body==&desk_font_20 && theme.glass_alpha==(glass==0?255:glass==1?224:184));
        assert(theme.accent!=theme.surface && theme.selected!=theme.field);
    }
    assert(theme.accent==0x16776e && theme.selected==0xc6e7df);
    /* Legacy themes omit the appended fields; actual controls retain their colors. */
    struct desk_theme custom=desk_theme_silver;
    custom.surface=0x17212b;custom.border=0x445566;custom.field=custom.shadow=0;
    assert(desk_theme_field(&custom)==custom.surface);
    assert(desk_theme_shadow(&custom)==custom.border);
    struct desk_list list;desk_list_init(&list);
    desk_list_configure(&list,(struct desk_rect){0,0,40,30},1);
    desk_clip(&a,(struct desk_rect){0,0,W,H});desk_list_draw(&a,&list,&custom);
    assert(full[1+10*STRIDE+10]==custom.surface);
    char text_data[16],history[16];struct desk_text_buffer text;
    assert(!desk_text_buffer_init(&text,text_data,history,sizeof(text_data)));
    struct desk_edit edit;desk_edit_init(&edit,&text,0);
    desk_edit_configure(&edit,(struct desk_rect){0,0,40,28},custom.body,1);
    desk_edit_draw(&a,&edit,&custom);assert(full[1+10*STRIDE+10]==custom.surface);
    custom.field=0x334455;custom.shadow=0x667788;
    desk_edit_draw(&a,&edit,&custom);assert(full[1+10*STRIDE+10]==custom.field);
    assert(desk_theme_shadow(&custom)==0x667788);
    /* Even a directly assigned, oversized clip cannot cause unbounded scanning. */
    a.clip=(struct desk_rect){0,0,INT32_MAX,INT32_MAX};
    original=a.clip;desk_theme_gradient(&a,(struct desk_rect){0,0,10,INT32_MAX},0,0xffffff);
    assert(!memcmp(&original,&a.clip,sizeof(original)));
    return 0;
}
