#include <srvros/desk_log.h>
#include <srvros/desktop.h>
#include <srvros/desk_render.h>
#include <srvros/desk_widgets.h>
#include <srvros/gui.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>

/* Visual feedback must survive a press/release pair drained in one receive
 * batch. It never holds the logical button or delays activation. Start the
 * ten-tick (~100 ms) pulse after the compositor releases the highlighted frame. */
struct button_feedback { uint64_t serial,until; int active; };
static void feedback_commit(struct button_feedback *f,uint64_t serial) {
    if(f->active && !f->serial) f->serial=serial;
}
static void feedback_release(struct button_feedback *f,uint64_t serial,uint64_t now) {
    if(f->active && f->serial==serial && !f->until) f->until=now+10;
}
static int feedback_expire(struct button_feedback *f,uint64_t now) {
    if(!f->active || !f->until || now<f->until) return 0;
    *f=(struct button_feedback){0};return 1;
}
#define STUDY_LINES 48
static void layout(struct desk_scrollbar *bar,struct desk_button *button,int width,int height) {
    desk_scrollbar_configure(bar,(struct desk_rect){width-30,94,18,height-202},
                             STUDY_LINES*24,height-202);
    desk_button_configure(button,(struct desk_rect){width-132,height-38,108,28},1);
}
static struct desk_widget_event pointer_event(const struct desk_packet *p) {
    return (struct desk_widget_event){
        .type=(p->value&DESK_POINTER_CANCEL)?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,
        .x=p->x,.y=p->y,.down=(p->value&DESK_POINTER_LEFT)!=0,
        .pressed=(p->value&DESK_POINTER_PRESS)!=0};
}
static void draw(struct desk_canvas *c,const char *input,int large,
                 const struct desk_scrollbar *bar,const struct desk_button *button,int feedback) {
    const struct desk_theme *theme=&desk_theme_quiet;
    int offset=bar->offset;
    desk_fill(c,(struct desk_rect){0,0,c->width,c->height},theme->surface);
    desk_text(c,theme->heading,24,24,"A little room to think.",theme->text);
    desk_text(c,&desk_font_16,24,58,"Two fingers to scroll. Type below. Drag the title.",0x465348);
    struct desk_rect clip=c->clip;
    struct desk_rect viewport={24,94,c->width-58,c->height-202};
    desk_clip(c,desk_intersect(clip,viewport));
    static const char *lines[]={
        "A quiet space for a few thoughts.",
        "The quick brown fox jumps over the lazy dog.",
        "Café · naïve · £ € — Aa Bb Cc 0123456789",
        "Lift your fingers, then settle them again.",
        "Move the page without moving the pointer.",
        "Small movements deserve a steady response."};
    for (int i=0;i<STUDY_LINES;i++) {
        int y=viewport.y+i*24-offset;
        if (y+24<viewport.y || y>=viewport.y+viewport.h) continue;
        char text[120]; snprintf(text,sizeof(text),"%02d   %s",i+1,lines[i%6]);
        desk_text(c,&desk_font_16,viewport.x,y,text,0x465348);
    }
    c->clip=clip;
    desk_scrollbar_draw(c,bar,theme);
    desk_clip(c,desk_intersect(clip,(struct desk_rect){20,c->height-92,c->width-40,44}));
    desk_fill(c,c->clip,theme->selected);
    desk_text(c,large?&desk_font_20:&desk_font_16,26,c->height-82,input,theme->accent);
    c->clip=clip;
    char status[64]; snprintf(status,sizeof(status),"Scroll %d / %d",offset,desk_scrollbar_limit(bar));
    desk_label_draw(c,(struct desk_rect){24,c->height-30,c->width-164,24},theme->body,status,theme->muted);
    struct desk_button visual=*button;
    if(feedback && visual.enabled) visual.armed=visual.inside=1;
    desk_button_draw(c,&visual,"Text size",theme);
}
int main(int argc,char **argv) {
    if (argc==2 && strcmp(argv[1],"--license")==0) { srv_puts(desk_font_license); return 0; }
    struct desk_packet open=desk_packet_new(DESK_OPEN);
    open.width=600; open.height=320;
    strcpy(open.text,"Type study");
    if (desk_send(0,&open)!=0) { srv_puts("deskclient: no compositor\n"); return 1; }
    uint64_t server=0,surface=0,serial=0,generation=0,pending=0,pending_gen=0,retry_after=0;
    uint64_t last=srv_ticks(),frames=0,input_serial=0;
    uint32_t width=0,height=0,*pixels=desk_pixels_alloc(DESK_MAX_WIDTH,DESK_MAX_HEIGHT);
    if(!pixels) return 1;
    struct desk_packet configure={0};
    char input[160]={0}; size_t length=0;
    int dirty=0,large=0,failed=0,quit=0;
    struct desk_scrollbar scrollbar={0};
    struct desk_button size_button={0};
    struct button_feedback feedback={0};
    while (!quit) {
        for (int n=0;n<32;n++) {
            uint64_t source; struct desk_packet p;
            int r=desk_receive(&source,&p);
            if (r<0) { failed=1; quit=1; break; }
            if (!r) break;
            if (r==2) continue;
            if (!server && p.type==DESK_CONFIGURE) server=source;
            if (!server || server!=source) continue;
            if (p.type==DESK_CLOSE) { quit=1; break; }
            if (p.type==DESK_CONFIGURE && p.generation>configure.generation &&
                p.width>=280 && p.height>=240 && p.width<=DESK_MAX_WIDTH && p.height<=DESK_MAX_HEIGHT) {
                configure=p;
                feedback=(struct button_feedback){0};
                struct desk_widget_event cancel={.type=DESK_WIDGET_CANCEL};
                desk_scrollbar_event(&scrollbar,cancel); desk_button_event(&size_button,cancel);
                last=srv_ticks();
            } else if (p.type==DESK_RELEASE && p.surface==surface && p.serial==pending &&
                       p.generation==pending_gen) {
                feedback_release(&feedback,p.serial,srv_ticks());
                pending=0; last=srv_ticks();
                if(p.value==DESK_RELEASE_RETRY) { dirty=1;retry_after=last+5; }
            }
            else if (p.type==DESK_POINTER && height && p.generation==generation &&
                     configure.generation==generation) {
                struct desk_widget_event event=pointer_event(&p);
                unsigned scroll=desk_scrollbar_event(&scrollbar,event);
                unsigned button=desk_button_event(&size_button,event);
                if(event.type==DESK_WIDGET_CANCEL && feedback.active) {
                    feedback=(struct button_feedback){0};button|=DESK_WIDGET_REDRAW;
                }
                if (button&DESK_WIDGET_ACTIVATED) {
                    feedback=(struct button_feedback){.active=1};
                    large=!large;
                    button|=DESK_WIDGET_REDRAW;
                    desk_log("deskclient: text-size=%d\n",large?20:16);
                }
                if ((scroll|button)&DESK_WIDGET_REDRAW) {
                    if (!dirty && !pending) last=srv_ticks();
                    dirty=1;
                }
                if (scroll&DESK_WIDGET_CHANGED)
                    desk_log("deskclient: scroll=%d\n",scrollbar.offset);
            }
            else if (p.type==DESK_SCROLL && height) {
                if (!dirty && !pending) last=srv_ticks();
                desk_scrollbar_scroll(&scrollbar,p.y); dirty=1;
                desk_log("deskclient: scroll=%d\n",scrollbar.offset);
            }
            else if (p.type==DESK_TEXT) {
                if (p.input_serial>input_serial) { input_serial=p.input_serial; dirty=1; }
                if (!dirty && !pending) last=srv_ticks();
                if (p.value==12) { large=!large; dirty=1; }
                else if ((p.value==8 || p.value==127) && length) { input[--length]=0; dirty=1; }
                else if (p.value>=32 && p.value<=126 && length+1<sizeof(input)) {
                    input[length++]=(char)p.value; input[length]=0; dirty=1;
                }
                desk_log("deskclient: text=%s\n",input);
            }
        }
        if (quit) break;
        if(feedback_expire(&feedback,srv_ticks())) {
            if(!dirty && !pending) last=srv_ticks();
            dirty=1;
        }
        if (!pending && configure.generation>generation) {
            uint64_t next_surface=0;
            if (gui_surface_create(configure.width,configure.height,DESK_SURFACE_STREAM,&next_surface)!=0) {
                failed=1; break;
            }
            if (surface) gui_surface_destroy(surface);
            surface=next_surface; width=configure.width; height=configure.height;
            generation=configure.generation; retry_after=0; layout(&scrollbar,&size_button,width,height); dirty=1;
        }
        if (dirty && !pending && surface && (uint64_t)srv_ticks()>=retry_after) {
            struct desk_canvas c;
            desk_canvas_init(&c,pixels,width,height,width);
            draw(&c,input,large,&scrollbar,&size_button,feedback.active);
            struct desk_packet p=desk_packet_new(DESK_COMMIT);
            p.input_serial=input_serial; p.surface=surface; p.serial=serial+1; p.generation=generation; p.width=width; p.height=height;
            if (gui_surface_blit(surface,0,0,width,height,pixels,width)!=0) { failed=1; break; }
            if (desk_send(server,&p)==0) {
                pending=++serial; feedback_commit(&feedback,pending); pending_gen=generation; dirty=0; last=srv_ticks(); frames++;
            }
        }
        if ((!server || pending || dirty) && (uint64_t)srv_ticks()-last>500) { failed=1; break; }
        if (server && !desk_process_alive(server)) break;
        srv_sleep_ticks(1);
    }
    if (server) { struct desk_packet p=desk_packet_new(DESK_CLOSE); desk_send(server,&p); }
    if (surface && !pending) gui_surface_destroy(surface);
    desk_pixels_free(pixels,DESK_MAX_WIDTH,DESK_MAX_HEIGHT);
    desk_log("deskclient: %s frames=%llu input=%s\n",failed?"FAIL":"exited",(unsigned long long)frames,input);
    return failed?1:0;
}
